/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/JobSystem.h>

#include <Jolt/FloatEnvironment.h>
#include <Jolt/Profiler.h>

#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/Debug/Trace.h>
#include <AzCore/Jobs/JobContext.h>
#include <AzCore/Jobs/JobManager.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/chrono/chrono.h>
#include <AzCore/std/createdestroy.h>

#include <cstddef>

namespace Jolt
{
    namespace
    {
        [[nodiscard]]
        AZ::u64 GetSteadyNanoseconds()
        {
            return static_cast<AZ::u64>(
                AZStd::chrono::duration_cast<AZStd::chrono::nanoseconds>(
                    AZStd::chrono::steady_clock::now().time_since_epoch())
                    .count());
        }
    } // namespace

    JobSystem::ProviderJob::ProviderJob(
        const char* name,
        const JPH::ColorArg color,
        JobSystem* jobSystem,
        const JobFunction& function,
        const JPH::uint32 dependencyCount)
        : Job(name, color, jobSystem, function, dependencyCount)
        , m_name(name)
    {
    }

    const char* JobSystem::ProviderJob::GetProfileName() const
    {
        if (m_name)
        {
            return m_name;
        }

        return "Jolt job";
    }

    JobSystem::Task::Task(
        JobSystem& jobSystem,
        AZ::JobContext* jobContext)
        : AZ::Job(true, jobContext)
        , m_jobSystem(jobSystem)
    {
    }

    void JobSystem::Task::operator delete(
        void* pointer) noexcept
    {
        static_assert(offsetof(TaskSlot, m_storage) == 0);
        auto* slot = reinterpret_cast<TaskSlot*>(pointer);
        slot->m_jobSystem->RecycleTaskSlot(slot);
    }

    void JobSystem::Task::Process()
    {
        const DeterministicFloatScope floatScope;
        while (true)
        {
            ProviderJob* job = nullptr;
            bool collectTimings = false;
            AZ::u64 queueLatencyNanoseconds = 0;
            {
                AZStd::unique_lock lock(m_jobSystem.m_taskMutex);
                if (m_jobSystem.m_queuedJobCount == 0)
                {
                    AZ_Assert(
                        m_jobSystem.m_activeTaskCount > 0,
                        "The Jolt task completion count is inconsistent.");
                    --m_jobSystem.m_activeTaskCount;
                    if (m_jobSystem.m_activeTaskCount == 0)
                    {
                        AZ_Assert(
                            m_jobSystem.m_queueReadIndex == m_jobSystem.m_queueWriteIndex,
                            "The Jolt task queue indices are inconsistent.");
                    }
                    return;
                }

                job = m_jobSystem.m_queuedJobs[m_jobSystem.m_queueReadIndex];
                m_jobSystem.m_queuedJobs[m_jobSystem.m_queueReadIndex] = nullptr;
                ++m_jobSystem.m_queueReadIndex;
                if (m_jobSystem.m_queueReadIndex == m_jobSystem.m_queuedJobs.size())
                {
                    m_jobSystem.m_queueReadIndex = 0;
                }
                --m_jobSystem.m_queuedJobCount;
                collectTimings = m_jobSystem.m_collectUpdateTimings;
                if (collectTimings)
                {
                    queueLatencyNanoseconds = GetSteadyNanoseconds() - job->m_queuedNanoseconds;
                }
            }

            AZ::u64 executionStartNanoseconds = 0;
            if (collectTimings)
            {
                executionStartNanoseconds = GetSteadyNanoseconds();
            }
            {
                JOLT_PROFILE_SCOPE(Physics, job->GetProfileName());
                job->Execute();
            }
            if (collectTimings)
            {
                const AZ::u64 executionNanoseconds = GetSteadyNanoseconds() - executionStartNanoseconds;
                AZStd::lock_guard lock(m_jobSystem.m_taskMutex);
                m_jobSystem.m_updateStatistics.m_executionNanoseconds += executionNanoseconds;
                m_jobSystem.m_updateStatistics.m_queueLatencyNanoseconds += queueLatencyNanoseconds;
                m_jobSystem.m_updateStatistics.m_maximumQueueLatencyNanoseconds = AZStd::max(
                    m_jobSystem.m_updateStatistics.m_maximumQueueLatencyNanoseconds,
                    queueLatencyNanoseconds);
            }
            job->Release();
        }
    }

