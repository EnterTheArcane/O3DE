/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/CustomConstraintInternal.h>

#include <Jolt/FloatEnvironment.h>

#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/utility/move.h>

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/MotionProperties.h>
#include <Jolt/Physics/StateRecorder.h>
#if defined(JPH_DEBUG_RENDERER)
#include <Jolt/Renderer/DebugRenderer.h>
#endif

namespace Jolt
{
    namespace
    {
        [[nodiscard]]
        AZ::Vector3 FromNativeCustomConstraintVector(
            const JPH::Vec3Arg value)
        {
            return {value.GetX(), value.GetY(), value.GetZ()};
        }

        [[nodiscard]]
        AZ::Quaternion FromNativeCustomConstraintRotation(
            const JPH::QuatArg rotation)
        {
            return AZ::Quaternion(
                rotation.GetX(),
                rotation.GetY(),
                rotation.GetZ(),
                rotation.GetW());
        }

        [[nodiscard]]
        WorldTransform FromNativeCustomConstraintTransform(
            JPH::RMat44Arg transform,
            const WorldPosition& origin)
        {
            const JPH::RVec3 position = transform.GetTranslation();
            return {
                .m_position = {
                    .m_x = static_cast<double>(position.GetX()) + origin.m_x,
                    .m_y = static_cast<double>(position.GetY()) + origin.m_y,
                    .m_z = static_cast<double>(position.GetZ()) + origin.m_z,
                },
                .m_rotation = FromNativeCustomConstraintRotation(transform.GetQuaternion()),
            };
        }

        [[nodiscard]]
        bool IsFinite(
            const CustomConstraintRow& row)
        {
            return row.m_firstAngular.IsFinite()
                && row.m_firstLinear.IsFinite()
                && row.m_secondAngular.IsFinite()
                && row.m_secondLinear.IsFinite()
                && AZ::IsFiniteFloat(row.m_bias)
                && AZ::IsFiniteFloat(row.m_error)
                && AZ::IsFiniteFloat(row.m_maximumImpulse)
                && AZ::IsFiniteFloat(row.m_minimumImpulse)
                && row.m_minimumImpulse <= row.m_maximumImpulse;
        }

        [[nodiscard]]
        JPH::Vec3 ToNativeCustomConstraintVector(
            const AZ::Vector3& value)
        {
            return {value.GetX(), value.GetY(), value.GetZ()};
        }
    } // namespace

    CustomConstraint::CustomConstraint(
        JPH::Body& firstBody,
        JPH::Body& secondBody,
        const JPH::TwoBodyConstraintSettings& settings,
        ICustomConstraintProvider& provider,
        const AZ::TypeId providerId,
        const AZ::u64 providerVersion,
        AZStd::vector<AZ::u8> data,
        AZStd::vector<AZ::u8> state,
        const WorldPosition& origin,
        JPH::Mat44Arg firstFrame,
        JPH::Mat44Arg secondFrame,
        const AZ::u32 maximumRowCount)
        : JPH::TwoBodyConstraint(firstBody, secondBody, settings)
        , m_provider(&provider)
        , m_rows(maximumRowCount)
        , m_solverRows(maximumRowCount)
        , m_data(AZStd::move(data))
        , m_state(AZStd::move(state))
        , m_origin(origin)
        , m_firstFrame(firstFrame)
        , m_secondFrame(secondFrame)
        , m_providerId(providerId)
        , m_providerVersion(providerVersion)
    {
    }

    JPH::EConstraintSubType CustomConstraint::GetSubType() const
    {
        return JPH::EConstraintSubType::User1;
    }

    void CustomConstraint::NotifyShapeChanged(
        const JPH::BodyID& bodyId,
        const JPH::Vec3Arg centerOfMassDelta)
    {
        if (mBody1->GetID() == bodyId)
        {
            m_firstFrame.SetTranslation(m_firstFrame.GetTranslation() - centerOfMassDelta);
        }
        else if (mBody2->GetID() == bodyId)
        {
            m_secondFrame.SetTranslation(m_secondFrame.GetTranslation() - centerOfMassDelta);
        }
    }

    void CustomConstraint::ResetWarmStart()
    {
        for (SolverRow& row : m_solverRows)
        {
            row.m_totalImpulse = 0.0f;
        }
    }

