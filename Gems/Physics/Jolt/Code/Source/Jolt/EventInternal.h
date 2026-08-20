/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Event.h>

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/parallel/atomic.h>
#include <AzCore/std/parallel/mutex.h>

namespace Jolt
{
    class EventBatchPool final
    {
    public:
        AZ_CLASS_ALLOCATOR(EventBatchPool, AZ::SystemAllocator);

        [[nodiscard]]
        static EventBatchPool* Create();

        [[nodiscard]]
        EventBatch Acquire();

        void Shutdown();

        static void AddReference(EventBatchStorage* storage);

        static void Release(EventBatchStorage* storage);

    private:
        EventBatchPool() = default;
        ~EventBatchPool();

        AZ_DISABLE_COPY_MOVE(EventBatchPool);

        void AddPoolReference();

        void ReleasePoolReference();

        void Recycle(EventBatchStorage* storage);

        AZStd::mutex m_mutex;
        EventBatchStorage* m_freeStorage = nullptr;
        AZStd::atomic<AZ::u32> m_referenceCount{1};
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

        EventBatchPool& m_pool;
        AZStd::atomic<AZ::u32> m_referenceCount{0};
        EventBatchStorage* m_nextFree = nullptr;

        AZStd::vector<ContactEvent> m_contacts;
        AZStd::vector<ContactPoint> m_contactPoints;
        AZStd::vector<ActivationEvent> m_activations;
        AZStd::vector<BodyMoveEvent> m_bodyMoves;
        AZStd::vector<VirtualCharacterMoveEvent> m_virtualCharacterMoves;
        AZ::u64 m_sequence = 0;
    };
} // namespace Jolt
