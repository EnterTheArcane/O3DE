/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Rollback.h>

#include <Jolt/BodyConfiguration.h>
#include <Jolt/Constraint.h>
#include <Jolt/Handle.h>
#include <Jolt/Query.h>
#include <Jolt/TypeIds.h>

#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/base.h>

namespace Jolt
{
    struct StepInformation final
    {
        AZ_TYPE_INFO(StepInformation, StepInformationTypeId);

        float m_deltaTime = 0.0f;
        bool m_isFirst = false;
        bool m_isLast = false;
    };

    class IStepContext
    {
    public:
        virtual ~IStepContext() = default;

        //! This no-lock view is valid only for the duration of IStepListener::OnStep.

        virtual bool AddForceAndTorque(
            BodyHandle bodyHandle,
            const AZ::Vector3& force,
            const AZ::Vector3& torque) = 0;

        virtual bool AddImpulse(
            BodyHandle bodyHandle,
            const AZ::Vector3& impulse) = 0;

        [[nodiscard]]
        virtual bool GetBodyState(
            BodyHandle bodyHandle,
            BodyState& state) const = 0;

        virtual bool SetBodyVelocities(
            BodyHandle bodyHandle,
            const AZ::Vector3& linearVelocity,
            const AZ::Vector3& angularVelocity) = 0;

        virtual bool SetConstraintEnabled(
            ConstraintHandle constraintHandle,
            bool enabled) = 0;
    };

    class IStepListener
        : public IRollbackParticipant
    {
    public:
        virtual ~IStepListener() = default;

        //! Called under simulation locks and potentially on a worker. Do not call ISystem.
        //! The state hash must include behavior-affecting mutable state, but exclude observational state.

        [[nodiscard]]
        virtual AZ::u64 GetStateHash() const = 0;

        virtual void OnStep(
            const StepInformation& information,
            IStepContext& context) = 0;
    };

    class ISimulationShapeFilter
        : public IRollbackParticipant
    {
    public:
        virtual ~ISimulationShapeFilter() = default;

        //! Called concurrently under simulation locks. Implementations must not call ISystem.
        //! The state hash must include behavior-affecting mutable state, but exclude observational state.

        [[nodiscard]]
        virtual AZ::u64 GetStateHash() const = 0;

        [[nodiscard]]
        virtual bool ShouldCollide(
            const SimulationShape& firstShape,
            const SimulationShape& secondShape) const = 0;
    };
} // namespace Jolt
