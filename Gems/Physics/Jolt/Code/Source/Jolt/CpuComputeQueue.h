/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/Jobs/Job.h>
#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

#include <Jolt/Jolt.h>
#include <Jolt/Compute/ComputeQueue.h>
#include <Jolt/Core/Reference.h>
#include <Jolt/Core/UnorderedSet.h>

namespace AZ
{
    class JobContext;
} // namespace AZ

namespace JPH
{
    class ComputeBuffer;
    class ComputeShaderCPU;
    class ShaderWrapper;
} // namespace JPH

namespace Jolt
{
    class CpuComputeQueue final
        : public JPH::ComputeQueue
    {
    public:
        CpuComputeQueue(
            AZ::JobContext* jobContext,
            AZ::u32 workerCount);

        ~CpuComputeQueue() override;

        AZ_DISABLE_COPY_MOVE(CpuComputeQueue);

        void SetShader(const JPH::ComputeShader* shader) override;

        void SetConstantBuffer(
            const char* name,
            const JPH::ComputeBuffer* buffer) override;

        void SetBuffer(
            const char* name,
            const JPH::ComputeBuffer* buffer) override;

        void SetRWBuffer(
            const char* name,
            JPH::ComputeBuffer* buffer,
            EBarrier barrier = EBarrier::Yes) override;

        void ScheduleReadback(
            JPH::ComputeBuffer* destination,
            const JPH::ComputeBuffer* source) override;

        void Dispatch(
            JPH::uint threadGroupCountX,
            JPH::uint threadGroupCountY,
            JPH::uint threadGroupCountZ) override;

        void Execute() override;

        void Wait() override;

    private:
        class DispatchJob;

        static void ProcessRange(
            JPH::ShaderWrapper& wrapper,
            size_t threadCountX,
            size_t threadCountY,
            size_t threadCountZ,
            size_t beginIndex,
            size_t endIndex);

        AZ::JobContext* m_jobContext = nullptr;
        AZ::u32 m_workerCount = 1;
        JPH::RefConst<JPH::ComputeShaderCPU> m_shader;
        JPH::ShaderWrapper* m_wrapper = nullptr;
        JPH::UnorderedSet<JPH::RefConst<JPH::ComputeBuffer>> m_usedBuffers;
        AZStd::vector<AZStd::unique_ptr<DispatchJob>> m_dispatchJobs;
    };
} // namespace Jolt
