/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/HairInternal.h>

#include <AzCore/std/limits.h>
#include <AzCore/Casting/numeric_cast.h>

#include <cmath>
#include <cstring>
#include <type_traits>

namespace Jolt
{
    namespace
    {
        [[nodiscard]]
        bool HasBufferLayout(
            const NativeHair::BufferState& state,
            const AZ::u64 elementCount,
            const size_t stride)
        {
            return state.m_present
                && state.m_elementCount == elementCount
                && state.m_stride == stride
                && stride > 0
                && elementCount <= AZStd::numeric_limits<size_t>::max() / stride
                && state.m_data.size() == elementCount * stride;
        }

        [[nodiscard]]
        bool IsBufferCanonicalEmpty(const NativeHair::BufferState& state)
        {
            return !state.m_present
                && state.m_data.empty()
                && state.m_elementCount == 0
                && state.m_stride == 0;
        }

        [[nodiscard]]
        bool IsFinite(const JPH::Float3& value)
        {
            return std::isfinite(value.x)
                && std::isfinite(value.y)
                && std::isfinite(value.z);
        }

        [[nodiscard]]
        bool IsFinite(const JPH::Float4& value)
        {
            return std::isfinite(value.x)
                && std::isfinite(value.y)
                && std::isfinite(value.z)
                && std::isfinite(value.w);
        }

        [[nodiscard]]
        bool IsFinite(const JPH_HairGlobalPoseTransform& value)
        {
            return IsFinite(value.mPosition)
                && IsFinite(value.mRotation);
        }

        [[nodiscard]]
        bool IsFinite(const JPH_HairPosition& value)
        {
            return IsFinite(value.mPosition)
                && IsFinite(value.mRotation);
        }

        [[nodiscard]]
        bool IsFinite(const JPH_HairVelocity& value)
        {
            return IsFinite(value.mVelocity)
                && IsFinite(value.mAngularVelocity);
        }

        [[nodiscard]]
        bool IsNormalized(const JPH::Float4& rotation)
        {
            return JPH::Quat(rotation.x, rotation.y, rotation.z, rotation.w).IsNormalized();
        }

        [[nodiscard]]
        bool IsZero(const JPH::Float4& rotation)
        {
            return rotation.x == 0.0f
                && rotation.y == 0.0f
                && rotation.z == 0.0f
                && rotation.w == 0.0f;
        }

        template<class Element>
        [[nodiscard]]
        bool HasFiniteBufferElements(const NativeHair::BufferState& state)
        {
            static_assert(std::is_trivially_copyable_v<Element>);
            if (state.m_data.size() % sizeof(Element) != 0)
            {
                return false;
            }
            for (size_t offset = 0; offset < state.m_data.size(); offset += sizeof(Element))
            {
                Element element;
                std::memcpy(&element, state.m_data.data() + offset, sizeof(element));
                if (!IsFinite(element))
                {
                    return false;
                }
            }
            return true;
        }

        template<class Element>
        [[nodiscard]]
        bool HasCompatibleRotations(
            const NativeHair::BufferState& state,
            const bool allowZero)
        {
            static_assert(std::is_trivially_copyable_v<Element>);
            if (state.m_data.size() % sizeof(Element) != 0)
            {
                return false;
            }
            for (size_t offset = 0; offset < state.m_data.size(); offset += sizeof(Element))
            {
                Element element;
                std::memcpy(&element, state.m_data.data() + offset, sizeof(element));
                if (!IsNormalized(element.mRotation)
                    && (!allowZero || !IsZero(element.mRotation)))
                {
                    return false;
                }
            }
            return true;
        }

    } // namespace

    const JPH::Mat44& NativeHair::GetScalpToHeadTransform() const
    {
        return mScalpToHead;
    }

