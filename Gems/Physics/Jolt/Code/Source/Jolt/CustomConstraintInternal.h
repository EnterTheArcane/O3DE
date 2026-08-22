/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/CustomConstraint.h>

#include <AzCore/std/containers/vector.h>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Constraints/TwoBodyConstraint.h>

namespace Jolt
{
    class CustomConstraint final
        : public JPH::TwoBodyConstraint
    {
    public:
        JPH_OVERRIDE_NEW_DELETE

        CustomConstraint(
            JPH::Body& firstBody,
            JPH::Body& secondBody,
            const JPH::TwoBodyConstraintSettings& settings,
            ICustomConstraintProvider& provider,
            AZ::TypeId providerId,
            AZ::u64 providerVersion,
            AZStd::vector<AZ::u8> data,
            AZStd::vector<AZ::u8> state,
            const WorldPosition& origin,
            JPH::Mat44Arg firstFrame,
            JPH::Mat44Arg secondFrame,
            AZ::u32 maximumRowCount);

        JPH::EConstraintSubType GetSubType() const override;

        void NotifyShapeChanged(
            const JPH::BodyID& bodyId,
            JPH::Vec3Arg centerOfMassDelta) override;

        void ResetWarmStart() override;

        void SetupVelocityConstraint(float deltaTime) override;

        void WarmStartVelocityConstraint(float warmStartImpulseRatio) override;

        bool SolveVelocityConstraint(float deltaTime) override;

        bool SolvePositionConstraint(
            float deltaTime,
            float baumgarte) override;

#ifdef JPH_DEBUG_RENDERER
        void DrawConstraint(JPH::DebugRenderer* renderer) const override;
#endif

        void SaveState(JPH::StateRecorder& stream) const override;

        void RestoreState(JPH::StateRecorder& stream) override;

        JPH::Ref<JPH::ConstraintSettings> GetConstraintSettings() const override;

        JPH::Mat44 GetConstraintToBody1Matrix() const override;

        JPH::Mat44 GetConstraintToBody2Matrix() const override;

        [[nodiscard]]
        AZStd::span<const AZ::u8> GetData() const;

        [[nodiscard]]
        AZ::TypeId GetProviderId() const;

        [[nodiscard]]
        AZ::u64 GetProviderVersion() const;

        [[nodiscard]]
        AZ::u32 GetMaximumRowCount() const;

        [[nodiscard]]
        AZ::u32 GetVelocityRowCount() const;

        [[nodiscard]]
        AZ::u32 GetImpulses(AZStd::span<float> impulses) const;

        [[nodiscard]]
        AZStd::span<const AZ::u8> GetState() const;

        bool SetState(AZStd::span<const AZ::u8> state);

    private:
        struct SolverRow final
        {
            JPH::Vec3 m_firstInverseAngular = JPH::Vec3::sZero();
            JPH::Vec3 m_firstInverseLinear = JPH::Vec3::sZero();
            JPH::Vec3 m_secondInverseAngular = JPH::Vec3::sZero();
            JPH::Vec3 m_secondInverseLinear = JPH::Vec3::sZero();
            float m_effectiveMass = 0.0f;
            float m_totalImpulse = 0.0f;
        };

        [[nodiscard]]
        CustomConstraintContext CreateContext(
            float deltaTime,
            float baumgarte) const;

        [[nodiscard]]
        AZ::u32 PrepareRows(
            bool position,
            float deltaTime,
            float baumgarte);

        [[nodiscard]]
        bool ConfigureSolverRow(
            AZ::u32 rowIndex,
            bool resetImpulse);

        [[nodiscard]]
        bool ApplyPositionImpulse(
            const SolverRow& solverRow,
            float impulse);

        [[nodiscard]]
        bool ApplyVelocityImpulse(
            const SolverRow& solverRow,
            float impulse);

        ICustomConstraintProvider* m_provider = nullptr;

        AZStd::vector<CustomConstraintRow> m_rows;
        AZStd::vector<SolverRow> m_solverRows;

        AZStd::vector<AZ::u8> m_data;
        AZStd::vector<AZ::u8> m_state;

        WorldPosition m_origin;
        JPH::Mat44 m_firstFrame;
        JPH::Mat44 m_secondFrame;

        AZ::TypeId m_providerId = AZ::TypeId::CreateNull();
        AZ::u64 m_providerVersion = 0;
        AZ::u32 m_velocityRowCount = 0;
    };
} // namespace Jolt
