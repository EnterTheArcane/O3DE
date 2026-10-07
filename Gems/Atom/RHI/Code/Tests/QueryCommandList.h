/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Atom/RHI/CommandList.h>

namespace UnitTest
{
    // Query tests use the command list for its identity.
    class QueryCommandList final : public AZ::RHI::CommandList
    {
    public:
        void SetViewports(const AZ::RHI::Viewport*, uint32_t) override {}
        void SetScissors(const AZ::RHI::Scissor*, uint32_t) override {}
        void SetShaderResourceGroupForDraw(const AZ::RHI::DeviceShaderResourceGroup&) override {}
        void SetShaderResourceGroupForDispatch(const AZ::RHI::DeviceShaderResourceGroup&) override {}
        void Submit(const AZ::RHI::DeviceCopyItem&, uint32_t) override {}
        void Submit(const AZ::RHI::DeviceDrawItem&, uint32_t) override {}
        void Submit(const AZ::RHI::DeviceDispatchItem&, uint32_t) override {}
        void Submit(const AZ::RHI::DeviceDispatchRaysItem&, uint32_t) override {}
        void BeginPredication(const AZ::RHI::DeviceBuffer&, uint64_t, AZ::RHI::PredicationOp) override {}
        void EndPredication() override {}
        void BuildBottomLevelAccelerationStructure(const AZ::RHI::DeviceRayTracingBlas&) override {}
        void UpdateBottomLevelAccelerationStructure(const AZ::RHI::DeviceRayTracingBlas&) override {}
        void BuildClusterAccelerationStructures(const AZ::RHI::DeviceRayTracingClusterBlas&) override {}
        void BuildClusterBottomLevelAccelerationStructures(const AZStd::vector<const AZ::RHI::DeviceRayTracingClusterBlas*>&) override {}
        void QueryBlasCompactionSizes(
            const AZStd::vector<AZStd::pair<AZ::RHI::DeviceRayTracingBlas*, AZ::RHI::DeviceRayTracingCompactionQuery*>>&) override {}
        void CompactBottomLevelAccelerationStructure(const AZ::RHI::DeviceRayTracingBlas&, const AZ::RHI::DeviceRayTracingBlas&) override {}
        void BuildTopLevelAccelerationStructure(
            const AZ::RHI::DeviceRayTracingTlas&, const AZStd::vector<const AZ::RHI::DeviceRayTracingBlas*>&,
            const AZStd::vector<const AZ::RHI::DeviceRayTracingClusterBlas*>&) override {}
        void SetFragmentShadingRate(AZ::RHI::ShadingRate, const AZ::RHI::ShadingRateCombinators&) override {}
    };
}
