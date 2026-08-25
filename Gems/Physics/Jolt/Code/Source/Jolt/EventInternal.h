/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Diagnostics.h>
#include <Jolt/Event.h>

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/limits.h>
#include <AzCore/std/parallel/atomic.h>
#include <AzCore/std/parallel/mutex.h>

namespace Jolt
{
    inline constexpr AZ::u32 MaximumCachedEventBatchCount = 4;
    inline constexpr AZ::u64 MaximumCachedEventBatchBytes = 8 * 1024 * 1024;
    inline constexpr AZ::u64 MaximumCachedEventBatchStorageBytes = 4 * 1024 * 1024;

    class EventBatchIdentitySource final
    {
    public:
        explicit EventBatchIdentitySource(const AZ::u64 firstIdentity = 1) noexcept
            : m_nextIdentity(firstIdentity)
        {
        }

        AZ_DISABLE_COPY_MOVE(EventBatchIdentitySource);

        [[nodiscard]]
        AZ::u64 Acquire() noexcept
        {
            AZ::u64 identity = m_nextIdentity.load(AZStd::memory_order_relaxed);
            while (identity != 0)
            {
                AZ::u64 nextIdentity = 0;
                if (identity != AZStd::numeric_limits<AZ::u64>::max())
                {
                    nextIdentity = identity + 1;
                }
                if (m_nextIdentity.compare_exchange_weak(
                    identity,
                    nextIdentity,
                    AZStd::memory_order_relaxed,
                    AZStd::memory_order_relaxed))
                {
                    return identity;
                }
            }

            return 0;
        }

    private:
        AZStd::atomic<AZ::u64> m_nextIdentity;
    };

    class JOLT_API EventBatchPool final
    {
    public:
        AZ_CLASS_ALLOCATOR(EventBatchPool, AZ::SystemAllocator);

        [[nodiscard]]
        static EventBatchPool* Create();

        [[nodiscard]]
        EventBatch Acquire();

        [[nodiscard]]
        PoolStatistics GetStatistics(bool reset);

        bool Publish(
            EventBatch& batch,
            AZ::u64 sequence,
            AZStd::vector<ContactEvent>& contacts,
            AZStd::vector<ContactPoint>& contactPoints,
            AZStd::vector<ActivationEvent>& activations,
            AZStd::vector<BodyMoveEvent>& bodyMoves,
            AZStd::vector<VirtualCharacterMoveEvent>& virtualCharacterMoves);

        void Shutdown();

        void UpdateRetainedBytes(EventBatchStorage* storage);

        static void AddReference(EventBatchStorage* storage);

        static void Release(EventBatchStorage* storage);

    private:
        explicit EventBatchPool(EventBatchIdentitySource& identitySource)
            : m_identitySource(identitySource)
        {
        }

        ~EventBatchPool();

        AZ_DISABLE_COPY_MOVE(EventBatchPool);

        void AddPoolReference();

        void ReleasePoolReference();

        void Recycle(EventBatchStorage* storage);

        void UpdateHighWater();

        EventBatchIdentitySource& m_identitySource;
        AZStd::mutex m_mutex;
        EventBatchStorage* m_freeStorage = nullptr;
        AZStd::atomic<AZ::u32> m_referenceCount{1};
        AZ::u64 m_liveBytes = 0;
        AZ::u64 m_cachedBytes = 0;
        AZ::u64 m_highWaterBytes = 0;
        AZ::u32 m_liveCount = 0;
        AZ::u32 m_cachedCount = 0;
        AZ::u32 m_highWaterCount = 0;
        bool m_acceptingStorage = true;
    };

    struct EventBatchStorage final
    {
        AZ_CLASS_ALLOCATOR(EventBatchStorage, AZ::SystemAllocator);

        explicit EventBatchStorage(EventBatchPool& pool)
            : m_pool(pool)
        {
        }

        void Clear();

        [[nodiscard]]
        AZ::u64 CalculateRetainedBytes() const;

        EventBatchPool& m_pool;
        AZStd::atomic<AZ::u32> m_referenceCount{0};
        EventBatchStorage* m_nextFree = nullptr;
        AZ::u64 m_retainedBytes = 0;

        AZStd::vector<ContactEvent> m_contacts;
        AZStd::vector<ContactPoint> m_contactPoints;
        AZStd::vector<ActivationEvent> m_activations;
        AZStd::vector<BodyMoveEvent> m_bodyMoves;
        AZStd::vector<VirtualCharacterMoveEvent> m_virtualCharacterMoves;
        AZ::u64 m_id = 0;
        AZ::u64 m_sequence = 0;
    };
} // namespace Jolt
