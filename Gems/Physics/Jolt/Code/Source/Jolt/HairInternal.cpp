/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/HairInternal.h>

#include <AzCore/std/limits.h>
#include <AzCore/Casting/numeric_cast.h>

#include <cstring>

namespace Jolt
{
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
