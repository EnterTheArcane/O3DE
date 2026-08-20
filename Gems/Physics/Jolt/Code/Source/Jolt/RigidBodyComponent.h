/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <Jolt/RigidBodyBus.h>
#include <Jolt/RigidBodyConfiguration.h>
#include <Jolt/TypeIds.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TransformBus.h>

namespace Jolt
{
    class ColliderComponent;
    class ISystem;

    class JOLT_API RigidBodyComponent final
        : public AZ::Component
        , public RigidBodyRequestBus::Handler
        , public BodyRequestBus::Handler
        , private BodyNotificationBus::Handler
        , private AZ::TransformNotificationBus::Handler
    {
    public:
        AZ_COMPONENT(RigidBodyComponent, RigidBodyComponentTypeId);

        RigidBodyComponent() = default;
        explicit RigidBodyComponent(RigidBodyConfiguration configuration);
        ~RigidBodyComponent() override = default;

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

        [[nodiscard]]
        BodyRuntimeConfiguration GetRuntimeConfiguration() const override;

        bool GetInverseInertia(AZ::Matrix3x3& inverseInertia) const override;

        bool GetInverseMass(float& inverseMass) const override;

        bool GetPointVelocity(
            const WorldPosition& point,
            AZ::Vector3& velocity) const override;

        [[nodiscard]]
        MotionType GetMotionType() const override;

        [[nodiscard]]
        ObjectLayer GetObjectLayer() const override;

        [[nodiscard]]
        CollisionGroupConfiguration GetCollisionGroup() const override;

        [[nodiscard]]
        ShapeHandle GetShapeHandle() const override;

        bool GetAccumulatedForceAndTorque(
            AZ::Vector3& force,
            AZ::Vector3& torque) const override;

        bool ResetAccumulatedForce() override;

        bool ResetAccumulatedTorque() override;

        bool ResetMotion() override;

        bool GetBounds(BroadPhaseAabb& bounds) const override;

        bool GetSubmergedVolume(
            const WorldPosition& surfacePosition,
            const AZ::Vector3& surfaceNormal,
            SubmergedVolumeResult& result) const override;

        bool GetSurfaceNormal(
            SubShapeId subShapeId,
            const WorldPosition& surfacePosition,
            AZ::Vector3& normal) const override;

        bool GetMaterial(
            SubShapeId subShapeId,
            MaterialHandle& materialHandle) const override;

        [[nodiscard]]
        WorldPosition GetPosition() const override;

        [[nodiscard]]
        AZ::Quaternion GetRotation() const override;

        bool GetVelocities(
            AZ::Vector3& linearVelocity,
            AZ::Vector3& angularVelocity) const override;

        [[nodiscard]]
        AZ::Vector3 GetLinearVelocity() const override;

        [[nodiscard]]
        AZ::Vector3 GetAngularVelocity() const override;

        bool ActivateBody() override;

        bool DeactivateBody() override;

        bool ResetSleepTimer() override;

        bool InvalidateContactCache() override;

        bool AddForce(
            const AZ::Vector3& force,
            bool activate) override;

        bool AddForceAtPosition(
            const AZ::Vector3& force,
            const WorldPosition& position,
            bool activate) override;

        bool AddForceAndTorque(
            const AZ::Vector3& force,
            const AZ::Vector3& torque,
            bool activate) override;

        bool AddTorque(
            const AZ::Vector3& torque,
            bool activate) override;

        bool AddImpulse(const AZ::Vector3& impulse) override;

        bool AddImpulseAtPosition(
            const AZ::Vector3& impulse,
            const WorldPosition& position) override;

        bool AddAngularImpulse(const AZ::Vector3& impulse) override;

        bool ApplyBuoyancyImpulse(const BuoyancyConfiguration& configuration) override;

        [[nodiscard]]
        float GetFriction() const override;

        bool SetFriction(float friction) override;

        [[nodiscard]]
        float GetRestitution() const override;

        bool SetRestitution(float restitution) override;

        [[nodiscard]]
        float GetGravityFactor() const override;

        bool SetGravityFactor(float gravityFactor) override;

        [[nodiscard]]
        float GetMaximumLinearVelocity() const override;

        bool SetMaximumLinearVelocity(float maximumLinearVelocity) override;

        [[nodiscard]]
        float GetMaximumAngularVelocity() const override;

        bool SetMaximumAngularVelocity(float maximumAngularVelocity) override;

        [[nodiscard]]
        MotionQuality GetMotionQuality() const override;

        bool SetMotionQuality(MotionQuality motionQuality) override;

        [[nodiscard]]
        bool IsManifoldReductionEnabled() const override;

        bool SetManifoldReductionEnabled(bool enabled) override;

        [[nodiscard]]
        bool IsSensor() const override;

        bool SetSensor(bool sensor) override;

        [[nodiscard]]
        float GetLinearDamping() const override;

        bool SetLinearDamping(float linearDamping) override;

        [[nodiscard]]
        float GetAngularDamping() const override;

        bool SetAngularDamping(float angularDamping) override;

        [[nodiscard]]
        bool IsSleepingAllowed() const override;

        bool SetSleepingAllowed(bool sleepingAllowed) override;

        [[nodiscard]]
        bool IsGyroscopicForceEnabled() const override;

        bool SetGyroscopicForceEnabled(bool enabled) override;

        [[nodiscard]]
        bool IsKinematicVsNonDynamicCollisionEnabled() const override;

        bool SetKinematicVsNonDynamicCollisionEnabled(bool enabled) override;

        [[nodiscard]]
        bool IsEnhancedInternalEdgeRemovalEnabled() const override;

        bool SetEnhancedInternalEdgeRemovalEnabled(bool enabled) override;

        bool GetSolverStepCounts(
            AZ::u8& velocityStepCount,
            AZ::u8& positionStepCount) const override;

        bool SetSolverStepCounts(
            AZ::u8 velocityStepCount,
            AZ::u8 positionStepCount) override;

        bool SetPosition(
            const WorldPosition& position,
            bool activate) override;

        bool SetRotation(
            const AZ::Quaternion& rotation,
            bool activate) override;

        bool SetTransform(
            const WorldTransform& transform,
            bool activate) override;

        bool SetTransformWhenChanged(
            const WorldTransform& transform,
            bool activate) override;

        bool SetVelocities(
            const AZ::Vector3& linearVelocity,
            const AZ::Vector3& angularVelocity) override;

        bool SetLinearVelocity(const AZ::Vector3& linearVelocity) override;

        bool SetAngularVelocity(const AZ::Vector3& angularVelocity) override;

        bool AddVelocities(
            const AZ::Vector3& linearVelocity,
            const AZ::Vector3& angularVelocity) override;

        bool AddLinearVelocity(const AZ::Vector3& linearVelocity) override;

        bool MoveKinematically(
            const WorldTransform& target,
            float duration) override;

        bool UpdateRuntimeConfiguration(
            const BodyRuntimeConfiguration& configuration,
            bool activate) override;

    private:
        void Activate() override;

        void Deactivate() override;

        void OnBodyMoved(const BodyMoveEvent& event) override;

        void OnTransformChanged(
            const AZ::Transform& local,
            const AZ::Transform& world) override;

        RigidBodyConfiguration m_configuration;

        ISystem* m_system = nullptr;
        ColliderComponent* m_collider = nullptr;
        WorldHandle m_worldHandle;
        BodyHandle m_bodyHandle;
        float m_uniformScale = 1.0f;
        bool m_syncingTransform = false;
    };
} // namespace Jolt
