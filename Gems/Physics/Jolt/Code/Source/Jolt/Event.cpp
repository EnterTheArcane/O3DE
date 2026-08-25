/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/EventInternal.h>

#include <Jolt/BehaviorReflection.h>

#include <AzCore/Debug/Trace.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Jolt
{
    namespace
    {
        EventBatchIdentitySource ModuleEventBatchIdentitySource;
    } // namespace

    EventBatch::EventBatch(EventBatchStorage* storage)
        : m_storage(storage)
    {
    }

    EventBatch::EventBatch(const EventBatch& other)
        : m_storage(other.m_storage)
    {
        EventBatchPool::AddReference(m_storage);
    }

    EventBatch::EventBatch(EventBatch&& other) noexcept
        : m_storage(other.m_storage)
    {
        other.m_storage = nullptr;
    }

    EventBatch::~EventBatch()
    {
        EventBatchPool::Release(m_storage);
    }

    EventBatch& EventBatch::operator=(const EventBatch& other)
    {
        if (this != &other)
        {
            EventBatchPool::AddReference(other.m_storage);
            EventBatchPool::Release(m_storage);
            m_storage = other.m_storage;
        }
        return *this;
    }

    EventBatch& EventBatch::operator=(EventBatch&& other) noexcept
    {
        if (this != &other)
        {
            EventBatchPool::Release(m_storage);
            m_storage = other.m_storage;
            other.m_storage = nullptr;
        }
        return *this;
    }

    EventBatch::operator bool() const
    {
        return m_storage;
    }

    AZ::u64 EventBatch::GetId() const
    {
        if (m_storage)
        {
            return m_storage->m_id;
        }

        return 0;
    }

    AZ::u64 EventBatch::GetSequence() const
    {
        if (m_storage)
        {
            return m_storage->m_sequence;
        }
        return 0;
    }

    AZStd::span<const ActivationEvent> EventBatch::GetActivations() const &
    {
        if (m_storage)
        {
            return m_storage->m_activations;
        }
        return {};
    }

    AZStd::span<const ContactEvent> EventBatch::GetContacts() const &
    {
        if (m_storage)
        {
            return m_storage->m_contacts;
        }
        return {};
    }

    AZStd::span<const BodyMoveEvent> EventBatch::GetBodyMoves() const &
    {
        if (m_storage)
        {
            return m_storage->m_bodyMoves;
        }
        return {};
    }

    AZStd::span<const VirtualCharacterMoveEvent> EventBatch::GetVirtualCharacterMoves() const &
    {
        if (m_storage)
        {
            return m_storage->m_virtualCharacterMoves;
        }
        return {};
    }

    AZStd::span<const ContactPoint> EventBatch::GetContactPoints(const ContactEvent& contact) const &
    {
        if (!m_storage
            || contact.m_batchId != m_storage->m_id
            || contact.m_firstPoint > m_storage->m_contactPoints.size()
            || contact.m_pointCount > m_storage->m_contactPoints.size() - contact.m_firstPoint)
        {
            return {};
        }

        return AZStd::span<const ContactPoint>(m_storage->m_contactPoints).subspan(
            contact.m_firstPoint,
            contact.m_pointCount);
    }

    EventBatchPool* EventBatchPool::Create()
    {
        return aznew EventBatchPool(ModuleEventBatchIdentitySource);
    }

    EventBatchPool::~EventBatchPool()
    {
        AZ_Assert(!m_acceptingStorage, "The event-batch pool must be shut down before destruction.");
        AZ_Assert(!m_freeStorage, "The event-batch pool retained storage during destruction.");
        AZ_Assert(m_liveBytes == 0, "The event-batch pool retained bytes during destruction.");
        AZ_Assert(m_liveCount == 0, "The event-batch pool retained live storage during destruction.");
        AZ_Assert(m_cachedBytes == 0, "The event-batch pool retained cached bytes during destruction.");
        AZ_Assert(m_cachedCount == 0, "The event-batch pool retained cached storage during destruction.");
    }

    EventBatch EventBatchPool::Acquire()
    {
        EventBatchStorage* storage = nullptr;
        AZ::u64 identity = 0;
        {
            AZStd::lock_guard lock(m_mutex);
            if (!m_acceptingStorage)
            {
                return {};
            }

            identity = m_identitySource.Acquire();
            if (identity == 0)
            {
                return {};
            }

            if (m_freeStorage)
            {
                storage = m_freeStorage;
                m_freeStorage = storage->m_nextFree;
                storage->m_nextFree = nullptr;
                m_cachedBytes -= storage->m_retainedBytes;
                --m_cachedCount;
            }
            else
            {
                storage = aznew EventBatchStorage(*this);
                storage->m_retainedBytes = storage->CalculateRetainedBytes();
                m_liveBytes += storage->m_retainedBytes;
                ++m_liveCount;
                UpdateHighWater();
                AddPoolReference();
            }
        }

        storage->Clear();
        storage->m_id = identity;
        storage->m_referenceCount.store(1, AZStd::memory_order_release);
        return EventBatch(storage);
    }

    PoolStatistics EventBatchPool::GetStatistics(
        const bool reset)
    {
        AZStd::lock_guard lock(m_mutex);
        PoolStatistics statistics{
            .m_liveBytes = m_liveBytes,
            .m_cachedBytes = m_cachedBytes,
            .m_outstandingBytes = m_liveBytes - m_cachedBytes,
            .m_highWaterBytes = m_highWaterBytes,
            .m_liveCount = m_liveCount,
            .m_cachedCount = m_cachedCount,
            .m_outstandingCount = m_liveCount - m_cachedCount,
            .m_highWaterCount = m_highWaterCount,
        };
        if (reset)
        {
            m_highWaterBytes = m_liveBytes;
            m_highWaterCount = m_liveCount;
        }
        return statistics;
    }

    bool EventBatchPool::Publish(
        EventBatch& batch,
        const AZ::u64 sequence,
        AZStd::vector<ContactEvent>& contacts,
        AZStd::vector<ContactPoint>& contactPoints,
        AZStd::vector<ActivationEvent>& activations,
        AZStd::vector<BodyMoveEvent>& bodyMoves,
        AZStd::vector<VirtualCharacterMoveEvent>& virtualCharacterMoves)
    {
        EventBatchStorage* storage = batch.m_storage;
        if (!storage || &storage->m_pool != this)
        {
            return false;
        }

        for (ContactEvent& event : contacts)
        {
            event.m_batchId = storage->m_id;
        }
        storage->m_contacts.swap(contacts);
        storage->m_contactPoints.swap(contactPoints);
        storage->m_activations.swap(activations);
        storage->m_bodyMoves.swap(bodyMoves);
        storage->m_virtualCharacterMoves.swap(virtualCharacterMoves);
        storage->m_sequence = sequence;
        UpdateRetainedBytes(storage);
        return true;
    }

    void EventBatchPool::Shutdown()
    {
        EventBatchStorage* freeStorage = nullptr;
        {
            AZStd::lock_guard lock(m_mutex);
            if (!m_acceptingStorage)
            {
                return;
            }

            m_acceptingStorage = false;
            freeStorage = m_freeStorage;
            m_freeStorage = nullptr;
            for (EventBatchStorage* storage = freeStorage; storage; storage = storage->m_nextFree)
            {
                m_liveBytes -= storage->m_retainedBytes;
                m_cachedBytes -= storage->m_retainedBytes;
                --m_liveCount;
                --m_cachedCount;
            }
        }

        while (freeStorage)
        {
            EventBatchStorage* storage = freeStorage;
            freeStorage = storage->m_nextFree;
            delete storage;
            ReleasePoolReference();
        }
        ReleasePoolReference();
    }

    void EventBatchPool::AddReference(EventBatchStorage* storage)
    {
        if (storage)
        {
            storage->m_referenceCount.fetch_add(1, AZStd::memory_order_relaxed);
        }
    }

    void EventBatchPool::Release(EventBatchStorage* storage)
    {
        if (storage && storage->m_referenceCount.fetch_sub(1, AZStd::memory_order_acq_rel) == 1)
        {
            storage->m_pool.Recycle(storage);
        }
    }

    void EventBatchPool::AddPoolReference()
    {
        m_referenceCount.fetch_add(1, AZStd::memory_order_relaxed);
    }

    void EventBatchPool::ReleasePoolReference()
    {
        if (m_referenceCount.fetch_sub(1, AZStd::memory_order_acq_rel) == 1)
        {
            delete this;
        }
    }

    void EventBatchPool::UpdateRetainedBytes(
        EventBatchStorage* storage)
    {
        if (!storage)
        {
            return;
        }

        const AZ::u64 retainedBytes = storage->CalculateRetainedBytes();
        AZStd::lock_guard lock(m_mutex);
        if (retainedBytes >= storage->m_retainedBytes)
        {
            m_liveBytes += retainedBytes - storage->m_retainedBytes;
        }
        else
        {
            m_liveBytes -= storage->m_retainedBytes - retainedBytes;
        }
        storage->m_retainedBytes = retainedBytes;
        UpdateHighWater();
    }

    void EventBatchPool::Recycle(EventBatchStorage* storage)
    {
        bool deleteStorage = false;
        {
            AZStd::lock_guard lock(m_mutex);
            if (m_acceptingStorage
                && m_cachedCount < MaximumCachedEventBatchCount
                && storage->m_retainedBytes <= MaximumCachedEventBatchStorageBytes
                && storage->m_retainedBytes <= MaximumCachedEventBatchBytes - m_cachedBytes)
            {
                storage->m_nextFree = m_freeStorage;
                m_freeStorage = storage;
                m_cachedBytes += storage->m_retainedBytes;
                ++m_cachedCount;
                return;
            }

            m_liveBytes -= storage->m_retainedBytes;
            --m_liveCount;
            deleteStorage = true;
        }

        if (deleteStorage)
        {
            delete storage;
            ReleasePoolReference();
        }
    }

    void EventBatchPool::UpdateHighWater()
    {
        if (m_liveBytes > m_highWaterBytes)
        {
            m_highWaterBytes = m_liveBytes;
        }
        if (m_liveCount > m_highWaterCount)
        {
            m_highWaterCount = m_liveCount;
        }
    }

    void EventBatchStorage::Clear()
    {
        m_contacts.clear();
        m_contactPoints.clear();
        m_activations.clear();
        m_bodyMoves.clear();
        m_virtualCharacterMoves.clear();
        m_id = 0;
        m_sequence = 0;
    }

    AZ::u64 EventBatchStorage::CalculateRetainedBytes() const
    {
        return sizeof(EventBatchStorage)
            + m_contacts.capacity() * sizeof(ContactEvent)
            + m_contactPoints.capacity() * sizeof(ContactPoint)
            + m_activations.capacity() * sizeof(ActivationEvent)
            + m_bodyMoves.capacity() * sizeof(BodyMoveEvent)
            + m_virtualCharacterMoves.capacity() * sizeof(VirtualCharacterMoveEvent);
    }

    void ReflectEvents(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<ContactPoint>()
                ->Field("PositionOnFirstBody", &ContactPoint::m_positionOnFirstBody)
                ->Field("PositionOnSecondBody", &ContactPoint::m_positionOnSecondBody);

            serializeContext
                ->Class<ContactEvent>()
                ->Field("FirstBodyHandle", &ContactEvent::m_firstBodyHandle)
                ->Field("SecondBodyHandle", &ContactEvent::m_secondBodyHandle)
                ->Field("FirstShapeHandle", &ContactEvent::m_firstShapeHandle)
                ->Field("SecondShapeHandle", &ContactEvent::m_secondShapeHandle)
                ->Field("FirstMaterialHandle", &ContactEvent::m_firstMaterialHandle)
                ->Field("SecondMaterialHandle", &ContactEvent::m_secondMaterialHandle)
                ->Field("FirstSubShapeId", &ContactEvent::m_firstSubShapeId)
                ->Field("SecondSubShapeId", &ContactEvent::m_secondSubShapeId)
                ->Field("BatchId", &ContactEvent::m_batchId)
                ->Field("Normal", &ContactEvent::m_normal)
                ->Field("PenetrationDepth", &ContactEvent::m_penetrationDepth)
                ->Field("FirstPoint", &ContactEvent::m_firstPoint)
                ->Field("PointCount", &ContactEvent::m_pointCount)
                ->Field("Phase", &ContactEvent::m_phase);

            serializeContext
                ->Class<ActivationEvent>()
                ->Field("BodyHandle", &ActivationEvent::m_bodyHandle)
                ->Field("State", &ActivationEvent::m_state);

            serializeContext
                ->Class<BodyMoveEvent>()
                ->Field("Transform", &BodyMoveEvent::m_transform)
                ->Field("BodyHandle", &BodyMoveEvent::m_bodyHandle)
                ->Field("EntityId", &BodyMoveEvent::m_entityId);

            serializeContext
                ->Class<VirtualCharacterMoveEvent>()
                ->Field("Transform", &VirtualCharacterMoveEvent::m_transform)
                ->Field("CharacterHandle", &VirtualCharacterMoveEvent::m_characterHandle)
                ->Field("EntityId", &VirtualCharacterMoveEvent::m_entityId);
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            JOLT_BEHAVIOR_ENUM(*behaviorContext, ActivationState, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, ActivationState, Active);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, ActivationState, Inactive);

            JOLT_BEHAVIOR_ENUM(*behaviorContext, EventPhase, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, EventPhase, Begin);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, EventPhase, End);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, EventPhase, Persist);

            JOLT_BEHAVIOR_ENUM(*behaviorContext, ContactDecision, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, ContactDecision, AcceptAllForPair);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, ContactDecision, Accept);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, ContactDecision, Reject);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, ContactDecision, RejectAllForPair);

            behaviorContext->Class<ContactPoint>("JoltContactPoint")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Alias, "ContactPoint")
                ->Attribute(AZ::Script::Attributes::ClassNameOverride, "ContactPoint")
                ->Property(
                    "positionOnFirstBody",
                    BehaviorValueGetter(&ContactPoint::m_positionOnFirstBody),
                    nullptr)
                ->Property(
                    "positionOnSecondBody",
                    BehaviorValueGetter(&ContactPoint::m_positionOnSecondBody),
                    nullptr);

            behaviorContext->Class<ContactEvent>("JoltContactEvent")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Alias, "ContactEvent")
                ->Attribute(AZ::Script::Attributes::ClassNameOverride, "ContactEvent")
                ->Property("firstBodyHandle", BehaviorValueGetter(&ContactEvent::m_firstBodyHandle), nullptr)
                ->Property("secondBodyHandle", BehaviorValueGetter(&ContactEvent::m_secondBodyHandle), nullptr)
                ->Property("firstShapeHandle", BehaviorValueGetter(&ContactEvent::m_firstShapeHandle), nullptr)
                ->Property("secondShapeHandle", BehaviorValueGetter(&ContactEvent::m_secondShapeHandle), nullptr)
                ->Property(
                    "firstMaterialHandle",
                    BehaviorValueGetter(&ContactEvent::m_firstMaterialHandle),
                    nullptr)
                ->Property(
                    "secondMaterialHandle",
                    BehaviorValueGetter(&ContactEvent::m_secondMaterialHandle),
                    nullptr)
                ->Property("firstSubShapeId", BehaviorValueGetter(&ContactEvent::m_firstSubShapeId), nullptr)
                ->Property("secondSubShapeId", BehaviorValueGetter(&ContactEvent::m_secondSubShapeId), nullptr)
                ->Property("batchId", BehaviorValueGetter(&ContactEvent::m_batchId), nullptr)
                ->Property("normal", BehaviorValueGetter(&ContactEvent::m_normal), nullptr)
                ->Property(
                    "penetrationDepth",
                    BehaviorValueGetter(&ContactEvent::m_penetrationDepth),
                    nullptr)
                ->Property("pointCount", BehaviorValueGetter(&ContactEvent::m_pointCount), nullptr)
                ->Property("phase", BehaviorValueGetter(&ContactEvent::m_phase), nullptr);

            behaviorContext->Class<ActivationEvent>("ActivationEvent")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("bodyHandle", BehaviorValueGetter(&ActivationEvent::m_bodyHandle), nullptr)
                ->Property("state", BehaviorValueGetter(&ActivationEvent::m_state), nullptr);

            behaviorContext->Class<ContactPointView>("ContactPointView")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Method("GetPointCount", &ContactPointView::GetPointCount)
                ->Method("GetPoint", &ContactPointView::GetPoint);
        }
    }
} // namespace Jolt
