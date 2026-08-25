/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/ComponentDependencyManager.h>

#include <AzCore/std/algorithm.h>

namespace Jolt
{
    namespace
    {
        template<typename Map, typename Client, typename Connect>
        void RegisterClient(
            Map& clients,
            const AZ::EntityId entityId,
            Client& client,
            Connect&& connect)
        {
            if (!entityId.IsValid())
            {
                return;
            }

            auto [iterator, inserted] = clients.try_emplace(entityId);
            if (inserted)
            {
                connect(entityId);
            }

            auto& entityClients = iterator->second.m_clients;
            if (AZStd::find(entityClients.begin(), entityClients.end(), &client) == entityClients.end())
            {
                entityClients.push_back(&client);
            }
        }

        template<typename Map, typename Client, typename Disconnect>
        void UnregisterClient(
            Map& clients,
            const AZ::EntityId entityId,
            Client& client,
            Disconnect&& disconnect)
        {
            auto iterator = clients.find(entityId);
            if (iterator == clients.end())
            {
                return;
            }

            auto& clientList = iterator->second;
            if (clientList.m_dispatchDepth > 0)
            {
                for (Client*& registeredClient : clientList.m_clients)
                {
                    if (registeredClient == &client)
                    {
                        registeredClient = nullptr;
                    }
                }
                return;
            }

            clientList.m_clients.erase(
                AZStd::remove(clientList.m_clients.begin(), clientList.m_clients.end(), &client),
                clientList.m_clients.end());
            if (clientList.m_clients.empty())
            {
                disconnect(entityId);
                clients.erase(iterator);
            }
        }

        template<typename Map, typename Visitor, typename Disconnect>
        bool VisitClients(
            Map& clients,
            const AZ::EntityId entityId,
            Visitor&& visitor,
            Disconnect&& disconnect)
        {
            auto iterator = clients.find(entityId);
            if (iterator == clients.end())
            {
                return true;
            }

            ++iterator->second.m_dispatchDepth;
            const size_t clientCount = iterator->second.m_clients.size();
            bool succeeded = true;
            for (size_t clientIndex = 0; clientIndex < clientCount; ++clientIndex)
            {
                iterator = clients.find(entityId);
                AZ_Assert(iterator != clients.end(), "A dispatching dependency list cannot be removed.");
                if (iterator == clients.end() || clientIndex >= iterator->second.m_clients.size())
                {
                    succeeded = false;
                    break;
                }

                auto* client = iterator->second.m_clients[clientIndex];
                if (client && !visitor(*client))
                {
                    succeeded = false;
                }
            }

            iterator = clients.find(entityId);
            AZ_Assert(iterator != clients.end(), "A dispatching dependency list cannot be removed.");
            if (iterator == clients.end())
            {
                return false;
            }

            auto& clientList = iterator->second;
            AZ_Assert(clientList.m_dispatchDepth > 0, "The dependency dispatch depth underflowed.");
            --clientList.m_dispatchDepth;
            if (clientList.m_dispatchDepth == 0)
            {
                clientList.m_clients.erase(
                    AZStd::remove(clientList.m_clients.begin(), clientList.m_clients.end(), nullptr),
                    clientList.m_clients.end());
                if (clientList.m_clients.empty())
                {
                    disconnect(entityId);
                    clients.erase(iterator);
                }
            }
            return succeeded;
        }
    } // namespace

    bool ResourceDestructionPlan::AddObserver(const ResourceDestructionObserver observer)
    {
        if (!observer.m_context
            || !observer.m_entityId.IsValid()
            || observer.m_componentId == AZ::InvalidComponentId
            || !observer.m_notify)
        {
            return false;
        }

        const auto existing = AZStd::find_if(
            m_observers.begin(),
            m_observers.end(),
            [observer](const ResourceDestructionObserver& candidate)
            {
                return candidate.m_context == observer.m_context
                    && candidate.m_entityId == observer.m_entityId
                    && candidate.m_componentId == observer.m_componentId;
            });
        if (existing != m_observers.end())
        {
            return existing->m_notify == observer.m_notify;
        }

        m_observers.push_back(observer);
        return true;
    }

