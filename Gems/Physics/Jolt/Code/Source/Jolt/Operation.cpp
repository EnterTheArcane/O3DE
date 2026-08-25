/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/OperationInternal.h>

#include <Jolt/FloatEnvironment.h>
#include <Jolt/Profiler.h>

#include <AzCore/Debug/Trace.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/parallel/lock.h>
#include <AzCore/std/parallel/thread.h>

namespace Jolt::Internal
{
    OperationRecord::Job::Job(
        OperationRecord& record,
        AZ::JobContext* jobContext)
        : AZ::Job(false, jobContext)
        , m_record(record)
    {
    }

    void OperationRecord::Job::Process()
    {
        const DeterministicFloatScope floatScope;
        m_record.Execute();
    }

    OperationRecord::OperationRecord(
        OperationPool& pool,
        AZ::JobContext* jobContext,
        const void* typeKey)
        : m_pool(pool)
        , m_typeKey(typeKey)
    {
        if (jobContext)
        {
            m_job.emplace(*this, jobContext);
            m_completion.emplace(false, jobContext);
        }
    }

    void OperationRecord::AddReference()
    {
        m_referenceCount.fetch_add(1, AZStd::memory_order_relaxed);
    }

    bool OperationRecord::Cancel()
    {
        OperationStatus expected = OperationStatus::Pending;
        return m_status.compare_exchange_strong(
            expected,
            OperationStatus::Canceled,
            AZStd::memory_order_acq_rel,
            AZStd::memory_order_acquire);
    }

    bool OperationRecord::CanReapWithoutWaiting() const
    {
        const OperationStatus status = GetStatus();
        return m_referenceCount.load(AZStd::memory_order_acquire) == 1
            && m_taskReferenceActive.load(AZStd::memory_order_acquire)
            && m_completion
            && m_completion->GetDependentCount() == 1
            && (status == OperationStatus::Succeeded
                || status == OperationStatus::Failed
                || status == OperationStatus::Canceled);
    }

    const void* OperationRecord::GetResult() const
    {
        const OperationStatus status = GetStatus();
        if (status == OperationStatus::Succeeded || status == OperationStatus::Failed)
        {
            return m_result;
        }
        return nullptr;
    }

    OperationStatus OperationRecord::GetStatus() const
    {
        return m_status.load(AZStd::memory_order_acquire);
    }

    AZ::u64 OperationRecord::GetRetainedBytes() const
    {
        return m_retainedBytes.load(AZStd::memory_order_acquire);
    }

    void OperationRecord::Join()
    {
        if (!m_taskReferenceActive.load(AZStd::memory_order_acquire))
        {
            return;
        }

        bool expected = false;
        if (m_joinStarted.compare_exchange_strong(
                expected,
                true,
                AZStd::memory_order_acq_rel,
                AZStd::memory_order_acquire))
        {
            m_completion->StartAndWaitForCompletion();
            m_joinComplete.store(true, AZStd::memory_order_release);
            ReleaseTaskReference();
            return;
        }

        while (!m_joinComplete.load(AZStd::memory_order_acquire))
        {
            AZStd::this_thread::yield();
        }
    }

    void OperationRecord::Prepare(
        const bool runAsynchronously)
    {
        if (runAsynchronously)
        {
            m_job->Reset(true);
            m_completion->Reset(true);
        }
        m_referenceCount.store(2, AZStd::memory_order_release);
        m_status.store(OperationStatus::Pending, AZStd::memory_order_release);
        m_joinStarted.store(!runAsynchronously, AZStd::memory_order_release);
        m_joinComplete.store(!runAsynchronously, AZStd::memory_order_release);
        m_taskReferenceActive.store(true, AZStd::memory_order_release);
        m_result = nullptr;
        if (runAsynchronously)
        {
            m_job->SetDependent(&*m_completion);
        }
    }

    void OperationRecord::ReleaseReference()
    {
        const AZ::u32 previousReferenceCount = m_referenceCount.fetch_sub(1, AZStd::memory_order_acq_rel);
        if (previousReferenceCount == 1)
        {
            OperationPool::Recycle(this);
            return;
        }
        if (previousReferenceCount == 2)
        {
            RequestReap();
        }
    }

    bool OperationRecord::RequiresJoin() const
    {
        return m_taskReferenceActive.load(AZStd::memory_order_acquire);
    }

    void OperationRecord::Start(
        const bool runAsynchronously)
    {
        if (runAsynchronously)
        {
            m_job->Start();
            return;
        }

        Execute();
        ReleaseTaskReference();
    }

    void OperationRecord::SetResultAddress(
        const void* result)
    {
        m_result = result;
    }

