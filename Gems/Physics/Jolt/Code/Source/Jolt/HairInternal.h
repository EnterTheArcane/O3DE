/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>

#include <Jolt/Jolt.h>
#include <Jolt/Compute/ComputeBuffer.h>
#include <Jolt/Physics/Hair/Hair.h>

namespace Jolt
{
    class NativeHair final
        : public JPH::Hair
    {
    public:
        using JPH::Hair::Hair;

        [[nodiscard]]
        const JPH::Mat44& GetScalpToHeadTransform() const;

        struct BufferState final
        {
            AZStd::vector<AZ::u8> m_data;
            AZ::u64 m_elementCount = 0;
            AZ::u32 m_stride = 0;
            bool m_present = false;
        };

        struct State final
        {
            JPH::RVec3 m_previousPosition;
            JPH::RVec3 m_position;
            JPH::Quat m_previousRotation;
            JPH::Quat m_rotation;

            BufferState m_globalPoseTransforms;
            BufferState m_positions;
            BufferState m_previousPositions;
            BufferState m_renderPositions;
            BufferState m_scalpVertices;
            BufferState m_targetGlobalPoseTransforms;
            BufferState m_targetPositions;
            BufferState m_velocities;
            BufferState m_velocityAndDensity;

            bool m_teleported = true;
        };

        [[nodiscard]]
        bool CaptureState(State& state) const;

        [[nodiscard]]
        bool RestoreState(
            const State& state,
            JPH::ComputeSystem& computeSystem);

        static void SerializeState(
            const State& state,
            AZStd::vector<AZ::u8>& data);

    private:
        [[nodiscard]]
        static bool CaptureBuffer(
            JPH::ComputeBuffer* buffer,
            BufferState& state);

        [[nodiscard]]
        static bool RestoreBuffer(
            JPH::Ref<JPH::ComputeBuffer>& buffer,
            const BufferState& state,
            JPH::ComputeSystem& computeSystem);

        static void SerializeBuffer(
            const BufferState& state,
            AZStd::vector<AZ::u8>& data);
    };
} // namespace Jolt
