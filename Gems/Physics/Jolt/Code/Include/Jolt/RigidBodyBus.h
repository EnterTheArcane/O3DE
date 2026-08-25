/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/BodyBus.h>
#include <Jolt/BodyConfiguration.h>
#include <Jolt/Shape.h>

#include <AzCore/Component/ComponentBus.h>
#include <AzCore/Math/Matrix3x3.h>
#include <AzCore/Math/Quaternion.h>
#include <AzCore/Math/Vector3.h>

namespace Jolt
{
    class IRigidBodyRequests
        : public AZ::ComponentBus
    {
    public:
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;

        virtual bool EnableSimulation() = 0;

        virtual bool DisableSimulation() = 0;

        [[nodiscard]]
        virtual bool IsSimulationEnabled() const = 0;

        [[nodiscard]]
        virtual WorldHandle GetWorldHandle() const = 0;

        [[nodiscard]]
        virtual BodyHandle GetBodyHandle() const = 0;

        [[nodiscard]]
        virtual WorldTransform GetCenterOfMassTransform() const = 0;

        [[nodiscard]]
        virtual BodyState GetState() const = 0;

        [[nodiscard]]
        virtual BodyRuntimeConfiguration GetRuntimeConfiguration() const = 0;

        virtual bool GetInverseInertia(AZ::Matrix3x3& inverseInertia) const = 0;

        virtual bool GetInverseMass(float& inverseMass) const = 0;

        virtual bool GetPointVelocity(
            const WorldPosition& point,
            AZ::Vector3& velocity) const = 0;

        [[nodiscard]]
        virtual MotionType GetMotionType() const = 0;

        [[nodiscard]]
        virtual ObjectLayer GetObjectLayer() const = 0;

        [[nodiscard]]
        virtual CollisionGroupConfiguration GetCollisionGroup() const = 0;

        [[nodiscard]]
        virtual ShapeHandle GetShapeHandle() const = 0;

        virtual bool GetAccumulatedForceAndTorque(
            AZ::Vector3& force,
            AZ::Vector3& torque) const = 0;

        virtual bool ResetAccumulatedForce() = 0;

        virtual bool ResetAccumulatedTorque() = 0;

        virtual bool ResetMotion() = 0;

        virtual bool GetBounds(BroadPhaseAabb& bounds) const = 0;

        virtual bool GetSubmergedVolume(
            const WorldPosition& surfacePosition,
            const AZ::Vector3& surfaceNormal,
            SubmergedVolumeResult& result) const = 0;

        virtual bool GetSurfaceNormal(
            SubShapeId subShapeId,
            const WorldPosition& surfacePosition,
            AZ::Vector3& normal) const = 0;

        virtual bool GetMaterial(
            SubShapeId subShapeId,
            MaterialHandle& materialHandle) const = 0;

        [[nodiscard]]
        virtual WorldPosition GetPosition() const = 0;

        [[nodiscard]]
        virtual AZ::Quaternion GetRotation() const = 0;

        virtual bool GetVelocities(
            AZ::Vector3& linearVelocity,
            AZ::Vector3& angularVelocity) const = 0;

        [[nodiscard]]
        virtual AZ::Vector3 GetLinearVelocity() const = 0;

        [[nodiscard]]
        virtual AZ::Vector3 GetAngularVelocity() const = 0;

        virtual bool ActivateBody() = 0;

        virtual bool DeactivateBody() = 0;

        virtual bool ResetSleepTimer() = 0;

        virtual bool InvalidateContactCache() = 0;

        virtual bool AddForce(
            const AZ::Vector3& force,
            bool activate) = 0;

        virtual bool AddForceAtPosition(
            const AZ::Vector3& force,
            const WorldPosition& position,
            bool activate) = 0;

        virtual bool AddForceAndTorque(
            const AZ::Vector3& force,
            const AZ::Vector3& torque,
            bool activate) = 0;

        virtual bool AddTorque(
            const AZ::Vector3& torque,
            bool activate) = 0;

        virtual bool AddImpulse(const AZ::Vector3& impulse) = 0;

        virtual bool AddImpulseAtPosition(
            const AZ::Vector3& impulse,
            const WorldPosition& position) = 0;

        virtual bool AddAngularImpulse(const AZ::Vector3& impulse) = 0;

        virtual bool ApplyBuoyancyImpulse(const BuoyancyConfiguration& configuration) = 0;

        [[nodiscard]]
        virtual float GetFriction() const = 0;

        virtual bool SetFriction(float friction) = 0;

        [[nodiscard]]
        virtual float GetRestitution() const = 0;

        virtual bool SetRestitution(float restitution) = 0;

        [[nodiscard]]
        virtual float GetGravityFactor() const = 0;

        virtual bool SetGravityFactor(float gravityFactor) = 0;

        [[nodiscard]]
        virtual float GetMaximumLinearVelocity() const = 0;

        virtual bool SetMaximumLinearVelocity(float maximumLinearVelocity) = 0;

        [[nodiscard]]
        virtual float GetMaximumAngularVelocity() const = 0;

        virtual bool SetMaximumAngularVelocity(float maximumAngularVelocity) = 0;

        [[nodiscard]]
        virtual MotionQuality GetMotionQuality() const = 0;

        virtual bool SetMotionQuality(MotionQuality motionQuality) = 0;

        [[nodiscard]]
        virtual bool IsManifoldReductionEnabled() const = 0;

        virtual bool SetManifoldReductionEnabled(bool enabled) = 0;

        [[nodiscard]]
        virtual bool IsSensor() const = 0;

        virtual bool SetSensor(bool sensor) = 0;

        [[nodiscard]]
        virtual float GetLinearDamping() const = 0;

        virtual bool SetLinearDamping(float linearDamping) = 0;

        [[nodiscard]]
        virtual float GetAngularDamping() const = 0;

        virtual bool SetAngularDamping(float angularDamping) = 0;

        [[nodiscard]]
        virtual bool IsSleepingAllowed() const = 0;

        virtual bool SetSleepingAllowed(bool sleepingAllowed) = 0;

        [[nodiscard]]
        virtual bool IsGyroscopicForceEnabled() const = 0;

        virtual bool SetGyroscopicForceEnabled(bool enabled) = 0;

        [[nodiscard]]
        virtual bool IsKinematicVsNonDynamicCollisionEnabled() const = 0;

        virtual bool SetKinematicVsNonDynamicCollisionEnabled(bool enabled) = 0;

        [[nodiscard]]
        virtual bool IsEnhancedInternalEdgeRemovalEnabled() const = 0;

        virtual bool SetEnhancedInternalEdgeRemovalEnabled(bool enabled) = 0;

        virtual bool GetSolverStepCounts(
            AZ::u8& velocityStepCount,
            AZ::u8& positionStepCount) const = 0;

        virtual bool SetSolverStepCounts(
            AZ::u8 velocityStepCount,
            AZ::u8 positionStepCount) = 0;

        virtual bool SetPosition(
            const WorldPosition& position,
            bool activate) = 0;

        virtual bool SetRotation(
            const AZ::Quaternion& rotation,
            bool activate) = 0;

        virtual bool SetTransform(
            const WorldTransform& transform,
            bool activate) = 0;

        virtual bool SetTransformWhenChanged(
            const WorldTransform& transform,
            bool activate) = 0;

        virtual bool SetVelocities(
            const AZ::Vector3& linearVelocity,
            const AZ::Vector3& angularVelocity) = 0;

        virtual bool SetLinearVelocity(const AZ::Vector3& linearVelocity) = 0;

        virtual bool SetAngularVelocity(const AZ::Vector3& angularVelocity) = 0;

        virtual bool AddVelocities(
            const AZ::Vector3& linearVelocity,
            const AZ::Vector3& angularVelocity) = 0;

        virtual bool AddLinearVelocity(const AZ::Vector3& linearVelocity) = 0;

        virtual bool MoveKinematically(
            const WorldTransform& target,
            float duration) = 0;

        virtual bool UpdateRuntimeConfiguration(
            const BodyRuntimeConfiguration& configuration,
            bool activate) = 0;
    };

    using RigidBodyRequestBus = AZ::EBus<IRigidBodyRequests>;

} // namespace Jolt