    bool ResourceDestructionPlan::AddConstraint(
        const ResourceDestructionObserver observer,
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle)
    {
        if (!worldHandle || !constraintHandle)
        {
            return false;
        }

        const auto existing = AZStd::find_if(
            m_constraints.begin(),
            m_constraints.end(),
            [worldHandle, constraintHandle](const ConstraintDestruction& destruction)
            {
                return destruction.m_worldHandle == worldHandle
                    && destruction.m_constraintHandle == constraintHandle;
            });
        if (existing != m_constraints.end())
        {
            return existing->m_observer.m_context == observer.m_context
                && existing->m_observer.m_entityId == observer.m_entityId
                && existing->m_observer.m_componentId == observer.m_componentId
                && existing->m_observer.m_notify == observer.m_notify
                && AddObserver(observer);
        }
        if (!AddObserver(observer))
        {
            return false;
        }

        m_constraints.push_back({
            .m_observer = observer,
            .m_worldHandle = worldHandle,
            .m_constraintHandle = constraintHandle,
        });
        return true;
    }

    bool ResourceDestructionPlan::AddVehicle(
        const ResourceDestructionObserver observer,
        const WorldHandle worldHandle,
        const VehicleHandle vehicleHandle)
    {
        if (!worldHandle || !vehicleHandle)
        {
            return false;
        }

        const auto existing = AZStd::find_if(
            m_vehicles.begin(),
            m_vehicles.end(),
            [worldHandle, vehicleHandle](const VehicleDestruction& destruction)
            {
                return destruction.m_worldHandle == worldHandle
                    && destruction.m_vehicleHandle == vehicleHandle;
            });
        if (existing != m_vehicles.end())
        {
            return existing->m_observer.m_context == observer.m_context
                && existing->m_observer.m_entityId == observer.m_entityId
                && existing->m_observer.m_componentId == observer.m_componentId
                && existing->m_observer.m_notify == observer.m_notify
                && AddObserver(observer);
        }
        if (!AddObserver(observer))
        {
            return false;
        }

        m_vehicles.push_back({
            .m_observer = observer,
            .m_worldHandle = worldHandle,
            .m_vehicleHandle = vehicleHandle,
        });
        return true;
    }

    AZStd::span<const ConstraintDestruction> ResourceDestructionPlan::GetConstraints() const
    {
        return m_constraints;
    }

    AZStd::span<const VehicleDestruction> ResourceDestructionPlan::GetVehicles() const
    {
        return m_vehicles;
    }

    void ResourceDestructionPlan::NotifyDestroying() const
    {
        for (const ResourceDestructionObserver& observer : m_observers)
        {
            observer.m_notify(
                observer.m_context,
                observer.m_entityId,
                observer.m_componentId,
                ResourceDestructionPhase::Destroying);
        }
    }

    void ResourceDestructionPlan::NotifyDestroyed() const
    {
        for (const ResourceDestructionObserver& observer : m_observers)
        {
            observer.m_notify(
                observer.m_context,
                observer.m_entityId,
                observer.m_componentId,
                ResourceDestructionPhase::Destroyed);
        }
    }

    ComponentDependencyManager::ComponentDependencyManager()
    {
        AZ::Interface<IComponentDependencyManager>::Register(this);
    }

    ComponentDependencyManager::~ComponentDependencyManager()
    {
        PathNotificationBus::MultiHandler::BusDisconnect();
        ConstraintNotificationBus::MultiHandler::BusDisconnect();
        BodyNotificationBus::MultiHandler::BusDisconnect();
        AZ::Interface<IComponentDependencyManager>::Unregister(this);
    }

    void ComponentDependencyManager::RegisterBody(
        const AZ::EntityId entityId,
        IBodyDependencyClient& client)
    {
        RegisterClient(
            m_bodyClients,
            entityId,
            client,
            [&](const AZ::EntityId id)
            {
                BodyNotificationBus::MultiHandler::BusConnect(id);
            });
    }

    void ComponentDependencyManager::UnregisterBody(
        const AZ::EntityId entityId,
        IBodyDependencyClient& client)
    {
        UnregisterClient(
            m_bodyClients,
            entityId,
            client,
            [&](const AZ::EntityId id)
            {
                BodyNotificationBus::MultiHandler::BusDisconnect(id);
            });
    }

    void ComponentDependencyManager::RegisterConstraint(
        const AZ::EntityId entityId,
        IConstraintDependencyClient& client)
    {
        RegisterClient(
            m_constraintClients,
            entityId,
            client,
            [&](const AZ::EntityId id)
            {
                ConstraintNotificationBus::MultiHandler::BusConnect(id);
            });
    }