    void OperationRecord::Execute()
    {
        OperationStatus expected = OperationStatus::Pending;
        if (!m_status.compare_exchange_strong(
                expected,
                OperationStatus::Running,
                AZStd::memory_order_acq_rel,
                AZStd::memory_order_acquire))
        {
            AZ_Assert(expected == OperationStatus::Canceled, "A Jolt operation entered an invalid state.");
            RequestReap();
            return;
        }

        JOLT_PROFILE_SCOPE(Physics, "Jolt::Operation");
        if (ExecuteWork())
        {
            m_status.store(OperationStatus::Succeeded, AZStd::memory_order_release);
        }
        else
        {
            m_status.store(OperationStatus::Failed, AZStd::memory_order_release);
        }
        RequestReap();
    }

    void OperationRecord::RequestReap()
    {
        const OperationStatus status = GetStatus();
        if (m_referenceCount.load(AZStd::memory_order_acquire) == 1
            && m_taskReferenceActive.load(AZStd::memory_order_acquire)
            && (status == OperationStatus::Succeeded
                || status == OperationStatus::Failed
                || status == OperationStatus::Canceled))
        {
            m_pool.QueueForReap(this);
        }
    }

    void OperationRecord::ReleaseTaskReference()
    {
        if (m_taskReferenceActive.exchange(false, AZStd::memory_order_acq_rel))
        {
            ReleaseReference();
        }
    }

    void OperationRecord::RefreshRetainedBytes()
    {
        m_pool.UpdateRecordRetainedBytes(this);
    }

    OperationPool* OperationPool::Create(
        AZ::JobContext* jobContext)
    {
        return aznew OperationPool(jobContext);
    }

    OperationPool::OperationPool(
        AZ::JobContext* jobContext)
        : m_jobContext(jobContext)
    {
    }

    OperationPool::~OperationPool()
    {
        AZ_Assert(!m_acceptingOperations, "The operation pool must be shut down before destruction.");
        AZ_Assert(!m_activeRecords, "The operation pool retained active operations during destruction.");
        AZ_Assert(!m_reapCandidates, "The operation pool retained completed reap candidates during destruction.");
        AZ_Assert(m_freeLists.empty(), "The operation pool retained reusable operations during destruction.");
        AZ_Assert(m_liveBytes.load(AZStd::memory_order_relaxed) == 0, "The operation pool retained bytes during destruction.");
        AZ_Assert(m_liveCount.load(AZStd::memory_order_relaxed) == 0, "The operation pool retained records during destruction.");
        AZ_Assert(m_cachedBytes == 0, "The operation pool retained cached bytes during destruction.");
        AZ_Assert(m_cachedCount == 0, "The operation pool retained cached records during destruction.");
    }

    void OperationPool::Drain()
    {
        while (true)
        {
            OperationRecord* record = nullptr;
            {
                AZStd::lock_guard lock(m_mutex);
                for (OperationRecord* candidate = m_activeRecords; candidate; candidate = candidate->m_nextActive)
                {
                    if (candidate->RequiresJoin())
                    {
                        RemoveReapCandidate(candidate);
                        candidate->AddReference();
                        record = candidate;
                        break;
                    }
                }
            }

            if (!record)
            {
                return;
            }

            record->Join();
            record->ReleaseReference();
        }
    }

    PoolStatistics OperationPool::GetStatistics(
        const bool reset)
    {
        AZStd::lock_guard lock(m_mutex);
        const AZ::u64 liveBytes = m_liveBytes.load(AZStd::memory_order_acquire);
        const AZ::u32 liveCount = m_liveCount.load(AZStd::memory_order_acquire);
        PoolStatistics statistics{
            .m_liveBytes = liveBytes,
            .m_cachedBytes = m_cachedBytes,
            .m_outstandingBytes = liveBytes - m_cachedBytes,
            .m_highWaterBytes = AZStd::max(liveBytes, m_highWaterBytes.load(AZStd::memory_order_acquire)),
            .m_liveCount = liveCount,
            .m_cachedCount = m_cachedCount,
            .m_outstandingCount = liveCount - m_cachedCount,
            .m_highWaterCount = AZStd::max(liveCount, m_highWaterCount.load(AZStd::memory_order_acquire)),
        };
        if (reset)
        {
            m_highWaterBytes.store(liveBytes, AZStd::memory_order_release);
            m_highWaterCount.store(liveCount, AZStd::memory_order_release);
            UpdateHighWater();
        }
        return statistics;
    }

