/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Symbol/Internal/SymbolTable.h>

#include <AzCore/Debug/Trace.h>
#include <AzCore/Symbol/Internal/SymbolFailure.h>
#include <AzCore/Symbol/Internal/SymbolGroup.h>
#include <AzCore/Symbol/Symbol.h>
#include <AzCore/Utils/NoDestructor.h>
#include <AzCore/std/createdestroy.h>
#include <AzCore/std/parallel/lock.h>

#include <cstring>
#include <limits>
#include <new>

namespace AZ::Internal
{
    constexpr size_t SymbolStorageBudgetBytes = size_t{1024} * 1024 * 1024;

    namespace
    {
        constexpr u8 SymbolTableEmptyControl = 0x80;
        constexpr size_t SymbolTableInvalidSlot = static_cast<size_t>(-1);

        [[nodiscard]]
        bool EntriesMatch(
            const SymbolEntry& entry,
            const AZStd::string_view value,
            const u64 hash)
        {
            if (entry.m_hash != hash || entry.m_size != value.size())
            {
                return false;
            }
            return std::memcmp(entry.GetData(), value.data(), value.size()) == 0;
        }

        [[nodiscard]]
        size_t FirstSetLane(const u16 mask)
        {
            for (size_t lane = 0; lane < SymbolGroup::Width; ++lane)
            {
                if ((mask & static_cast<u16>(1u << lane)) != 0)
                {
                    return lane;
                }
            }
            return SymbolTableInvalidSlot;
        }

        [[nodiscard]]
        bool AllocateTableStorage(
            SymbolAllocator& allocator,
            SymbolStorageBudget& storageBudget,
            const size_t capacity,
            u8*& controls,
            const SymbolEntry**& slots)
        {
            if (capacity > (std::numeric_limits<size_t>::max)() / sizeof(SymbolEntry*))
            {
                return false;
            }

            const size_t slotByteSize = capacity * sizeof(SymbolEntry*);
            if (capacity > (std::numeric_limits<size_t>::max)() - slotByteSize)
            {
                return false;
            }

            const size_t storageByteSize = capacity + slotByteSize;
            if (!storageBudget.TryReserve(storageByteSize))
            {
                return false;
            }

            u8* newControls = reinterpret_cast<u8*>(allocator.Allocate(capacity, SymbolGroup::Width, "AZ::Symbol controls"));
            const SymbolEntry** newSlots = reinterpret_cast<const SymbolEntry**>(
                allocator.Allocate(slotByteSize, alignof(SymbolEntry*), "AZ::Symbol slots"));

            if (!newControls || !newSlots)
            {
                if (newControls)
                {
                    allocator.Deallocate(newControls, capacity, SymbolGroup::Width);
                }
                if (newSlots)
                {
                    allocator.Deallocate(newSlots, slotByteSize, alignof(SymbolEntry*));
                }
                storageBudget.Release(storageByteSize);
                return false;
            }

            std::memset(newControls, SymbolTableEmptyControl, capacity);
            std::memset(newSlots, 0, slotByteSize);
            controls = newControls;
            slots = newSlots;
            return true;
        }
    } // namespace

    SymbolTable::SymbolTable()
        : m_storageBudget{SymbolStorageBudgetBytes}
    {
        for (Shard& shard : m_shards)
        {
            shard.m_arena.SetStorage(m_allocator, m_storageBudget);
        }
    }

    SymbolTable::SymbolTable(AZ::IAllocator& allocator)
        : m_allocator{allocator}
        , m_storageBudget{(std::numeric_limits<size_t>::max)()}
    {
        for (Shard& shard : m_shards)
        {
            shard.m_arena.SetStorage(m_allocator, m_storageBudget);
        }
    }

    SymbolTable::SymbolTable(
        AZ::IAllocator& allocator,
        const size_t storageBudgetBytes)
        : m_allocator{allocator}
        , m_storageBudget{storageBudgetBytes}
    {
        for (Shard& shard : m_shards)
        {
            shard.m_arena.SetStorage(m_allocator, m_storageBudget);
        }
    }

