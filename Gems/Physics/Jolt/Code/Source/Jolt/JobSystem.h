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
#include <AzCore/std/parallel/condition_variable.h>

#include <Jolt/Jolt.h>
#include <Jolt/Core/FixedSizeFreeList.h>
#include <Jolt/Core/JobSystemWithBarrier.h>

namespace AZ
{
    class JobContext;
} // namespace AZ

namespace Jolt
{
    class JobSystem final
        : public JPH::JobSystemWithBarrier
    {
    public:
        struct UpdateStatistics final
        {
            AZ::u64 m_executionNanoseconds = 0;
            AZ::u64 m_maximumQueueLatencyNanoseconds = 0;
            AZ::u64 m_queueLatencyNanoseconds = 0;
            AZ::u32 m_jobCount = 0;
            AZ::u32 m_maximumTaskCount = 0;
            AZ::u32 m_taskCount = 0;
        };

        JobSystem(
            AZ::u32 maximumJobCount,
            AZ::u32 maximumBarrierCount,
            AZ::u32 workerCount,
            AZ::JobContext* jobContext);

        ~JobSystem() override;

        AZ_DISABLE_COPY_MOVE(JobSystem);

        void BeginUpdate(bool collectTimings);

        [[nodiscard]]
        UpdateStatistics EndUpdate();

        int GetMaxConcurrency() const override;

        void WaitForJobs(Barrier* barrier) override;

        JobHandle CreateJob(
            const char* name,
            JPH::ColorArg color,
            const JobFunction& function,
            JPH::uint32 dependencyCount) override;

    protected:
        void QueueJob(Job* job) override;

        void QueueJobs(
            Job** jobs,
            JPH::uint jobCount) override;

        void FreeJob(Job* job) override;

    private:
        class ProviderJob final
            : public Job
        {
        public:
            ProviderJob(
                const char* name,
                JPH::ColorArg color,
                JobSystem* jobSystem,
                const JobFunction& function,
                JPH::uint32 dependencyCount);

            [[nodiscard]]
            const char* GetProfileName() const;

            AZ::u64 m_queuedNanoseconds = 0;

        private:
            const char* m_name = nullptr;
        };

        class Task final
            : public AZ::Job
        {
        public:
            AZ_CLASS_ALLOCATOR(Task, AZ::ThreadPoolAllocator);

            Task(
                JobSystem& jobSystem,
                AZ::JobContext* jobContext);

        protected:
            void Process() override;

        private:
            JobSystem& m_jobSystem;
        };

        using AvailableJobs = JPH::FixedSizeFreeList<ProviderJob>;

        AvailableJobs m_jobs;
        AZStd::vector<ProviderJob*> m_queuedJobs;
        AZ::JobContext* m_jobContext = nullptr;
        AZStd::condition_variable m_taskCondition;
        AZStd::mutex m_taskMutex;
        size_t m_queueReadIndex = 0;
        size_t m_queueWriteIndex = 0;
        size_t m_queuedJobCount = 0;
        UpdateStatistics m_updateStatistics;
        AZ::u32 m_workerCount = 1;
        AZ::u32 m_activeTaskCount = 0;
        bool m_collectUpdateStatistics = false;
        bool m_collectUpdateTimings = false;
        bool m_stopTasks = false;
    };
} // namespace Jolt
