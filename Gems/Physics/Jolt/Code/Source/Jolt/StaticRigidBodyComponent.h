/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <Jolt/BodyBus.h>
#include <Jolt/RigidBodyConfiguration.h>
#include <Jolt/StaticRigidBodyBus.h>
#include <Jolt/TypeIds.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TransformBus.h>

namespace Jolt
{
    enum class ResourceDestructionPhase : AZ::u8;

    class ColliderComponent;
    class RuntimeImplementation;

    class JOLT_API StaticRigidBodyComponent final
        : public AZ::Component
        , public StaticRigidBodyRequestBus::Handler
        , public BodyRequestBus::Handler
        , private AZ::TransformNotificationBus::Handler
    {
    public:
        AZ_COMPONENT(StaticRigidBodyComponent, StaticRigidBodyComponentTypeId);

        StaticRigidBodyComponent() = default;
        explicit StaticRigidBodyComponent(StaticRigidBodyConfiguration configuration);
        ~StaticRigidBodyComponent() override = default;

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        bool EnableSimulation() override;

        bool DisableSimulation() override;

        [[nodiscard]]
        bool IsSimulationEnabled() const override;

        [[nodiscard]]
        WorldHandle GetWorldHandle() const override;

        [[nodiscard]]
        BodyHandle GetBodyHandle() const override;

        [[nodiscard]]
        AZ::u64 GetUserData() const override;

        bool SetUserData(AZ::u64 userData) override;

        [[nodiscard]]
        WorldTransform GetCenterOfMassTransform() const override;

        [[nodiscard]]
        BodyState GetState() const override;

    private:
        void Activate() override;

        void Deactivate() override;

        bool DestroySimulation(bool mandatory);

        static void NotifyResourceDestruction(
            void* context,
            AZ::EntityId entityId,
            AZ::ComponentId componentId,
            ResourceDestructionPhase phase);

        void OnTransformChanged(
            const AZ::Transform& local,
            const AZ::Transform& world) override;

        StaticRigidBodyConfiguration m_configuration;

        RuntimeImplementation* m_system = nullptr;
        ColliderComponent* m_collider = nullptr;
        WorldHandle m_worldHandle;
        BodyHandle m_bodyHandle;
        float m_uniformScale = 1.0f;
    };
} // namespace Jolt