    void CustomConstraint::SetupVelocityConstraint(
        const float deltaTime)
    {
        m_velocityRowCount = PrepareRows(false, deltaTime, 0.0f);
        for (AZ::u32 rowIndex = 0; rowIndex < m_velocityRowCount; ++rowIndex)
        {
            [[maybe_unused]] const bool configured = ConfigureSolverRow(rowIndex, false);
        }
        for (size_t rowIndex = m_velocityRowCount; rowIndex < m_solverRows.size(); ++rowIndex)
        {
            m_solverRows[rowIndex].m_effectiveMass = 0.0f;
            m_solverRows[rowIndex].m_totalImpulse = 0.0f;
        }
    }

    void CustomConstraint::WarmStartVelocityConstraint(
        const float warmStartImpulseRatio)
    {
        for (AZ::u32 rowIndex = 0; rowIndex < m_velocityRowCount; ++rowIndex)
        {
            SolverRow& solverRow = m_solverRows[rowIndex];
            solverRow.m_totalImpulse *= warmStartImpulseRatio;
            [[maybe_unused]] const bool applied = ApplyVelocityImpulse(
                solverRow,
                solverRow.m_totalImpulse);
        }
    }

    bool CustomConstraint::SolveVelocityConstraint(
        [[maybe_unused]] const float deltaTime)
    {
        bool appliedImpulse = false;
        for (AZ::u32 rowIndex = 0; rowIndex < m_velocityRowCount; ++rowIndex)
        {
            SolverRow& solverRow = m_solverRows[rowIndex];
            if (solverRow.m_effectiveMass == 0.0f)
            {
                continue;
            }

            const CustomConstraintRow& row = m_rows[rowIndex];
            const float relativeVelocity =
                ToNativeCustomConstraintVector(row.m_firstLinear).Dot(mBody1->GetLinearVelocity())
                + ToNativeCustomConstraintVector(row.m_firstAngular).Dot(mBody1->GetAngularVelocity())
                + ToNativeCustomConstraintVector(row.m_secondLinear).Dot(mBody2->GetLinearVelocity())
                + ToNativeCustomConstraintVector(row.m_secondAngular).Dot(mBody2->GetAngularVelocity());
            const float impulse = -solverRow.m_effectiveMass * (relativeVelocity + row.m_bias);
            const float totalImpulse = AZStd::clamp(
                solverRow.m_totalImpulse + impulse,
                row.m_minimumImpulse,
                row.m_maximumImpulse);
            const float appliedRowImpulse = totalImpulse - solverRow.m_totalImpulse;
            solverRow.m_totalImpulse = totalImpulse;
            appliedImpulse |= ApplyVelocityImpulse(solverRow, appliedRowImpulse);
        }
        return appliedImpulse;
    }

    bool CustomConstraint::SolvePositionConstraint(
        const float deltaTime,
        const float baumgarte)
    {
        const AZ::u32 rowCount = PrepareRows(true, deltaTime, baumgarte);
        bool appliedImpulse = false;
        for (AZ::u32 rowIndex = 0; rowIndex < rowCount; ++rowIndex)
        {
            if (!ConfigureSolverRow(rowIndex, false))
            {
                continue;
            }

            const CustomConstraintRow& row = m_rows[rowIndex];
            const SolverRow& solverRow = m_solverRows[rowIndex];
            const float impulse = AZStd::clamp(
                -solverRow.m_effectiveMass * baumgarte * row.m_error,
                row.m_minimumImpulse,
                row.m_maximumImpulse);
            appliedImpulse |= ApplyPositionImpulse(solverRow, impulse);
        }
        return appliedImpulse;
    }

#if defined(JPH_DEBUG_RENDERER)
    void CustomConstraint::DrawConstraint(
        JPH::DebugRenderer* renderer) const
    {
        renderer->DrawMarker(
            mBody1->GetCenterOfMassTransform() * m_firstFrame.GetTranslation(),
            JPH::Color::sRed,
            0.1f);
        renderer->DrawMarker(
            mBody2->GetCenterOfMassTransform() * m_secondFrame.GetTranslation(),
            JPH::Color::sGreen,
            0.1f);
    }
#endif

    void CustomConstraint::SaveState(
        JPH::StateRecorder& stream) const
    {
        JPH::TwoBodyConstraint::SaveState(stream);
        stream.Write(m_velocityRowCount);
        for (const SolverRow& row : m_solverRows)
        {
            stream.Write(row.m_totalImpulse);
        }
        stream.WriteBytes(m_state.data(), m_state.size());
    }