    SymbolTable::~SymbolTable()
    {
        for (Shard& shard : m_shards)
        {
            if (!shard.m_controls)
            {
                continue;
            }

            m_allocator.Deallocate(shard.m_controls, shard.m_capacity, SymbolGroup::Width);
            m_allocator.Deallocate(shard.m_slots, shard.m_capacity * sizeof(SymbolEntry*), alignof(SymbolEntry*));
            m_storageBudget.Release(shard.m_capacity + shard.m_capacity * sizeof(SymbolEntry*));
        }
    }

    SymbolTable& SymbolTable::Instance()
    {
        static AZ::NoDestructor<SymbolTable> table;
        return table.Get();
    }

    const SymbolEntry* SymbolTable::Intern(
        const AZStd::string_view value,
        const u64 hash)
    {
        const SymbolEntry* entry = TryIntern(value, hash);
        if (!entry)
        {
            FailSymbol("Failed to intern an AZ::Symbol value");
        }
        return entry;
    }

    const SymbolEntry* SymbolTable::TryIntern(
        const AZStd::string_view value,
        const u64 hash)
    {
        if (value.size() > Symbol::MaxStringSize)
        {
            return nullptr;
        }

        const u64 mixedHash = MixHash(hash);
        const size_t shardIndex = static_cast<size_t>(mixedHash >> ShardIndexShift);
        Shard& shard = m_shards[shardIndex];

        AZStd::lock_guard<AZStd::mutex> lock(shard.m_mutex);
        ProbeResult result;
        if (shard.m_controls)
        {
            result = Probe(shard, value, hash, mixedHash);
            if (result.m_entry)
            {
                return result.m_entry;
            }
        }

        const SymbolArena::Checkpoint arenaCheckpoint = shard.m_arena.GetCheckpoint();
        SymbolEntry* entry = shard.m_arena.AllocateEntry(value, hash);
        if (!entry)
        {
            return nullptr;
        }

        if (!shard.m_controls)
        {
            if (!InitializeShard(shard))
            {
                shard.m_arena.Rollback(arenaCheckpoint);
                return nullptr;
            }
            result = Probe(shard, value, hash, mixedHash);
        }

        if (shard.m_size >= shard.m_capacity - shard.m_capacity / 8)
        {
            if (!Resize(shard))
            {
                shard.m_arena.Rollback(arenaCheckpoint);
                return nullptr;
            }
            result = Probe(shard, value, hash, mixedHash);
        }

        if (result.m_emptySlot == SymbolTableInvalidSlot)
        {
            shard.m_arena.Rollback(arenaCheckpoint);
            return nullptr;
        }

        shard.m_slots[result.m_emptySlot] = entry;
        shard.m_controls[result.m_emptySlot] = Fingerprint(mixedHash);
        ++shard.m_size;
        return entry;
    }

    const SymbolEntry* SymbolTable::Find(
        const AZStd::string_view value,
        const u64 hash)
    {
        if (value.size() > Symbol::MaxStringSize)
        {
            return nullptr;
        }

        const u64 mixedHash = MixHash(hash);
        const size_t shardIndex = static_cast<size_t>(mixedHash >> ShardIndexShift);
        Shard& shard = m_shards[shardIndex];

        AZStd::lock_guard<AZStd::mutex> lock(shard.m_mutex);
        if (!shard.m_controls)
        {
            return nullptr;
        }
        return Probe(shard, value, hash, mixedHash).m_entry;
    }

    u64 SymbolTable::MixHash(u64 hash)
    {
        hash ^= hash >> 30;
        hash *= 0xBF58476D1CE4E5B9ull;
        hash ^= hash >> 27;
        hash *= 0x94D049BB133111EBull;
        hash ^= hash >> 31;
        return hash;
    }

    u8 SymbolTable::Fingerprint(const u64 mixedHash)
    {
        return static_cast<u8>((mixedHash >> 32) & 0x7F);
    }

