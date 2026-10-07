/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <AzCore/UnitTest/TestTypes.h>
#include <Atom/RHI/CommandList.h>
#include <Atom/RHI/DeviceQuery.h>
#include <Atom/RHI/DeviceQueryPool.h>
#include <AzCore/Memory/SystemAllocator.h>

namespace UnitTest
{
    // Query tests only track command-list identity; no rendering commands are submitted.
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

    class Query
        : public AZ::RHI::DeviceQuery
    {
    public:
        AZ_CLASS_ALLOCATOR(Query, AZ::SystemAllocator);
        
    private:
        AZ::RHI::ResultCode BeginInternal(AZ::RHI::CommandList& commandList, AZ::RHI::QueryControlFlags flags) override;
        AZ::RHI::ResultCode EndInternal(AZ::RHI::CommandList& commandList) override;
        AZ::RHI::ResultCode WriteTimestampInternal(AZ::RHI::CommandList& commandList) override;
    };

    class QueryPool
        : public AZ::RHI::DeviceQueryPool
    {
    public:
        AZ_CLASS_ALLOCATOR(QueryPool, AZ::SystemAllocator);

        AZStd::vector<AZ::RHI::Interval> m_calledIntervals;

    private:
        AZ::RHI::ResultCode InitInternal(AZ::RHI::Device& device, const AZ::RHI::QueryPoolDescriptor& descriptor) override;
        AZ::RHI::ResultCode InitQueryInternal(AZ::RHI::DeviceQuery& query) override;
        AZ::RHI::ResultCode GetResultsInternal(uint32_t startIndex, uint32_t queryCount, uint64_t* results, uint32_t resultsCount, AZ::RHI::QueryResultFlagBits flags) override;
    };
}