    JobSystem::JobSystem(
        const AZ::u32 maximumJobCount,
        const AZ::u32 maximumBarrierCount,
        const AZ::u32 workerCount,
        AZ::JobContext* jobContext)
        : JPH::JobSystemWithBarrier(maximumBarrierCount)
        , m_jobContext(jobContext)
    {
        m_jobs.Init(maximumJobCount, maximumJobCount);
        m_queuedJobs.resize(maximumJobCount);
        if (m_jobContext)
        {
            m_taskCompletion.emplace(false, jobContext);
            const AZ::u32 availableWorkerCount = m_jobContext->GetJobManager().GetNumWorkerThreads() + 1;
            m_workerCount = AZStd::min(
                AZStd::max(workerCount, AZ::u32{1}),
                availableWorkerCount);
            const AZ::u32 maximumBackgroundTaskCount = m_workerCount - 1;
            // AZ::Job invokes the delete hook after Process returns, so retain one replacement slot per retiring task.
            m_taskSlots.resize(maximumBackgroundTaskCount * 2);
            for (TaskSlot& slot : m_taskSlots)
            {
                slot.m_jobSystem = this;
                slot.m_next = m_freeTaskSlots;
                m_freeTaskSlots = &slot;
            }
        }
    }

    JobSystem::~JobSystem()
    {
        AZ_Assert(IsIdle(), "Jolt work remains active during job-system destruction.");
        [[maybe_unused]] size_t freeTaskCount = 0;
        for (TaskSlot* slot = m_freeTaskSlots; slot; slot = slot->m_next)
        {
            ++freeTaskCount;
        }
        AZ_Assert(freeTaskCount == m_taskSlots.size(), "The Jolt job system retained task slots during destruction.");
    }

    void JobSystem::BeginUpdate(
        const bool collectTimings)
    {
        AZStd::lock_guard lock(m_taskMutex);
        AZ_Assert(!m_collectUpdateStatistics, "Jolt job statistics collection is already active.");
        m_updateStatistics = {};
        m_collectUpdateStatistics = true;
        m_collectUpdateTimings = collectTimings;
    }

    JobSystem::UpdateStatistics JobSystem::EndUpdate()
    {
        AZStd::lock_guard lock(m_taskMutex);
        AZ_Assert(m_collectUpdateStatistics, "Jolt job statistics collection is not active.");
        m_collectUpdateStatistics = false;
        m_collectUpdateTimings = false;
        return m_updateStatistics;
    }

    int JobSystem::GetMaxConcurrency() const
    {
        return static_cast<int>(m_workerCount);
    }

    AZ::u64 JobSystem::GetRetainedBytes() const
    {
        return m_queuedJobs.capacity() * sizeof(ProviderJob*)
            + m_taskSlots.capacity() * sizeof(TaskSlot);
    }

    AZ::u32 JobSystem::GetTaskCapacity() const
    {
        return m_workerCount - 1;
    }

    bool JobSystem::IsIdle()
    {
        AZStd::lock_guard lock(m_taskMutex);
        return m_activeTaskCount == 0 && m_queuedJobCount == 0;
    }

    void JobSystem::WaitForJobs(
        Barrier* barrier)
    {
        JobSystemWithBarrier::WaitForJobs(barrier);
        if (m_taskCompletion)
        {
            m_taskCompletion->StartAndWaitForCompletion();
            m_taskCompletion->Reset(true);
        }
        AZStd::lock_guard lock(m_taskMutex);
        AZ_Assert(m_activeTaskCount == 0, "The Jolt task join completed with active tasks remaining.");
        AZ_Assert(m_queuedJobCount == 0, "The Jolt task join completed with queued jobs remaining.");
    }