    void OperationPool::Shutdown()
    {
        OperationRecord* freeRecords = nullptr;
        {
            AZStd::lock_guard lock(m_mutex);
            if (!m_acceptingOperations)
            {
                return;
            }

            m_acceptingOperations = false;
            for (FreeList& freeList : m_freeLists)
            {
                while (freeList.m_record)
                {
                    OperationRecord* record = freeList.m_record;
                    freeList.m_record = record->m_nextFree;
                    record->m_nextFree = freeRecords;
                    freeRecords = record;
                    freeList.m_cachedBytes -= record->GetRetainedBytes();
                    --freeList.m_cachedCount;
                    m_cachedBytes -= record->GetRetainedBytes();
                    --m_cachedCount;
                    UnregisterRecord(record);
                }
                AZ_Assert(freeList.m_cachedBytes == 0, "A Jolt operation free list retained bytes during shutdown.");
                AZ_Assert(freeList.m_cachedCount == 0, "A Jolt operation free list retained records during shutdown.");
            }
            m_freeLists.clear();
        }

        while (freeRecords)
        {
            OperationRecord* record = freeRecords;
            freeRecords = record->m_nextFree;
            record->m_nextFree = nullptr;
            delete record;
            ReleasePoolReference();
        }
        ReleasePoolReference();
    }

    void OperationPool::Recycle(
        OperationRecord* record)
    {
        record->m_pool.RecycleRecord(record);
    }

    void OperationPool::AddPoolReference()
    {
        m_referenceCount.fetch_add(1, AZStd::memory_order_relaxed);
    }

    OperationRecord* OperationPool::AcquireStorage(
        const void* typeKey)
    {
        AZStd::lock_guard lock(m_mutex);
        if (!m_acceptingOperations)
        {
            return nullptr;
        }

        for (FreeList& freeList : m_freeLists)
        {
            if (freeList.m_typeKey == typeKey)
            {
                OperationRecord* record = freeList.m_record;
                if (record)
                {
                    freeList.m_record = record->m_nextFree;
                    record->m_nextFree = nullptr;
                    freeList.m_cachedBytes -= record->GetRetainedBytes();
                    --freeList.m_cachedCount;
                    m_cachedBytes -= record->GetRetainedBytes();
                    --m_cachedCount;
                }
                return record;
            }
        }

        AZ_Assert(m_freeLists.size() < m_freeLists.capacity(), "The Jolt operation-kind capacity is exhausted.");
        m_freeLists.push_back({.m_typeKey = typeKey});
        return nullptr;
    }

    void OperationPool::Publish(
        OperationRecord* record)
    {
        AZStd::lock_guard lock(m_mutex);
        AZ_Assert(m_acceptingOperations, "A Jolt operation was published while the pool was shutting down.");
        record->m_nextActive = m_activeRecords;
        record->m_previousActiveLink = &m_activeRecords;
        if (m_activeRecords)
        {
            m_activeRecords->m_previousActiveLink = &record->m_nextActive;
        }
        m_activeRecords = record;
    }

    void OperationPool::ReapCompleted()
    {
        OperationRecord* readyRecords = nullptr;
        {
            AZStd::lock_guard lock(m_mutex);
            OperationRecord* candidate = m_reapCandidates;
            while (candidate)
            {
                OperationRecord* nextCandidate = candidate->m_nextReap;
                if (candidate->CanReapWithoutWaiting())
                {
                    RemoveReapCandidate(candidate);
                    candidate->m_nextReap = readyRecords;
                    readyRecords = candidate;
                }
                candidate = nextCandidate;
            }
        }

        while (readyRecords)
        {
            OperationRecord* record = readyRecords;
            readyRecords = record->m_nextReap;
            record->m_nextReap = nullptr;
            record->Join();
        }
    }

    void OperationPool::QueueForReap(
        OperationRecord* record)
    {
        AZStd::lock_guard lock(m_mutex);
        if (record->m_previousReapLink)
        {
            return;
        }

        record->m_nextReap = m_reapCandidates;
        record->m_previousReapLink = &m_reapCandidates;
        if (m_reapCandidates)
        {
            m_reapCandidates->m_previousReapLink = &record->m_nextReap;
        }
        m_reapCandidates = record;
    }

    void OperationPool::ReleasePoolReference()
    {
        if (m_referenceCount.fetch_sub(1, AZStd::memory_order_acq_rel) == 1)
        {
            delete this;
        }
    }

    void OperationPool::RecycleRecord(
        OperationRecord* record)
    {
        bool deleteRecord = false;
        {
            AZStd::lock_guard lock(m_mutex);
            AZ_Assert(record->m_previousActiveLink, "A Jolt operation was not owned by its pool.");
            RemoveActiveRecord(record);
            RemoveReapCandidate(record);
            record->ClearForReuse();

            if (m_acceptingOperations)
            {
                [[maybe_unused]] bool foundFreeList = false;
                for (FreeList& freeList : m_freeLists)
                {
                    if (freeList.m_typeKey == record->m_typeKey)
                    {
                        foundFreeList = true;
                        const AZ::u64 retainedBytes = record->GetRetainedBytes();
                        if (freeList.m_cachedCount < MaximumCachedOperationCountPerType
                            && retainedBytes <= MaximumCachedOperationRecordBytes
                            && retainedBytes <= MaximumCachedOperationBytesPerType - freeList.m_cachedBytes)
                        {
                            record->m_nextFree = freeList.m_record;
                            freeList.m_record = record;
                            freeList.m_cachedBytes += retainedBytes;
                            ++freeList.m_cachedCount;
                            m_cachedBytes += retainedBytes;
                            ++m_cachedCount;
                            return;
                        }
                        break;
                    }
                }
                AZ_Assert(foundFreeList, "The Jolt operation free list is missing its record type.");
            }
            UnregisterRecord(record);
            deleteRecord = true;
        }

        if (deleteRecord)
        {
            delete record;
            ReleasePoolReference();
        }
    }

