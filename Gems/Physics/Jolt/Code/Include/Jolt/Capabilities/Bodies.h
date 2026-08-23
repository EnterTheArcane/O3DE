/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/BodyCollision.h>
#include <Jolt/BodyConfiguration.h>
#include <Jolt/Configuration.h>
#include <Jolt/Diagnostics.h>
#include <Jolt/Query.h>
#include <Jolt/Shape.h>

namespace Jolt
{
    class Runtime;

    class JOLT_API Bodies
    {
    public:
        //! Returns the active capability, or nullptr if no global System is active.
        //! The pointer is non-owning and must not be acquired or used while System destruction can occur.
        [[nodiscard]]
        static Bodies* Get();

        [[nodiscard]]
        BodyHandle CreateBody(
            WorldHandle worldHandle,
            const BodyConfiguration& configuration);

        //! Creates a body with an explicit simulation identity for synchronized peers.
        [[nodiscard]]
        BodyHandle CreateBodyWithId(
            WorldHandle worldHandle,
            BodyId bodyId,
            const BodyConfiguration& configuration);

        bool AddBodyToSimulation(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool activate);

        [[nodiscard]]
        bool GetBodyUserData(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::u64& userData) const;

        bool SetBodyUserData(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::u64 userData);

        [[nodiscard]]
        bool GetBodyRuntimeConfiguration(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            BodyRuntimeConfiguration& configuration) const;

        //! Returns false when per-body statistics were not enabled when the native library was built.
        [[nodiscard]]
        bool GetBodySimulationStatistics(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            BodySimulationStatistics& statistics) const;

        //! Replaces rigid-body creation settings while preserving its native allocation capability.
        //! The body and configuration must remain outside the simulation, and the body cannot belong to a composite runtime object.
        bool ApplyBodyConfiguration(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const BodyConfiguration& configuration);

        bool AddBodiesToSimulation(
            WorldHandle worldHandle,
            AZStd::span<const BodyHandle> bodyHandles,
            bool activate);

        bool RemoveBodyFromSimulation(
            WorldHandle worldHandle,
            BodyHandle bodyHandle);

        bool RemoveBodiesFromSimulation(
            WorldHandle worldHandle,
            AZStd::span<const BodyHandle> bodyHandles);

        bool DestroyBody(
            WorldHandle worldHandle,
            BodyHandle bodyHandle);

        bool DestroyBodies(
            WorldHandle worldHandle,
            AZStd::span<const BodyHandle> bodyHandles);

        [[nodiscard]]
        bool IsBodyInSimulation(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) const;

        [[nodiscard]]
        bool IsValid(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) const;

        bool SetBodyMoveEventsEnabled(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool enabled);

        [[nodiscard]]
        bool GetBodyState(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            BodyState& state) const;

        [[nodiscard]]
        bool GetBodyCenterOfMassTransform(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            WorldTransform& transform) const;

        [[nodiscard]]
        bool GetBodyConfiguration(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            BodyConfiguration& configuration) const;

        [[nodiscard]]
        QueryResult GetBodies(
            WorldHandle worldHandle,
            BodyKind kind,
            bool activeOnly,
            AZStd::span<BodyHandle> bodies) const;

        //! Returns the identity that participates in native deterministic ordering.
        [[nodiscard]]
        bool GetBodyId(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            BodyId& bodyId) const;

        bool ActivateBody(
            WorldHandle worldHandle,
            BodyHandle bodyHandle);

        bool ActivateBodies(
            WorldHandle worldHandle,
            AZStd::span<const BodyHandle> bodyHandles);

        bool ActivateBodiesInBounds(
            WorldHandle worldHandle,
            const BroadPhaseAabb& bounds,
            ObjectLayer collisionLayer = ObjectLayer::Invalid);

        bool DeactivateBody(
            WorldHandle worldHandle,
            BodyHandle bodyHandle);

        bool DeactivateBodies(
            WorldHandle worldHandle,
            AZStd::span<const BodyHandle> bodyHandles);

        bool ResetBodySleepTimer(
            WorldHandle worldHandle,
            BodyHandle bodyHandle);

        bool InvalidateBodyContactCache(
            WorldHandle worldHandle,
            BodyHandle bodyHandle);

        [[nodiscard]]
        bool GetBodyPointVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const WorldPosition& point,
            AZ::Vector3& velocity) const;

        [[nodiscard]]
        bool GetBodyMotionType(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            MotionType& motionType) const;

        [[nodiscard]]
        bool GetBodyObjectLayer(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            ObjectLayer& objectLayer) const;

        [[nodiscard]]
        bool GetBodyCollisionGroup(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            CollisionGroupConfiguration& collisionGroup) const;

        [[nodiscard]]
        bool GetBodyShape(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            ShapeHandle& shapeHandle) const;

        [[nodiscard]]
        bool GetBodyAccumulatedForceAndTorque(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::Vector3& force,
            AZ::Vector3& torque) const;

        bool ResetBodyAccumulatedForce(
            WorldHandle worldHandle,
            BodyHandle bodyHandle);

        bool ResetBodyAccumulatedTorque(
            WorldHandle worldHandle,
            BodyHandle bodyHandle);

        bool ResetBodyMotion(
            WorldHandle worldHandle,
            BodyHandle bodyHandle);

        [[nodiscard]]
        bool GetBodyBounds(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            BroadPhaseAabb& bounds) const;

        [[nodiscard]]
        bool GetBodySubmergedVolume(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const WorldPosition& surfacePosition,
            const AZ::Vector3& surfaceNormal,
            SubmergedVolumeResult& result) const;

        [[nodiscard]]
        bool GetBodySurfaceNormal(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            SubShapeId subShapeId,
            const WorldPosition& surfacePosition,
            AZ::Vector3& normal) const;