    JPH::JobHandle JobSystem::CreateJob(
        const char* name,
        const JPH::ColorArg color,
        const JobFunction& function,
        const JPH::uint32 dependencyCount)
    {
        const JPH::uint32 index = m_jobs.ConstructObject(
            name,
            color,
            this,
            function,
            dependencyCount);
        if (index == AvailableJobs::cInvalidObjectIndex)
        {
            AZ_Error("Jolt", false, "The configured Jolt job capacity is exhausted.");
            return {};
        }

        ProviderJob& job = m_jobs.Get(index);
        JobHandle handle(&job);
        if (dependencyCount == 0)
        {
            QueueJob(&job);
        }

        return handle;
    }

    void JobSystem::QueueJob(
        Job* job)
    {
        Job* jobs[]{job};
        QueueJobs(jobs, 1);
    }

    void JobSystem::QueueJobs(
        Job** jobs,
        const JPH::uint jobCount)
    {
        if (m_workerCount <= 1)
        {
            return;
        }

        AZ::u32 taskCount = 0;
        {
            AZStd::lock_guard lock(m_taskMutex);
            for (JPH::uint jobIndex = 0; jobIndex < jobCount; ++jobIndex)
            {
                AZ_Assert(
                    m_queuedJobCount < m_queuedJobs.size(),
                    "The Jolt ready-job queue capacity is exhausted.");
                ProviderJob* providerJob = static_cast<ProviderJob*>(jobs[jobIndex]);
                providerJob->AddRef();
                if (m_collectUpdateTimings)
                {
                    providerJob->m_queuedNanoseconds = GetSteadyNanoseconds();
                }
                m_queuedJobs[m_queueWriteIndex] = providerJob;
                ++m_queueWriteIndex;
                if (m_queueWriteIndex == m_queuedJobs.size())
                {
                    m_queueWriteIndex = 0;
                }
                ++m_queuedJobCount;
            }

            const AZ::u32 maximumBackgroundTaskCount = m_workerCount - 1;
            const AZ::u32 availableTaskCount = maximumBackgroundTaskCount - m_activeTaskCount;
            taskCount = AZStd::min(
                aznumeric_cast<AZ::u32>(m_queuedJobCount),
                availableTaskCount);
            m_activeTaskCount += taskCount;
            if (m_collectUpdateStatistics)
            {
                m_updateStatistics.m_jobCount += jobCount;
                m_updateStatistics.m_maximumTaskCount = AZStd::max(
                    m_updateStatistics.m_maximumTaskCount,
                    m_activeTaskCount);
                m_updateStatistics.m_taskCount += taskCount;
            }
        }

        AZ_Assert(taskCount == 0 || m_taskCompletion, "Parallel Jolt tasks require a job completion context.");
        for (AZ::u32 taskIndex = 0; taskIndex < taskCount; ++taskIndex)
        {
            Task* task = CreateTask();
            task->SetDependent(&*m_taskCompletion);
            task->Start();
        }
    }

    void JobSystem::FreeJob(
        Job* job)
    {
        m_jobs.DestructObject(static_cast<ProviderJob*>(job));
    }

    JobSystem::Task* JobSystem::CreateTask()
    {
        TaskSlot* slot = nullptr;
        {
            AZStd::lock_guard lock(m_taskMutex);
            AZ_Assert(m_freeTaskSlots, "The Jolt background-task pool is exhausted.");
            slot = m_freeTaskSlots;
            m_freeTaskSlots = slot->m_next;
            slot->m_next = nullptr;
        }
        return AZStd::construct_at(
            reinterpret_cast<Task*>(slot->m_storage),
            *this,
            m_jobContext);
    }

    void JobSystem::RecycleTaskSlot(
        TaskSlot* slot)
    {
        AZStd::lock_guard lock(m_taskMutex);
        slot->m_next = m_freeTaskSlots;
        m_freeTaskSlots = slot;
    }
} // namespace Jolt