    bool NativeHair::CaptureState(
        State& state) const
    {
        state.m_previousPosition = mPrevPosition;
        state.m_position = mPosition;
        state.m_previousRotation = mPrevRotation;
        state.m_rotation = mRotation;
        state.m_teleported = mTeleported;
        return CaptureBuffer(mGlobalPoseTransformsCB, state.m_globalPoseTransforms)
            && CaptureBuffer(mPositionsCB, state.m_positions)
            && CaptureBuffer(mPreviousPositionsCB, state.m_previousPositions)
            && CaptureBuffer(mRenderPositionsCB, state.m_renderPositions)
            && CaptureBuffer(mScalpVerticesCB, state.m_scalpVertices)
            && CaptureBuffer(mTargetGlobalPoseTransformsCB, state.m_targetGlobalPoseTransforms)
            && CaptureBuffer(mTargetPositionsCB, state.m_targetPositions)
            && CaptureBuffer(mVelocitiesCB, state.m_velocities)
            && CaptureBuffer(mVelocityAndDensityCB, state.m_velocityAndDensity);
    }

    bool NativeHair::IsStateLayoutCompatible(const State& state) const
    {
        const JPH::HairSettings* settings = GetHairSettings();
        const AZ::u64 simulationStrandCount = settings->mSimStrands.size();
        const AZ::u64 paddedVertexCount = settings->GetNumVerticesPadded();
        const bool hasScalp = !settings->mScalpInverseBindPose.empty()
            && !settings->mScalpVertices.empty();
        if (!HasBufferLayout(
                state.m_globalPoseTransforms,
                simulationStrandCount,
                sizeof(JPH_HairGlobalPoseTransform))
            || !HasBufferLayout(state.m_positions, paddedVertexCount, sizeof(JPH_HairPosition))
            || !HasBufferLayout(state.m_previousPositions, paddedVertexCount, sizeof(JPH_HairPosition))
            || !HasBufferLayout(
                state.m_renderPositions,
                settings->mRenderVertices.size(),
                sizeof(JPH::Float3))
            || !HasBufferLayout(state.m_velocities, paddedVertexCount, sizeof(JPH_HairVelocity))
            || !HasBufferLayout(
                state.m_velocityAndDensity,
                settings->mNeutralDensity.size(),
                sizeof(JPH::Float4))
            || state.m_scalpVertices.m_present != hasScalp)
        {
            return false;
        }
        if (hasScalp
            && !HasBufferLayout(
                state.m_scalpVertices,
                settings->mScalpVertices.size(),
                sizeof(JPH::Float3)))
        {
            return false;
        }
        if (!hasScalp && !IsBufferCanonicalEmpty(state.m_scalpVertices))
        {
            return false;
        }

        if (state.m_targetPositions.m_present != state.m_targetGlobalPoseTransforms.m_present)
        {
            return false;
        }
        if (!state.m_targetPositions.m_present)
        {
            return IsBufferCanonicalEmpty(state.m_targetPositions)
                && IsBufferCanonicalEmpty(state.m_targetGlobalPoseTransforms);
        }
        return hasScalp
            && HasBufferLayout(
                state.m_targetPositions,
                simulationStrandCount,
                sizeof(JPH_HairPosition))
            && HasBufferLayout(
                state.m_targetGlobalPoseTransforms,
                simulationStrandCount,
                sizeof(JPH_HairGlobalPoseTransform));
    }

    bool NativeHair::IsImportedStatePayloadCompatible(
        const State& state,
        const bool initialized) const
    {
        if (!IsStateLayoutCompatible(state)
            || !IsStatePayloadFinite(state)
            || (!initialized && state.m_targetPositions.m_present))
        {
            return false;
        }

        const bool allowZero = !initialized;
        if (!HasCompatibleRotations<JPH_HairGlobalPoseTransform>(
                state.m_globalPoseTransforms,
                allowZero)
            || !HasCompatibleRotations<JPH_HairPosition>(state.m_positions, allowZero)
            || !HasCompatibleRotations<JPH_HairPosition>(state.m_previousPositions, true))
        {
            return false;
        }

        if (!state.m_targetPositions.m_present)
        {
            return true;
        }
        return HasCompatibleRotations<JPH_HairPosition>(state.m_targetPositions, false)
            && HasCompatibleRotations<JPH_HairGlobalPoseTransform>(
                state.m_targetGlobalPoseTransforms,
                false);
    }