        [[nodiscard]]
        bool GetBodyMaterial(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            SubShapeId subShapeId,
            MaterialHandle& materialHandle) const;

        [[nodiscard]]
        bool GetBodyPosition(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            WorldPosition& position) const;

        [[nodiscard]]
        bool GetBodyRotation(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::Quaternion& rotation) const;

        [[nodiscard]]
        bool GetBodyVelocities(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::Vector3& linearVelocity,
            AZ::Vector3& angularVelocity) const;

        [[nodiscard]]
        bool GetBodyLinearVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::Vector3& linearVelocity) const;

        [[nodiscard]]
        bool GetBodyAngularVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::Vector3& angularVelocity) const;

        bool SetBodyPosition(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const WorldPosition& position,
            bool activate);

        bool SetBodyRotation(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Quaternion& rotation,
            bool activate);

        bool SetBodyTransform(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const WorldTransform& transform,
            bool activate);

        bool SetBodyTransformWhenChanged(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const WorldTransform& transform,
            bool activate);

        bool SetBodyVelocities(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& linearVelocity,
            const AZ::Vector3& angularVelocity);

        bool SetBodyLinearVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& linearVelocity);

        bool SetBodyAngularVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& angularVelocity);

        bool AddBodyVelocities(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& linearVelocity,
            const AZ::Vector3& angularVelocity);

        bool AddBodyLinearVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& linearVelocity);

        bool SetBodyTransformAndVelocities(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const WorldTransform& transform,
            const AZ::Vector3& linearVelocity,
            const AZ::Vector3& angularVelocity);

        bool MoveBodyKinematically(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const WorldTransform& target,
            float duration);

        bool AddForce(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& force,
            bool activate = true);

        bool AddForceAtPosition(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& force,
            const WorldPosition& position,
            bool activate = true);

        bool AddTorque(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& torque,
            bool activate = true);

        bool AddForceAndTorque(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& force,
            const AZ::Vector3& torque,
            bool activate = true);

        bool ApplyBuoyancyImpulse(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const BuoyancyConfiguration& configuration);

        [[nodiscard]]
        bool GetBodyFriction(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float& friction) const;

        bool SetBodyFriction(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float friction);

        [[nodiscard]]
        bool GetBodyRestitution(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float& restitution) const;

        bool SetBodyRestitution(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float restitution);

        [[nodiscard]]
        bool GetBodyGravityFactor(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float& gravityFactor) const;

        bool SetBodyGravityFactor(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float gravityFactor);

        [[nodiscard]]
        bool GetBodyMaximumLinearVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float& maximumLinearVelocity) const;

        bool SetBodyMaximumLinearVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float maximumLinearVelocity);

        [[nodiscard]]
        bool GetBodyMaximumAngularVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float& maximumAngularVelocity) const;

        bool SetBodyMaximumAngularVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float maximumAngularVelocity);

        [[nodiscard]]
        bool GetBodyMotionQuality(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            MotionQuality& motionQuality) const;

        bool SetBodyMotionQuality(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            MotionQuality motionQuality);

        [[nodiscard]]
        bool IsBodyManifoldReductionEnabled(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool& enabled) const;

        bool SetBodyManifoldReductionEnabled(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool enabled);

        [[nodiscard]]
        bool IsBodySensor(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool& sensor) const;

        bool SetBodySensor(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool sensor);

        [[nodiscard]]
        bool GetBodyLinearDamping(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float& linearDamping) const;

        bool SetBodyLinearDamping(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float linearDamping);

        [[nodiscard]]
        bool GetBodyAngularDamping(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float& angularDamping) const;

        bool SetBodyAngularDamping(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float angularDamping);

        [[nodiscard]]
        bool IsBodySleepingAllowed(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool& sleepingAllowed) const;

        bool SetBodySleepingAllowed(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool sleepingAllowed);

        [[nodiscard]]
        bool IsBodyGyroscopicForceEnabled(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool& enabled) const;

        bool SetBodyGyroscopicForceEnabled(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool enabled);

        [[nodiscard]]
        bool IsBodyKinematicVsNonDynamicCollisionEnabled(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool& enabled) const;

        bool SetBodyKinematicVsNonDynamicCollisionEnabled(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool enabled);

        [[nodiscard]]
        bool IsBodyEnhancedInternalEdgeRemovalEnabled(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool& enabled) const;

        bool SetBodyEnhancedInternalEdgeRemovalEnabled(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool enabled);

        [[nodiscard]]
        bool GetBodySolverStepCounts(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::u8& velocityStepCount,
            AZ::u8& positionStepCount) const;

        bool SetBodySolverStepCounts(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::u8 velocityStepCount,
            AZ::u8 positionStepCount);

        bool UpdateBodyRuntimeConfiguration(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const BodyRuntimeConfiguration& configuration,
            bool activate);

        [[nodiscard]]
        bool GetBodyInverseInertia(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::Matrix3x3& inverseInertia) const;

        [[nodiscard]]
        bool GetBodyInverseMass(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float& inverseMass) const;

        bool AddImpulse(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& impulse);

        bool AddImpulseAtPosition(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& impulse,
            const WorldPosition& position);

        bool AddAngularImpulse(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& angularImpulse);

        bool SetBodyShape(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            ShapeHandle shapeHandle,
            bool updateMassProperties,
            bool activate);

        bool SetBodyMotionType(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            MotionType motionType,
            bool activate);

        bool SetBodyObjectLayer(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            ObjectLayer objectLayer);

        bool SetBodyCollisionGroup(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const CollisionGroupConfiguration& collisionGroup,
            bool activate);

    private:
        friend class Runtime;

        Bodies() = default;
        ~Bodies() = default;
    };
} // namespace Jolt