    void CustomConstraint::RestoreState(
        JPH::StateRecorder& stream)
    {
        JPH::TwoBodyConstraint::RestoreState(stream);
        stream.Read(m_velocityRowCount);
        const bool validRowCount = m_velocityRowCount <= m_solverRows.size();
        for (SolverRow& row : m_solverRows)
        {
            stream.Read(row.m_totalImpulse);
        }
        stream.ReadBytes(m_state.data(), m_state.size());
        if (!validRowCount)
        {
            m_velocityRowCount = 0;
            ResetWarmStart();
        }
    }

    JPH::Ref<JPH::ConstraintSettings> CustomConstraint::GetConstraintSettings() const
    {
        return nullptr;
    }

    JPH::Mat44 CustomConstraint::GetConstraintToBody1Matrix() const
    {
        return m_firstFrame;
    }

    JPH::Mat44 CustomConstraint::GetConstraintToBody2Matrix() const
    {
        return m_secondFrame;
    }

    AZStd::span<const AZ::u8> CustomConstraint::GetData() const
    {
        return m_data;
    }

    AZ::TypeId CustomConstraint::GetProviderId() const
    {
        return m_providerId;
    }

    AZ::u64 CustomConstraint::GetProviderVersion() const
    {
        return m_providerVersion;
    }

    AZ::u32 CustomConstraint::GetMaximumRowCount() const
    {
        return aznumeric_cast<AZ::u32>(m_rows.size());
    }

    AZ::u32 CustomConstraint::GetVelocityRowCount() const
    {
        return m_velocityRowCount;
    }

    AZ::u32 CustomConstraint::GetImpulses(
        const AZStd::span<float> impulses) const
    {
        const size_t impulseCount = AZStd::min(impulses.size(), static_cast<size_t>(m_velocityRowCount));
        for (size_t rowIndex = 0; rowIndex < impulseCount; ++rowIndex)
        {
            impulses[rowIndex] = m_solverRows[rowIndex].m_totalImpulse;
        }
        return aznumeric_cast<AZ::u32>(impulseCount);
    }

    AZStd::span<const AZ::u8> CustomConstraint::GetState() const
    {
        return m_state;
    }

    bool CustomConstraint::SetState(
        const AZStd::span<const AZ::u8> state)
    {
        if (state.size() != m_state.size())
        {
            return false;
        }

        AZStd::copy(state.begin(), state.end(), m_state.begin());
        ResetWarmStart();
        return true;
    }

    CustomConstraintContext CustomConstraint::CreateContext(
        const float deltaTime,
        const float baumgarte) const
    {
        const JPH::RMat44 firstCenterOfMassTransform = mBody1->GetCenterOfMassTransform();
        const JPH::RMat44 secondCenterOfMassTransform = mBody2->GetCenterOfMassTransform();
        return {
            .m_firstBody = {
                .m_centerOfMassTransform = FromNativeCustomConstraintTransform(firstCenterOfMassTransform, m_origin),
                .m_angularVelocity = FromNativeCustomConstraintVector(mBody1->GetAngularVelocity()),
                .m_linearVelocity = FromNativeCustomConstraintVector(mBody1->GetLinearVelocity()),
            },
            .m_secondBody = {
                .m_centerOfMassTransform = FromNativeCustomConstraintTransform(secondCenterOfMassTransform, m_origin),
                .m_angularVelocity = FromNativeCustomConstraintVector(mBody2->GetAngularVelocity()),
                .m_linearVelocity = FromNativeCustomConstraintVector(mBody2->GetLinearVelocity()),
            },
            .m_firstFrame = FromNativeCustomConstraintTransform(firstCenterOfMassTransform * m_firstFrame, m_origin),
            .m_secondFrame = FromNativeCustomConstraintTransform(secondCenterOfMassTransform * m_secondFrame, m_origin),
            .m_baumgarte = baumgarte,
            .m_deltaTime = deltaTime,
        };
    }

    AZ::u32 CustomConstraint::PrepareRows(
        const bool position,
        const float deltaTime,
        const float baumgarte)
    {
        AZStd::fill(m_rows.begin(), m_rows.end(), CustomConstraintRow{});
        const DeterministicFloatScope floatScope;
        const CustomConstraintContext context = CreateContext(deltaTime, baumgarte);
        AZ::u32 rowCount = 0;
        if (position)
        {
            rowCount = m_provider->PreparePositionRows(context, m_data, m_state, m_rows);
        }
        else
        {
            rowCount = m_provider->PrepareVelocityRows(context, m_data, m_state, m_rows);
        }
        if (rowCount > m_rows.size())
        {
            return 0;
        }

        for (AZ::u32 rowIndex = 0; rowIndex < rowCount; ++rowIndex)
        {
            if (!IsFinite(m_rows[rowIndex]))
            {
                return 0;
            }
        }
        return rowCount;
    }

