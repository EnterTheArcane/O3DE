/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Box3D/Configuration.h>
#include <Box3D/RigidBodyBus.h>
#include <Box3D/TypeIds.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Name/Name.h>

namespace Box3D
{
    class ColliderComponent;

    class BOX3D_API StaticRigidBodyComponent final
        : public AZ::Component
        , public RigidBodyRequestBus::Handler
        , private AZ::TransformNotificationBus::Handler
    {
    public:
        AZ_COMPONENT(StaticRigidBodyComponent, StaticRigidBodyComponentTypeId);

        StaticRigidBodyComponent() = default;
        explicit StaticRigidBodyComponent(AZ::Name worldName);
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
        BodyState GetState() const override;

        [[nodiscard]]
        AZ::Name GetName() const override;

        bool SetName(AZ::Name name) override;

        [[nodiscard]]
        BodyProperties GetProperties() const override;

        bool SetProperties(const BodyProperties& properties) override;

        [[nodiscard]]
        AZ::Aabb GetAabb() const override;

        [[nodiscard]]
        ClosestPoint GetClosestPoint(const AZ::Vector3& target) const override;

        [[nodiscard]]
        MassProperties GetMassProperties() const override;

        bool SetMassProperties(const MassProperties& properties) override;

        bool RecomputeMassFromShapes() override;

        bool SetTransform(const AZ::Transform& transform) override;

        bool SetLinearVelocity(const AZ::Vector3& velocity) override;

        bool SetAngularVelocity(const AZ::Vector3& velocity) override;

        [[nodiscard]]
        AZ::Vector3 GetLinearVelocityAtWorldPoint(const AZ::Vector3& worldPoint) const override;

        bool SetKinematicTarget(
            const AZ::Transform& transform,
            float fixedTimeStep) override;

        bool ApplyLinearImpulse(const AZ::Vector3& impulse) override;

        bool ApplyLinearImpulseAtWorldPoint(
            const AZ::Vector3& impulse,
            const AZ::Vector3& worldPoint) override;

        bool ApplyAngularImpulse(const AZ::Vector3& impulse) override;

        bool ApplyForce(
            const AZ::Vector3& force,
            bool wake) override;

        bool ApplyForceAtWorldPoint(
            const AZ::Vector3& force,
            const AZ::Vector3& worldPoint,
            bool wake) override;

        bool ApplyTorque(
            const AZ::Vector3& torque,
            bool wake) override;

        bool SetAwake(bool awake) override;

        bool SetHitEventsEnabled(bool enabled) override;

    private:
        void Activate() override;

        void Deactivate() override;

        void OnTransformChanged(
            const AZ::Transform& local,
            const AZ::Transform& world) override;

        AZ::Name m_worldName;

        ISystem* m_system = nullptr;
        ColliderComponent* m_collider = nullptr;

        WorldHandle m_worldHandle;
        BodyHandle m_bodyHandle;

        float m_uniformScale = 1.0f;
    };
} // namespace Box3D
