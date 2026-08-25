/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/CpuComputeQueue.h>

#include <Jolt/FloatEnvironment.h>
#include <Jolt/Profiler.h>

#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/Debug/Trace.h>
#include <AzCore/Jobs/JobEmpty.h>
#include <AzCore/std/algorithm.h>

#include <Jolt/Core/HashCombine.h>
#include <Jolt/Compute/CPU/ComputeBufferCPU.h>
#include <Jolt/Compute/CPU/ComputeShaderCPU.h>
#include <Jolt/Compute/CPU/HLSLToCPP.h>
#include <Jolt/Compute/CPU/ShaderWrapper.h>

namespace Jolt
{
    namespace
    {
        constexpr size_t MinimumThreadsPerJob = 64;
        constexpr size_t ExpectedBindingCount = 16;
        constexpr size_t MaximumCachedShaderWrapperCount = 16;
    } // namespace

    class CpuComputeQueue::DispatchJob final
        : public AZ::Job
    {
    public:
        explicit DispatchJob(
            AZ::JobContext* jobContext)
            : AZ::Job(false, jobContext)
        {
        }

        void Configure(
            JPH::ShaderWrapper& wrapper,
            const size_t threadCountX,
            const size_t threadCountY,
            const size_t threadCountZ,
            const size_t beginIndex,
            const size_t endIndex,
            AZ::Job& dependent)
        {
            Reset(true);
            m_wrapper = &wrapper;
            m_threadCountX = threadCountX;
            m_threadCountY = threadCountY;
            m_threadCountZ = threadCountZ;
            m_beginIndex = beginIndex;
            m_endIndex = endIndex;
            SetDependent(&dependent);
        }

    private:
        void Process() override
        {
            JOLT_PROFILE_SCOPE(Physics, "Jolt::CpuComputeQueue::Worker");
            const DeterministicFloatScope floatScope;
            ProcessRange(
                *m_wrapper,
                m_threadCountX,
                m_threadCountY,
                m_threadCountZ,
                m_beginIndex,
                m_endIndex);
        }

        JPH::ShaderWrapper* m_wrapper = nullptr;
        size_t m_threadCountX = 0;
        size_t m_threadCountY = 0;
        size_t m_threadCountZ = 0;
        size_t m_beginIndex = 0;
        size_t m_endIndex = 0;
    };

    CpuComputeQueue::CpuComputeQueue(
        AZ::JobContext* jobContext,
        const AZ::u32 workerCount)
        : m_jobContext(jobContext)
    {
        if (m_jobContext)
        {
            const AZ::u32 availableWorkerCount = m_jobContext->GetJobManager().GetNumWorkerThreads() + 1;
            m_workerCount = AZStd::min(
                AZStd::max(workerCount, AZ::u32{1}),
                availableWorkerCount);
            m_dispatchJobs.reserve(m_workerCount - 1);
        }
        m_bindings.reserve(ExpectedBindingCount);
        m_wrapperCache.reserve(MaximumCachedShaderWrapperCount);
    }

    CpuComputeQueue::~CpuComputeQueue()
    {
        AZ_Assert(IsIdle(), "A Jolt CPU compute dispatch was left incomplete.");
        for (WrapperCacheEntry& entry : m_wrapperCache)
        {
            delete entry.m_wrapper;
        }
    }

    AZ::u32 CpuComputeQueue::GetCachedWrapperCount() const
    {
        return aznumeric_cast<AZ::u32>(m_wrapperCache.size());
    }

    AZ::u64 CpuComputeQueue::GetRetainedBytes() const
    {
        AZ::u64 retainedBytes = sizeof(CpuComputeQueue)
            + m_bindings.capacity() * sizeof(Binding)
            + m_wrapperCache.capacity() * sizeof(WrapperCacheEntry)
            + m_dispatchJobs.capacity() * sizeof(AZStd::unique_ptr<DispatchJob>);
        for (const WrapperCacheEntry& entry : m_wrapperCache)
        {
            retainedBytes += entry.m_wrapper->GetSize();
            retainedBytes += entry.m_bindingHashes.capacity() * sizeof(JPH::uint64);
        }
        retainedBytes += m_dispatchJobs.size() * sizeof(DispatchJob);
        return retainedBytes;
    }

    AZ::u32 CpuComputeQueue::GetWorkerCount() const
    {
        return m_workerCount;
    }

    AZ::u64 CpuComputeQueue::GetWrapperCreationCount() const
    {
        return m_wrapperCreationCount;
    }

    bool CpuComputeQueue::IsIdle() const
    {
        return !m_shader && !m_wrapper;
    }