    void OperationPool::RegisterRecord(
        OperationRecord* record)
    {
        const AZ::u64 retainedBytes = record->CalculateRetainedBytes();
        record->m_retainedBytes.store(retainedBytes, AZStd::memory_order_release);
        m_liveBytes.fetch_add(retainedBytes, AZStd::memory_order_acq_rel);
        m_liveCount.fetch_add(1, AZStd::memory_order_acq_rel);
        UpdateHighWater();
    }

    void OperationPool::RemoveActiveRecord(
        OperationRecord* record)
    {
        if (!record->m_previousActiveLink)
        {
            return;
        }

        *record->m_previousActiveLink = record->m_nextActive;
        if (record->m_nextActive)
        {
            record->m_nextActive->m_previousActiveLink = record->m_previousActiveLink;
        }
        record->m_nextActive = nullptr;
        record->m_previousActiveLink = nullptr;
    }

    void OperationPool::RemoveReapCandidate(
        OperationRecord* record)
    {
        if (!record->m_previousReapLink)
        {
            return;
        }

        *record->m_previousReapLink = record->m_nextReap;
        if (record->m_nextReap)
        {
            record->m_nextReap->m_previousReapLink = record->m_previousReapLink;
        }
        record->m_nextReap = nullptr;
        record->m_previousReapLink = nullptr;
    }

    void OperationPool::UnregisterRecord(
        OperationRecord* record)
    {
        const AZ::u64 retainedBytes = record->m_retainedBytes.exchange(0, AZStd::memory_order_acq_rel);
        m_liveBytes.fetch_sub(retainedBytes, AZStd::memory_order_acq_rel);
        m_liveCount.fetch_sub(1, AZStd::memory_order_acq_rel);
    }

    void OperationPool::UpdateHighWater()
    {
        const AZ::u64 liveBytes = m_liveBytes.load(AZStd::memory_order_acquire);
        AZ::u64 highWaterBytes = m_highWaterBytes.load(AZStd::memory_order_relaxed);
        while (liveBytes > highWaterBytes
            && !m_highWaterBytes.compare_exchange_weak(
                highWaterBytes,
                liveBytes,
                AZStd::memory_order_release,
                AZStd::memory_order_relaxed))
        {
        }

        const AZ::u32 liveCount = m_liveCount.load(AZStd::memory_order_acquire);
        AZ::u32 highWaterCount = m_highWaterCount.load(AZStd::memory_order_relaxed);
        while (liveCount > highWaterCount
            && !m_highWaterCount.compare_exchange_weak(
                highWaterCount,
                liveCount,
                AZStd::memory_order_release,
                AZStd::memory_order_relaxed))
        {
        }
    }

    void OperationPool::UpdateRecordRetainedBytes(
        OperationRecord* record)
    {
        const AZ::u64 retainedBytes = record->CalculateRetainedBytes();
        const AZ::u64 previousBytes = record->m_retainedBytes.exchange(retainedBytes, AZStd::memory_order_acq_rel);
        if (retainedBytes >= previousBytes)
        {
            m_liveBytes.fetch_add(retainedBytes - previousBytes, AZStd::memory_order_acq_rel);
            UpdateHighWater();
            return;
        }
        m_liveBytes.fetch_sub(previousBytes - retainedBytes, AZStd::memory_order_acq_rel);
    }

    bool CancelOperation(
        OperationRecord* record)
    {
        return record && record->Cancel();
    }

    const void* GetOperationResult(
        const OperationRecord* record)
    {
        if (record)
        {
            return record->GetResult();
        }
        return nullptr;
    }

    OperationStatus GetOperationStatus(
        const OperationRecord* record)
    {
        if (record)
        {
            return record->GetStatus();
        }
        return OperationStatus::None;
    }

    void ReleaseOperation(
        OperationRecord* record)
    {
        if (record)
        {
            record->ReleaseReference();
        }
    }

    OperationStatus WaitForOperation(
        OperationRecord* record)
    {
        if (!record)
        {
            return OperationStatus::None;
        }

        record->Join();
        return record->GetStatus();
    }
} // namespace Jolt::Internal
