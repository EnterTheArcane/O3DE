/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Operation.h>

#include <AzCore/Jobs/Job.h>
#include <AzCore/Jobs/JobEmpty.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/std/containers/fixed_vector.h>
#include <AzCore/std/optional.h>
#include <AzCore/std/parallel/atomic.h>
#include <AzCore/std/parallel/mutex.h>
#include <AzCore/std/utility/move.h>

namespace AZ
{
    class JobContext;
} // namespace AZ

namespace Jolt::Internal
{
    class OperationPool;

    class OperationRecord
    {
    public:
        AZ_CLASS_ALLOCATOR(OperationRecord, AZ::SystemAllocator);

        OperationRecord(
            OperationPool& pool,
            AZ::JobContext* jobContext,
            const void* typeKey);

        virtual ~OperationRecord() = default;

        AZ_DISABLE_COPY_MOVE(OperationRecord);

        void AddReference();

        [[nodiscard]]
        bool Cancel();

        [[nodiscard]]
        bool CanReap() const;

        [[nodiscard]]
        const void* GetResult() const;

        [[nodiscard]]
        OperationStatus GetStatus() const;

        void Join();

        void Prepare(bool runAsynchronously);

        void ReleaseReference();

        [[nodiscard]]
        bool RequiresJoin() const;

        void Start(bool runAsynchronously);

    protected:
        virtual void ClearForReuse() = 0;

        virtual bool ExecuteWork() = 0;

        void SetResultAddress(const void* result);

    private:
        class Job final
            : public AZ::Job
        {
        public:
            Job(
                OperationRecord& record,
                AZ::JobContext* jobContext);

        private:
            void Process() override;

            OperationRecord& m_record;
        };

        void Execute();

        void ReleaseTaskReference();

        friend class OperationPool;

        OperationPool& m_pool;
        AZStd::optional<Job> m_job;
        AZStd::optional<AZ::JobEmpty> m_completion;
        AZStd::atomic<AZ::u32> m_referenceCount{0};
        AZStd::atomic<OperationStatus> m_status{OperationStatus::None};
        AZStd::atomic_bool m_joinStarted{false};
        AZStd::atomic_bool m_joinComplete{false};
        AZStd::atomic_bool m_taskReferenceActive{false};
        const void* m_result = nullptr;

        OperationRecord* m_nextActive = nullptr;
        OperationRecord* m_nextFree = nullptr;
        const void* const m_typeKey;
    };

    template<class Result, class Work>
    class TypedOperationRecord final
        : public OperationRecord
    {
    public:
        AZ_CLASS_ALLOCATOR(TypedOperationRecord, AZ::SystemAllocator);

        using Executor = bool (*)(Work& work, Result& result);

        TypedOperationRecord(
            OperationPool& pool,
            AZ::JobContext* jobContext)
            : OperationRecord(pool, jobContext, GetTypeKey())
        {
        }

        void Configure(
            Work work,
            Executor executor,
            const bool runAsynchronously)
        {
            m_work = AZStd::move(work);
            if constexpr (requires(Result& result) { result.Reset(); })
            {
                m_result.Reset();
            }
            else
            {
                m_result = {};
            }
            m_executor = executor;
            Prepare(runAsynchronously);
        }

        [[nodiscard]]
        static const void* GetTypeKey()
        {
            return &s_typeKey;
        }

    protected:
        void ClearForReuse() override
        {
            m_work = {};
            if constexpr (requires(Result& result) { result.Reset(); })
            {
                m_result.Reset();
            }
            else
            {
                m_result = {};
            }
            m_executor = nullptr;
        }

        bool ExecuteWork() override
        {
            SetResultAddress(&m_result);
            return m_executor && m_executor(m_work, m_result);
        }

    private:
        static inline const AZ::u8 s_typeKey = 0;

        Work m_work;
        Result m_result;
        Executor m_executor = nullptr;
    };

    class OperationPool final
    {
    public:
        AZ_CLASS_ALLOCATOR(OperationPool, AZ::SystemAllocator);

        [[nodiscard]]
        static OperationPool* Create(AZ::JobContext* jobContext);

        template<class Result, class Work>
        [[nodiscard]]
        Operation<Result> CreateOperation(
            Work work,
            typename TypedOperationRecord<Result, Work>::Executor executor)
        {
            ReapCompleted();

            using Record = TypedOperationRecord<Result, Work>;
            OperationRecord* storage = AcquireStorage(Record::GetTypeKey());
            Record* record = nullptr;
            if (storage)
            {
                record = static_cast<Record*>(storage);
            }
            else
            {
                record = aznew Record(*this, m_jobContext);
                AddPoolReference();
            }

            const bool runAsynchronously = m_jobContext;
            record->Configure(AZStd::move(work), executor, runAsynchronously);
            Publish(record);
            record->Start(runAsynchronously);
            return Operation<Result>(record);
        }

        void Drain();

        void Shutdown();

        static void Recycle(OperationRecord* record);

    private:
        struct FreeList final
        {
            const void* m_typeKey = nullptr;
            OperationRecord* m_record = nullptr;
        };

        explicit OperationPool(AZ::JobContext* jobContext);
        ~OperationPool();

        AZ_DISABLE_COPY_MOVE(OperationPool);

        void AddPoolReference();

        [[nodiscard]]
        OperationRecord* AcquireStorage(const void* typeKey);

        void Publish(OperationRecord* record);

        void ReapCompleted();

        void ReleasePoolReference();

        void RecycleRecord(OperationRecord* record);

        AZ::JobContext* m_jobContext = nullptr;
        AZStd::mutex m_mutex;
        AZStd::fixed_vector<FreeList, 16> m_freeLists;
        OperationRecord* m_activeRecords = nullptr;
        AZStd::atomic<AZ::u32> m_referenceCount{1};
        bool m_acceptingOperations = true;
    };
} // namespace Jolt::Internal