    void CpuComputeQueue::SetShader(
        const JPH::ComputeShader* shader)
    {
        AZ_Assert(!m_shader && !m_wrapper, "A Jolt CPU compute shader is already active.");
        m_shader = static_cast<const JPH::ComputeShaderCPU*>(shader);
        m_wrapperCacheEntry = nullptr;
        for (WrapperCacheEntry& entry : m_wrapperCache)
        {
            if (entry.m_shader.GetPtr() == m_shader.GetPtr())
            {
                m_wrapper = entry.m_wrapper;
                m_wrapperCacheEntry = &entry;
                return;
            }
        }

        m_wrapper = m_shader->CreateWrapper();
        ++m_wrapperCreationCount;
        if (m_wrapperCache.size() < MaximumCachedShaderWrapperCount)
        {
            m_wrapperCache.push_back({
                .m_shader = m_shader,
                .m_wrapper = m_wrapper,
            });
            m_wrapperCacheEntry = &m_wrapperCache.back();
        }
    }

    void CpuComputeQueue::SetConstantBuffer(
        const char* name,
        const JPH::ComputeBuffer* buffer)
    {
        if (!buffer)
        {
            return;
        }

        AZ_Assert(
            buffer->GetType() == JPH::ComputeBuffer::EType::ConstantBuffer,
            "A Jolt CPU compute constant binding received an incompatible buffer.");
        const auto* cpuBuffer = static_cast<const JPH::ComputeBufferCPU*>(buffer);
        BindBuffer(name, *cpuBuffer);
    }

    void CpuComputeQueue::SetBuffer(
        const char* name,
        const JPH::ComputeBuffer* buffer)
    {
        if (!buffer)
        {
            return;
        }

        AZ_Assert(
            buffer->GetType() == JPH::ComputeBuffer::EType::UploadBuffer
                || buffer->GetType() == JPH::ComputeBuffer::EType::Buffer
                || buffer->GetType() == JPH::ComputeBuffer::EType::RWBuffer,
            "A Jolt CPU compute binding received an incompatible buffer.");
        const auto* cpuBuffer = static_cast<const JPH::ComputeBufferCPU*>(buffer);
        BindBuffer(name, *cpuBuffer);
    }

    void CpuComputeQueue::SetRWBuffer(
        const char* name,
        JPH::ComputeBuffer* buffer,
        [[maybe_unused]] const EBarrier barrier)
    {
        if (!buffer)
        {
            return;
        }

        AZ_Assert(
            buffer->GetType() == JPH::ComputeBuffer::EType::RWBuffer,
            "A Jolt CPU compute read-write binding received an incompatible buffer.");
        const auto* cpuBuffer = static_cast<const JPH::ComputeBufferCPU*>(buffer);
        BindBuffer(name, *cpuBuffer);
    }

    void CpuComputeQueue::ScheduleReadback(
        [[maybe_unused]] JPH::ComputeBuffer* destination,
        [[maybe_unused]] const JPH::ComputeBuffer* source)
    {
    }

