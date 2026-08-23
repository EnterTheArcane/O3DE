/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <Jolt/Constraint.h>
#include <Jolt/Query.h>

namespace Jolt
{
    class Runtime;

    class JOLT_API Constraints
    {
    public:
        //! Returns the active capability, or nullptr if no global System is active.
        //! The pointer is non-owning and must not be acquired or used while System destruction can occur.
        [[nodiscard]]
        static Constraints* Get();

        [[nodiscard]]
        ConstraintHandle CreateConstraint(
            WorldHandle worldHandle,
            const ConstraintConfiguration& configuration);

        bool AddConstraintToSimulation(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle);

        bool AddConstraintsToSimulation(
            WorldHandle worldHandle,
            AZStd::span<const ConstraintHandle> constraintHandles);

        bool RemoveConstraintFromSimulation(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle);

        bool RemoveConstraintsFromSimulation(
            WorldHandle worldHandle,
            AZStd::span<const ConstraintHandle> constraintHandles);

        bool DestroyConstraint(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle);

        bool DestroyConstraints(
            WorldHandle worldHandle,
            AZStd::span<const ConstraintHandle> constraintHandles);

        [[nodiscard]]
        bool IsConstraintInSimulation(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle) const;

        [[nodiscard]]
        bool IsValid(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle) const;

        bool SetConstraintEnabled(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            bool enabled);

        [[nodiscard]]
        bool GetConstraintState(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            ConstraintState& state) const;

        [[nodiscard]]
        bool GetConstraintConfiguration(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            ConstraintConfiguration& configuration) const;

        [[nodiscard]]
        bool GetConstraintUserData(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            AZ::u64& userData) const;

        bool SetConstraintUserData(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            AZ::u64 userData);

        [[nodiscard]]
        bool GetConstraintDebugDrawSize(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            float& debugDrawSize) const;

        bool SetConstraintDebugDrawSize(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            float debugDrawSize);

        [[nodiscard]]
        bool GetConstraintMeasurements(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            ConstraintMeasurements& measurements) const;

        [[nodiscard]]
        bool GetCustomConstraintInfo(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            CustomConstraintInfo& info) const;

        [[nodiscard]]
        BufferResult GetCustomConstraintImpulses(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            AZStd::span<float> impulses) const;

        [[nodiscard]]
        BufferResult GetCustomConstraintState(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            AZStd::span<AZ::u8> state) const;

        bool SetCustomConstraintState(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            AZStd::span<const AZ::u8> state);

        bool ResetConstraintWarmStart(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle);

        bool UpdateConstraintSolverConfiguration(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            const ConstraintSolverConfiguration& configuration);

        bool UpdateConeLimit(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            float halfConeAngle);

        bool UpdateDistanceLimits(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            float minimumDistance,
            float maximumDistance,
            const SpringConfiguration& spring);

        bool UpdateHingeLimits(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            float minimumAngle,
            float maximumAngle,
            const SpringConfiguration& spring,
            float maximumFrictionTorque);

        bool UpdateHingeMotor(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            const MotorConfiguration& motor,
            float targetAngle,
            float targetAngularVelocity);

        //! Sets the target orientation of a hinge motor relative to its first body.
        bool SetHingeTargetOrientation(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            const AZ::Quaternion& targetOrientation);

        bool UpdatePathMotor(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            const MotorConfiguration& motor,
            float targetPathFraction,
            float targetVelocity);

        bool UpdatePathProperties(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            PathHandle pathHandle,
            float pathFraction,
            float maximumFrictionForce);

        bool UpdatePointAnchors(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            ConstraintSpace space,
            const WorldPosition& firstPoint,
            const WorldPosition& secondPoint);

        bool UpdatePulleyLimits(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            float minimumLength,
            float maximumLength);

        bool UpdateSixDofLimits(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            AZStd::span<const SixDofAxisLimitConfiguration> axes);

        bool UpdateSixDofMotors(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            AZStd::span<const MotorConfiguration> motors,
            const AZ::Vector3& targetAngularVelocity,
            const AZ::Quaternion& targetOrientation,
            const AZ::Vector3& targetPosition,
            const AZ::Vector3& targetVelocity);

        bool UpdateSliderMotor(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            const MotorConfiguration& motor,
            float targetPosition,
            float targetVelocity);

        bool UpdateSliderLimits(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            float minimumPosition,
            float maximumPosition,
            const SpringConfiguration& spring,
            float maximumFrictionForce);

        bool UpdateSwingTwistMotors(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            const MotorConfiguration& swingMotor,
            const MotorConfiguration& twistMotor,
            const AZ::Vector3& targetAngularVelocity,
            const AZ::Quaternion& targetOrientation);

        bool UpdateSwingTwistLimits(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle,
            float normalHalfConeAngle,
            float planeHalfConeAngle,
            float twistMinimumAngle,
            float twistMaximumAngle,
            float maximumFrictionTorque);

    private:
        friend class Runtime;

        Constraints() = default;
        ~Constraints() = default;
    };
} // namespace Jolt
