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
            auto& entityClients = iterator->second;
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

            auto& entityClients = iterator->second;
            entityClients.erase(
                AZStd::remove(entityClients.begin(), entityClients.end(), &client),
                entityClients.end());
            if (entityClients.empty())
            {
                disconnect(entityId);
                clients.erase(iterator);
            }
        }

        template<typename Map, typename Visitor>
        void VisitClients(
            const Map& clients,
            const AZ::EntityId entityId,
            Visitor&& visitor)
        {
            const auto iterator = clients.find(entityId);
            if (iterator == clients.end())
            {
                return;
            }

            for (auto* client : iterator->second)
            {
                visitor(*client);
            }
        }
    } // namespace

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

    void ComponentDependencyManager::OnBodyCreated(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle)
    {
        const AZ::EntityId* entityId = BodyNotificationBus::GetCurrentBusId();
        if (!entityId)
        {
            return;
        }
        VisitClients(
            m_bodyClients,
            *entityId,
            [&](IBodyDependencyClient& client)
            {
                client.OnBodyDependencyCreated(worldHandle, bodyHandle);
            });
    }

    void ComponentDependencyManager::OnBodyDestroying(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle)
    {
        const AZ::EntityId* entityId = BodyNotificationBus::GetCurrentBusId();
        if (!entityId)
        {
            return;
        }
        VisitClients(
            m_bodyClients,
            *entityId,
            [&](IBodyDependencyClient& client)
            {
                client.OnBodyDependencyDestroying(worldHandle, bodyHandle);
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
        VisitClients(
            m_constraintClients,
            *entityId,
            [&](IConstraintDependencyClient& client)
            {
                client.OnConstraintDependencyCreated(worldHandle, constraintHandle);
            });
    }

    void ComponentDependencyManager::OnConstraintDestroying(
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle)
    {
        const AZ::EntityId* entityId = ConstraintNotificationBus::GetCurrentBusId();
        if (!entityId)
        {
            return;
        }
        VisitClients(
            m_constraintClients,
            *entityId,
            [&](IConstraintDependencyClient& client)
            {
                client.OnConstraintDependencyDestroying(worldHandle, constraintHandle);
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
        VisitClients(
            m_pathClients,
            *entityId,
            [&](IConstraintDependencyClient& client)
            {
                client.OnPathDependencyCreated(pathHandle);
            });
    }

    void ComponentDependencyManager::OnPathDestroying(
        const PathHandle pathHandle)
    {
        const AZ::EntityId* entityId = PathNotificationBus::GetCurrentBusId();
        if (!entityId)
        {
            return;
        }
        VisitClients(
            m_pathClients,
            *entityId,
            [&](IConstraintDependencyClient& client)
            {
                client.OnPathDependencyDestroying(pathHandle);
            });
    }
} // namespace Jolt