    void CpuComputeQueue::Dispatch(
        const JPH::uint threadGroupCountX,
        const JPH::uint threadGroupCountY,
        const JPH::uint threadGroupCountZ)
    {
        JOLT_PROFILE_SCOPE(Physics, "Jolt::CpuComputeQueue::Dispatch");
        if (m_wrapperCacheEntry)
        {
            bool bindingSignatureMatches = m_wrapperCacheEntry->m_bindingHashes.size() == m_bindings.size();
            if (bindingSignatureMatches)
            {
                for (size_t bindingIndex = 0; bindingIndex < m_bindings.size(); ++bindingIndex)
                {
                    if (m_wrapperCacheEntry->m_bindingHashes[bindingIndex] != JPH::HashString(m_bindings[bindingIndex].m_name))
                    {
                        bindingSignatureMatches = false;
                        break;
                    }
                }
            }

            if (m_wrapperCacheEntry->m_bindingHashes.empty())
            {
                m_wrapperCacheEntry->m_bindingHashes.reserve(m_bindings.size());
                for (const Binding& binding : m_bindings)
                {
                    m_wrapperCacheEntry->m_bindingHashes.push_back(JPH::HashString(binding.m_name));
                }
            }
            else if (!bindingSignatureMatches)
            {
                delete m_wrapperCacheEntry->m_wrapper;
                m_wrapperCacheEntry->m_wrapper = m_shader->CreateWrapper();
                ++m_wrapperCreationCount;
                m_wrapper = m_wrapperCacheEntry->m_wrapper;
                m_wrapperCacheEntry->m_bindingHashes.clear();
                m_wrapperCacheEntry->m_bindingHashes.reserve(m_bindings.size());
                for (const Binding& binding : m_bindings)
                {
                    m_wrapper->Bind(binding.m_name, binding.m_data, binding.m_size);
                    m_wrapperCacheEntry->m_bindingHashes.push_back(JPH::HashString(binding.m_name));
                }
            }
        }

        const size_t threadCountX = static_cast<size_t>(threadGroupCountX) * m_shader->GetGroupSizeX();
        const size_t threadCountY = static_cast<size_t>(threadGroupCountY) * m_shader->GetGroupSizeY();
        const size_t threadCountZ = static_cast<size_t>(threadGroupCountZ) * m_shader->GetGroupSizeZ();
        const size_t threadCount = threadCountX * threadCountY * threadCountZ;
        size_t partitionCount = 1;
        if (threadCount > 0)
        {
            const size_t maximumPartitionCount = (threadCount - 1) / MinimumThreadsPerJob + 1;
            partitionCount = AZStd::min(
                static_cast<size_t>(m_workerCount),
                maximumPartitionCount);
        }
        if (partitionCount <= 1)
        {
            ProcessRange(
                *m_wrapper,
                threadCountX,
                threadCountY,
                threadCountZ,
                0,
                threadCount);
        }
        else
        {
            AZ::JobEmpty completion(false, m_jobContext);
            const size_t backgroundJobCount = partitionCount - 1;
            while (m_dispatchJobs.size() < backgroundJobCount)
            {
                m_dispatchJobs.push_back(AZStd::make_unique<DispatchJob>(m_jobContext));
            }

            const size_t threadsPerPartition = threadCount / partitionCount;
            const size_t longerPartitionCount = threadCount % partitionCount;
            size_t beginIndex = threadsPerPartition;
            if (longerPartitionCount > 0)
            {
                ++beginIndex;
            }
            for (size_t jobIndex = 0; jobIndex < backgroundJobCount; ++jobIndex)
            {
                const size_t partitionIndex = jobIndex + 1;
                size_t partitionThreadCount = threadsPerPartition;
                if (partitionIndex < longerPartitionCount)
                {
                    ++partitionThreadCount;
                }
                const size_t endIndex = beginIndex + partitionThreadCount;
                DispatchJob& job = *m_dispatchJobs[jobIndex];
                job.Configure(
                    *m_wrapper,
                    threadCountX,
                    threadCountY,
                    threadCountZ,
                    beginIndex,
                    endIndex,
                    completion);
                job.Start();
                beginIndex = endIndex;
            }

            size_t callerThreadCount = threadsPerPartition;
            if (longerPartitionCount > 0)
            {
                ++callerThreadCount;
            }
            ProcessRange(
                *m_wrapper,
                threadCountX,
                threadCountY,
                threadCountZ,
                0,
                callerThreadCount);
            completion.StartAndWaitForCompletion();
        }

        if (!m_wrapperCacheEntry)
        {
            delete m_wrapper;
        }
        m_wrapper = nullptr;
        m_wrapperCacheEntry = nullptr;
        m_bindings.clear();
        m_shader = nullptr;
    }

    void CpuComputeQueue::Execute()
    {
    }

    void CpuComputeQueue::Wait()
    {
    }

    void CpuComputeQueue::BindBuffer(
        const char* name,
        const JPH::ComputeBufferCPU& buffer)
    {
        void* data = buffer.GetData();
        const JPH::uint64 size = buffer.GetSize() * buffer.GetStride();
        m_wrapper->Bind(name, data, size);
        m_bindings.push_back({
            .m_name = name,
            .m_data = data,
            .m_size = size,
            .m_buffer = &buffer,
        });
    }

    void CpuComputeQueue::ProcessRange(
        JPH::ShaderWrapper& wrapper,
        const size_t threadCountX,
        const size_t threadCountY,
        const size_t threadCountZ,
        const size_t beginIndex,
        const size_t endIndex)
    {
        if (threadCountY == 1 && threadCountZ == 1)
        {
            JPH::HLSLToCPP::uint3 threadId{
                aznumeric_cast<JPH::uint>(beginIndex),
                0,
                0,
            };
            for (size_t threadIndex = beginIndex; threadIndex < endIndex; ++threadIndex)
            {
                wrapper.Main(threadId);
                ++threadId.x;
            }
            return;
        }

        const size_t threadCountXY = threadCountX * threadCountY;
        for (size_t threadIndex = beginIndex; threadIndex < endIndex; ++threadIndex)
        {
            const size_t z = threadIndex / threadCountXY;
            const size_t xyIndex = threadIndex - z * threadCountXY;
            const size_t y = xyIndex / threadCountX;
            const size_t x = xyIndex - y * threadCountX;
            const JPH::HLSLToCPP::uint3 threadId{
                aznumeric_cast<JPH::uint>(x),
                aznumeric_cast<JPH::uint>(y),
                aznumeric_cast<JPH::uint>(z),
            };
            wrapper.Main(threadId);
        }
    }
} // namespace Jolt

#undef wxyz
#undef xwyz
#undef xy
#undef xywz
#undef xyz
#undef xzy
#undef yx
#undef yxz
#undef yzx
#undef zxy
#undef zyx