    bool NativeHair::IsStateCanonicalEmpty(const State& state)
    {
        return state.m_previousPosition == JPH::RVec3::sZero()
            && state.m_position == JPH::RVec3::sZero()
            && state.m_previousRotation == JPH::Quat::sIdentity()
            && state.m_rotation == JPH::Quat::sIdentity()
            && IsBufferCanonicalEmpty(state.m_globalPoseTransforms)
            && IsBufferCanonicalEmpty(state.m_positions)
            && IsBufferCanonicalEmpty(state.m_previousPositions)
            && IsBufferCanonicalEmpty(state.m_renderPositions)
            && IsBufferCanonicalEmpty(state.m_scalpVertices)
            && IsBufferCanonicalEmpty(state.m_targetGlobalPoseTransforms)
            && IsBufferCanonicalEmpty(state.m_targetPositions)
            && IsBufferCanonicalEmpty(state.m_velocities)
            && IsBufferCanonicalEmpty(state.m_velocityAndDensity)
            && state.m_teleported;
    }

    bool NativeHair::IsStatePayloadFinite(const State& state)
    {
        return HasFiniteBufferElements<JPH_HairGlobalPoseTransform>(state.m_globalPoseTransforms)
            && HasFiniteBufferElements<JPH_HairPosition>(state.m_positions)
            && HasFiniteBufferElements<JPH_HairPosition>(state.m_previousPositions)
            && HasFiniteBufferElements<JPH::Float3>(state.m_renderPositions)
            && HasFiniteBufferElements<JPH::Float3>(state.m_scalpVertices)
            && HasFiniteBufferElements<JPH_HairGlobalPoseTransform>(state.m_targetGlobalPoseTransforms)
            && HasFiniteBufferElements<JPH_HairPosition>(state.m_targetPositions)
            && HasFiniteBufferElements<JPH_HairVelocity>(state.m_velocities)
            && HasFiniteBufferElements<JPH::Float4>(state.m_velocityAndDensity);
    }

    bool NativeHair::RestoreState(
        const State& state,
        JPH::ComputeSystem& computeSystem)
    {
        if (!RestoreBuffer(mGlobalPoseTransformsCB, state.m_globalPoseTransforms, computeSystem)
            || !RestoreBuffer(mPositionsCB, state.m_positions, computeSystem)
            || !RestoreBuffer(mPreviousPositionsCB, state.m_previousPositions, computeSystem)
            || !RestoreBuffer(mRenderPositionsCB, state.m_renderPositions, computeSystem)
            || !RestoreBuffer(mScalpVerticesCB, state.m_scalpVertices, computeSystem)
            || !RestoreBuffer(mTargetGlobalPoseTransformsCB, state.m_targetGlobalPoseTransforms, computeSystem)
            || !RestoreBuffer(mTargetPositionsCB, state.m_targetPositions, computeSystem)
            || !RestoreBuffer(mVelocitiesCB, state.m_velocities, computeSystem)
            || !RestoreBuffer(mVelocityAndDensityCB, state.m_velocityAndDensity, computeSystem))
        {
            return false;
        }

        mPrevPosition = state.m_previousPosition;
        mPosition = state.m_position;
        mPrevRotation = state.m_previousRotation;
        mRotation = state.m_rotation;
        mTeleported = state.m_teleported;
        return true;
    }