    SymbolTable::ProbeResult SymbolTable::Probe(
        const Shard& shard,
        const AZStd::string_view value,
        const u64 hash,
        const u64 mixedHash)
    {
        const size_t groupCount = shard.m_capacity / SymbolGroup::Width;
        const size_t groupMask = groupCount - 1;
        const size_t initialGroup = static_cast<size_t>(mixedHash) & groupMask;
        const u8 fingerprint = Fingerprint(mixedHash);

        for (size_t probeIndex = 0; probeIndex < groupCount; ++probeIndex)
        {
            const size_t groupOffset = (probeIndex * (probeIndex + 1)) / 2;
            const size_t groupIndex = (initialGroup + groupOffset) & groupMask;
            const size_t groupStart = groupIndex * SymbolGroup::Width;
            const SymbolGroupMasks masks = SymbolGroup::Match(shard.m_controls + groupStart, fingerprint);

            for (size_t lane = 0; lane < SymbolGroup::Width; ++lane)
            {
                if ((masks.m_matches & static_cast<u16>(1u << lane)) == 0)
                {
                    continue;
                }

                const SymbolEntry* entry = shard.m_slots[groupStart + lane];
                AZ_Assert(entry, "Occupied AZ::Symbol control has no entry");
                if (entry && EntriesMatch(*entry, value, hash))
                {
                    return ProbeResult{
                        .m_entry = entry,
                        .m_emptySlot = SymbolTableInvalidSlot,
                    };
                }
            }

            if (masks.m_empty != 0)
            {
                return ProbeResult{
                    .m_entry = nullptr,
                    .m_emptySlot = groupStart + FirstSetLane(masks.m_empty),
                };
            }
        }

        return ProbeResult{};
    }

    bool SymbolTable::InitializeShard(Shard& shard)
    {
        if (!AllocateTableStorage(m_allocator, m_storageBudget, InitialCapacity, shard.m_controls, shard.m_slots))
        {
            return false;
        }
        shard.m_capacity = InitialCapacity;
        return true;
    }

    bool SymbolTable::Resize(Shard& shard)
    {
        const size_t oldCapacity = shard.m_capacity;
        u8* oldControls = shard.m_controls;
        const SymbolEntry** oldSlots = shard.m_slots;

        if (oldCapacity > (std::numeric_limits<size_t>::max)() / 2)
        {
            return false;
        }

        const size_t newCapacity = oldCapacity * 2;
        u8* newControls = nullptr;
        const SymbolEntry** newSlots = nullptr;
        if (!AllocateTableStorage(m_allocator, m_storageBudget, newCapacity, newControls, newSlots))
        {
            return false;
        }

        shard.m_capacity = newCapacity;
        shard.m_controls = newControls;
        shard.m_slots = newSlots;

        for (size_t slot = 0; slot < oldCapacity; ++slot)
        {
            if (oldControls[slot] != SymbolTableEmptyControl)
            {
                InsertExisting(shard, oldSlots[slot]);
            }
        }

        m_allocator.Deallocate(oldControls, oldCapacity, SymbolGroup::Width);
        m_allocator.Deallocate(oldSlots, oldCapacity * sizeof(SymbolEntry*), alignof(SymbolEntry*));
        m_storageBudget.Release(oldCapacity + oldCapacity * sizeof(SymbolEntry*));
        return true;
    }

    void SymbolTable::InsertExisting(Shard& shard, const SymbolEntry* entry)
    {
        const u64 mixedHash = MixHash(entry->m_hash);
        const ProbeResult result = Probe(shard, AZStd::string_view{entry->GetData(), entry->m_size}, entry->m_hash, mixedHash);
        AZ_Assert(!result.m_entry && result.m_emptySlot != SymbolTableInvalidSlot, "Invalid AZ::Symbol rehash result");
        shard.m_slots[result.m_emptySlot] = entry;
        shard.m_controls[result.m_emptySlot] = Fingerprint(mixedHash);
    }
} // namespace AZ::Internal