    void ComponentDependencyManager::UnregisterConstraint(
        const AZ::EntityId entityId,
        IConstraintDependencyClient& client)
    {
        UnregisterClient(
            m_constraintClients,
            entityId,
            client,
            [&](const AZ::EntityId id)
            {
                ConstraintNotificationBus::MultiHandler::BusDisconnect(id);
            });
    }

    void ComponentDependencyManager::RegisterPath(
        const AZ::EntityId entityId,
        IConstraintDependencyClient& client)
    {
        RegisterClient(
            m_pathClients,
            entityId,
            client,
            [&](const AZ::EntityId id)
            {
                PathNotificationBus::MultiHandler::BusConnect(id);
            });
    }

    void ComponentDependencyManager::UnregisterPath(
        const AZ::EntityId entityId,
        IConstraintDependencyClient& client)
    {
        UnregisterClient(
            m_pathClients,
            entityId,
            client,
            [&](const AZ::EntityId id)
            {
                PathNotificationBus::MultiHandler::BusDisconnect(id);
            });
    }

    bool ComponentDependencyManager::PrepareBodyDestruction(
        const AZ::EntityId entityId,
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        ResourceDestructionPlan& plan)
    {
        return VisitClients(
            m_bodyClients,
            entityId,
            [&](IBodyDependencyClient& client)
            {
                return client.PrepareBodyDependencyDestruction(worldHandle, bodyHandle, plan);
            },
            [&](const AZ::EntityId id)
            {
                BodyNotificationBus::MultiHandler::BusDisconnect(id);
            });
    }

    bool ComponentDependencyManager::PrepareConstraintDestruction(
        const AZ::EntityId entityId,
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle,
        ResourceDestructionPlan& plan)
    {
        return VisitClients(
            m_constraintClients,
            entityId,
            [&](IConstraintDependencyClient& client)
            {
                return client.PrepareConstraintDependencyDestruction(worldHandle, constraintHandle, plan);
            },
            [&](const AZ::EntityId id)
            {
                ConstraintNotificationBus::MultiHandler::BusDisconnect(id);
            });
    }

    bool ComponentDependencyManager::PreparePathDestruction(
        const AZ::EntityId entityId,
        const PathHandle pathHandle,
        ResourceDestructionPlan& plan)
    {
        return VisitClients(
            m_pathClients,
            entityId,
            [&](IConstraintDependencyClient& client)
            {
                return client.PreparePathDependencyDestruction(pathHandle, plan);
            },
            [&](const AZ::EntityId id)
            {
                PathNotificationBus::MultiHandler::BusDisconnect(id);
            });
    }

    void ComponentDependencyManager::OnBodyCreated(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle)
    {
        const AZ::EntityId* entityId = BodyNotificationBus::GetCurrentBusId();
        if (!entityId)
        {
            return;
        }
        [[maybe_unused]] const bool notified = VisitClients(
            m_bodyClients,
            *entityId,
            [&](IBodyDependencyClient& client)
            {
                client.OnBodyDependencyCreated(worldHandle, bodyHandle);
                return true;
            },
            [&](const AZ::EntityId id)
            {
                BodyNotificationBus::MultiHandler::BusDisconnect(id);
            });
    }

    void ComponentDependencyManager::OnConstraintCreated(
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle)
    {
        const AZ::EntityId* entityId = ConstraintNotificationBus::GetCurrentBusId();
        if (!entityId)
        {
            return;
        }
        [[maybe_unused]] const bool notified = VisitClients(
            m_constraintClients,
            *entityId,
            [&](IConstraintDependencyClient& client)
            {
                client.OnConstraintDependencyCreated(worldHandle, constraintHandle);
                return true;
            },
            [&](const AZ::EntityId id)
            {
                ConstraintNotificationBus::MultiHandler::BusDisconnect(id);
            });
    }

    void ComponentDependencyManager::OnPathCreated(
        const PathHandle pathHandle)
    {
        const AZ::EntityId* entityId = PathNotificationBus::GetCurrentBusId();
        if (!entityId)
        {
            return;
        }
        [[maybe_unused]] const bool notified = VisitClients(
            m_pathClients,
            *entityId,
            [&](IConstraintDependencyClient& client)
            {
                client.OnPathDependencyCreated(pathHandle);
                return true;
            },
            [&](const AZ::EntityId id)
            {
                PathNotificationBus::MultiHandler::BusDisconnect(id);
            });
    }
} // namespace Jolt