    bool CustomConstraint::ConfigureSolverRow(
        const AZ::u32 rowIndex,
        const bool resetImpulse)
    {
        const CustomConstraintRow& row = m_rows[rowIndex];
        SolverRow& solverRow = m_solverRows[rowIndex];
        if (resetImpulse)
        {
            solverRow.m_totalImpulse = 0.0f;
        }

        float inverseEffectiveMass = 0.0f;
        if (mBody1->IsDynamic())
        {
            const JPH::MotionProperties* motion = mBody1->GetMotionPropertiesUnchecked();
            const JPH::Vec3 linear = ToNativeCustomConstraintVector(row.m_firstLinear);
            const JPH::Vec3 angular = ToNativeCustomConstraintVector(row.m_firstAngular);
            solverRow.m_firstInverseLinear = motion->GetInverseMass() * linear;
            solverRow.m_firstInverseAngular = motion->MultiplyWorldSpaceInverseInertiaByVector(
                mBody1->GetRotation(),
                angular);
            inverseEffectiveMass += linear.Dot(solverRow.m_firstInverseLinear)
                + angular.Dot(solverRow.m_firstInverseAngular);
        }
        else
        {
            solverRow.m_firstInverseLinear = JPH::Vec3::sZero();
            solverRow.m_firstInverseAngular = JPH::Vec3::sZero();
        }

        if (mBody2->IsDynamic())
        {
            const JPH::MotionProperties* motion = mBody2->GetMotionPropertiesUnchecked();
            const JPH::Vec3 linear = ToNativeCustomConstraintVector(row.m_secondLinear);
            const JPH::Vec3 angular = ToNativeCustomConstraintVector(row.m_secondAngular);
            solverRow.m_secondInverseLinear = motion->GetInverseMass() * linear;
            solverRow.m_secondInverseAngular = motion->MultiplyWorldSpaceInverseInertiaByVector(
                mBody2->GetRotation(),
                angular);
            inverseEffectiveMass += linear.Dot(solverRow.m_secondInverseLinear)
                + angular.Dot(solverRow.m_secondInverseAngular);
        }
        else
        {
            solverRow.m_secondInverseLinear = JPH::Vec3::sZero();
            solverRow.m_secondInverseAngular = JPH::Vec3::sZero();
        }

        if (!AZ::IsFiniteFloat(inverseEffectiveMass) || inverseEffectiveMass <= 0.0f)
        {
            solverRow.m_effectiveMass = 0.0f;
            solverRow.m_totalImpulse = 0.0f;
            return false;
        }

        solverRow.m_effectiveMass = 1.0f / inverseEffectiveMass;
        return true;
    }

    bool CustomConstraint::ApplyPositionImpulse(
        const SolverRow& solverRow,
        const float impulse)
    {
        if (impulse == 0.0f)
        {
            return false;
        }

        if (mBody1->IsDynamic())
        {
            mBody1->AddPositionStep(impulse * solverRow.m_firstInverseLinear);
            mBody1->AddRotationStep(impulse * solverRow.m_firstInverseAngular);
        }
        if (mBody2->IsDynamic())
        {
            mBody2->AddPositionStep(impulse * solverRow.m_secondInverseLinear);
            mBody2->AddRotationStep(impulse * solverRow.m_secondInverseAngular);
        }
        return true;
    }

    bool CustomConstraint::ApplyVelocityImpulse(
        const SolverRow& solverRow,
        const float impulse)
    {
        if (impulse == 0.0f)
        {
            return false;
        }

        if (mBody1->IsDynamic())
        {
            JPH::MotionProperties* motion = mBody1->GetMotionPropertiesUnchecked();
            motion->AddLinearVelocityStep(impulse * solverRow.m_firstInverseLinear);
            motion->AddAngularVelocityStep(impulse * solverRow.m_firstInverseAngular);
        }
        if (mBody2->IsDynamic())
        {
            JPH::MotionProperties* motion = mBody2->GetMotionPropertiesUnchecked();
            motion->AddLinearVelocityStep(impulse * solverRow.m_secondInverseLinear);
            motion->AddAngularVelocityStep(impulse * solverRow.m_secondInverseAngular);
        }
        return true;
    }
} // namespace Jolt