    bool NativeHair::CaptureBuffer(
        JPH::ComputeBuffer* buffer,
        BufferState& state)
    {
        if (!buffer)
        {
            state.m_data.clear();
            state.m_elementCount = 0;
            state.m_stride = 0;
            state.m_present = false;
            return true;
        }
        if (buffer->GetStride() == 0
            || buffer->GetSize() > AZStd::numeric_limits<size_t>::max() / buffer->GetStride())
        {
            return false;
        }

        const size_t byteCount = aznumeric_cast<size_t>(buffer->GetSize()) * buffer->GetStride();
        state.m_data.resize(byteCount);
        const void* source = buffer->Map(JPH::ComputeBuffer::EMode::Read);
        if (!source)
        {
            return false;
        }

        std::memcpy(state.m_data.data(), source, byteCount);
        buffer->Unmap();
        state.m_elementCount = buffer->GetSize();
        state.m_stride = buffer->GetStride();
        state.m_present = true;
        return true;
    }

    void NativeHair::SerializeState(
        const State& state,
        AZStd::vector<AZ::u8>& data)
    {
        const auto append =
            [&data](const void* value, const size_t byteCount)
            {
                const size_t offset = data.size();
                data.resize(offset + byteCount);
                std::memcpy(data.data() + offset, value, byteCount);
            };
        append(&state.m_previousPosition, sizeof(state.m_previousPosition));
        append(&state.m_position, sizeof(state.m_position));
        append(&state.m_previousRotation, sizeof(state.m_previousRotation));
        append(&state.m_rotation, sizeof(state.m_rotation));
        const AZ::u8 teleported = state.m_teleported;
        append(&teleported, sizeof(teleported));
        SerializeBuffer(state.m_globalPoseTransforms, data);
        SerializeBuffer(state.m_positions, data);
        SerializeBuffer(state.m_previousPositions, data);
        SerializeBuffer(state.m_renderPositions, data);
        SerializeBuffer(state.m_scalpVertices, data);
        SerializeBuffer(state.m_targetGlobalPoseTransforms, data);
        SerializeBuffer(state.m_targetPositions, data);
        SerializeBuffer(state.m_velocities, data);
        SerializeBuffer(state.m_velocityAndDensity, data);
    }

    bool NativeHair::RestoreBuffer(
        JPH::Ref<JPH::ComputeBuffer>& buffer,
        const BufferState& state,
        JPH::ComputeSystem& computeSystem)
    {
        if (!state.m_present)
        {
            buffer = nullptr;
            return true;
        }
        if (state.m_stride == 0
            || state.m_elementCount > AZStd::numeric_limits<size_t>::max() / state.m_stride
            || state.m_data.size() != aznumeric_cast<size_t>(state.m_elementCount) * state.m_stride)
        {
            return false;
        }

        if (!buffer
            || buffer->GetSize() != state.m_elementCount
            || buffer->GetStride() != state.m_stride)
        {
            JPH::ComputeBufferResult result = computeSystem.CreateComputeBuffer(
                JPH::ComputeBuffer::EType::RWBuffer,
                state.m_elementCount,
                state.m_stride,
                state.m_data.data());
            if (result.HasError())
            {
                return false;
            }
            buffer = result.Get();
            return true;
        }

        void* destination = buffer->Map(JPH::ComputeBuffer::EMode::Write);
        if (!destination)
        {
            return false;
        }

        std::memcpy(destination, state.m_data.data(), state.m_data.size());
        buffer->Unmap();
        return true;
    }

    void NativeHair::SerializeBuffer(
        const BufferState& state,
        AZStd::vector<AZ::u8>& data)
    {
        const auto append =
            [&data](const void* value, const size_t byteCount)
            {
                const size_t offset = data.size();
                data.resize(offset + byteCount);
                std::memcpy(data.data() + offset, value, byteCount);
            };
        const AZ::u8 present = state.m_present;
        append(&present, sizeof(present));
        append(&state.m_elementCount, sizeof(state.m_elementCount));
        append(&state.m_stride, sizeof(state.m_stride));
        if (!state.m_data.empty())
        {
            append(state.m_data.data(), state.m_data.size());
        }
    }
} // namespace Jolt
