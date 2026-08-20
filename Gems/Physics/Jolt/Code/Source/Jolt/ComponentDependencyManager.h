/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/BodyBus.h>
#include <Jolt/ConstraintBus.h>
#include <Jolt/PathBus.h>

#include <AzCore/Interface/Interface.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/vector.h>

namespace Jolt
{
    class IBodyDependencyClient
    {
    public:
        AZ_RTTI(IBodyDependencyClient, "{53FD85B2-396C-4E5B-97DF-3E2698FCB3CA}");

        virtual ~IBodyDependencyClient() = default;

        virtual void OnBodyDependencyCreated(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) = 0;

        virtual bool OnBodyDependencyDestroying(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) = 0;
    };

    class IConstraintDependencyClient
        : public IBodyDependencyClient
    {
    public:
        AZ_RTTI(
            IConstraintDependencyClient,
            "{05D08FD6-A9F9-49E1-91AF-C6CD759582E2}",
            IBodyDependencyClient);

        ~IConstraintDependencyClient() override = default;

        virtual void OnConstraintDependencyCreated(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle) = 0;

        virtual bool OnConstraintDependencyDestroying(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle) = 0;

        virtual void OnPathDependencyCreated(PathHandle pathHandle) = 0;

        virtual bool OnPathDependencyDestroying(PathHandle pathHandle) = 0;
    };

    class IComponentDependencyManager
    {
    public:
        AZ_RTTI(IComponentDependencyManager, "{6A37A272-92DF-4AC0-BFB6-79D6756D1D32}");

        virtual ~IComponentDependencyManager() = default;

        virtual void RegisterBody(
            AZ::EntityId entityId,
            IBodyDependencyClient& client) = 0;

        virtual void UnregisterBody(
            AZ::EntityId entityId,
            IBodyDependencyClient& client) = 0;

        virtual void RegisterConstraint(
            AZ::EntityId entityId,
            IConstraintDependencyClient& client) = 0;

        virtual void UnregisterConstraint(
            AZ::EntityId entityId,
            IConstraintDependencyClient& client) = 0;

        virtual void RegisterPath(
            AZ::EntityId entityId,
            IConstraintDependencyClient& client) = 0;

        virtual void UnregisterPath(
            AZ::EntityId entityId,
            IConstraintDependencyClient& client) = 0;

        virtual bool PrepareBodyDestruction(
            AZ::EntityId entityId,
            WorldHandle worldHandle,
            BodyHandle bodyHandle) = 0;

        virtual bool PrepareConstraintDestruction(
            AZ::EntityId entityId,
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle) = 0;

        virtual bool PreparePathDestruction(
            AZ::EntityId entityId,
            PathHandle pathHandle) = 0;
    };

    class JOLT_API ComponentDependencyManager final
        : public IComponentDependencyManager
        , private BodyNotificationBus::MultiHandler
        , private ConstraintNotificationBus::MultiHandler
        , private PathNotificationBus::MultiHandler
    {
    public:
        AZ_RTTI(ComponentDependencyManager, "{72A85D65-5A05-4B56-BB91-A8643D649C33}", IComponentDependencyManager);

        ComponentDependencyManager();
        ~ComponentDependencyManager() override;

        AZ_DISABLE_COPY_MOVE(ComponentDependencyManager);

        void RegisterBody(
            AZ::EntityId entityId,
            IBodyDependencyClient& client) override;

        void UnregisterBody(
            AZ::EntityId entityId,
            IBodyDependencyClient& client) override;

        void RegisterConstraint(
            AZ::EntityId entityId,
            IConstraintDependencyClient& client) override;

        void UnregisterConstraint(
            AZ::EntityId entityId,
            IConstraintDependencyClient& client) override;

        void RegisterPath(
            AZ::EntityId entityId,
            IConstraintDependencyClient& client) override;

        void UnregisterPath(
            AZ::EntityId entityId,
            IConstraintDependencyClient& client) override;

        bool PrepareBodyDestruction(
            AZ::EntityId entityId,
            WorldHandle worldHandle,
            BodyHandle bodyHandle) override;

        bool PrepareConstraintDestruction(
            AZ::EntityId entityId,
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle) override;

        bool PreparePathDestruction(
            AZ::EntityId entityId,
            PathHandle pathHandle) override;

    private:
        template<class Client>
        struct ClientList final
        {
            AZStd::vector<Client*> m_clients;
            AZ::u32 m_dispatchDepth = 0;
        };

        using BodyClientList = ClientList<IBodyDependencyClient>;
        using BodyClientMap = AZStd::unordered_map<AZ::EntityId, BodyClientList>;
        using ConstraintClientList = ClientList<IConstraintDependencyClient>;
        using ConstraintClientMap = AZStd::unordered_map<AZ::EntityId, ConstraintClientList>;

        void OnBodyCreated(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) override;

        void OnConstraintCreated(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle) override;

        void OnPathCreated(PathHandle pathHandle) override;

        BodyClientMap m_bodyClients;
        ConstraintClientMap m_constraintClients;
        ConstraintClientMap m_pathClients;
    };
} // namespace Jolt
