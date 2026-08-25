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

namespace AZ
{
    class JobContext;
} // namespace AZ

namespace JPH
{
    class ComputeBuffer;
    class ComputeBufferCPU;
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

        [[nodiscard]]
        AZ::u32 GetCachedWrapperCount() const;

        [[nodiscard]]
        AZ::u64 GetRetainedBytes() const;

        [[nodiscard]]
        AZ::u32 GetWorkerCount() const;

        [[nodiscard]]
        AZ::u64 GetWrapperCreationCount() const;

        [[nodiscard]]
        bool IsIdle() const;

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

        struct Binding final
        {
            const char* m_name = nullptr;
            void* m_data = nullptr;
            JPH::uint64 m_size = 0;
            JPH::RefConst<JPH::ComputeBuffer> m_buffer;
        };

        struct WrapperCacheEntry final
        {
            JPH::RefConst<JPH::ComputeShaderCPU> m_shader;
            JPH::ShaderWrapper* m_wrapper = nullptr;
            AZStd::vector<JPH::uint64> m_bindingHashes;
        };

        void BindBuffer(
            const char* name,
            const JPH::ComputeBufferCPU& buffer);

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
        WrapperCacheEntry* m_wrapperCacheEntry = nullptr;
        AZStd::vector<Binding> m_bindings;
        AZStd::vector<WrapperCacheEntry> m_wrapperCache;
        AZStd::vector<AZStd::unique_ptr<DispatchJob>> m_dispatchJobs;
        AZ::u64 m_wrapperCreationCount = 0;
    };
} // namespace Jolt
