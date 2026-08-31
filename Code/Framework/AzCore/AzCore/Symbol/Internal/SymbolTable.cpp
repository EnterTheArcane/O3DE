/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Symbol/Internal/SymbolTable.h>

#include <AzCore/Math/Random.h>
#include <AzCore/Symbol/Internal/SymbolFailure.h>
#include <AzCore/Symbol/Internal/SymbolGroup.h>
#include <AzCore/Symbol/Internal/SymbolStorage.h>
#include <AzCore/Symbol/Internal/SymbolValidation.h>
#include <AzCore/Symbol/Symbol.h>
#include <AzCore/Utils/NoDestructor.h>
#include <AzCore/std/createdestroy.h>
#include <AzCore/std/containers/fixed_vector.h>
#include <AzCore/std/parallel/lock.h>

#include <bit>
#include <cstring>
#include <limits>

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
#   include <intrin.h>
#endif

#include <xxhash.h>

namespace AZ::Internal
{
    constexpr size_t SymbolStorageBudgetBytes = size_t{1024} * 1024 * 1024;

    namespace
    {
        constexpr size_t SymbolTableInvalidSlot = static_cast<size_t>(-1);
        constexpr size_t SymbolTableStorageAlignment = SymbolGroup::Width;

        static_assert(SymbolGroup::Width != 0 && (SymbolGroup::Width & (SymbolGroup::Width - 1)) == 0);
        static_assert(alignof(SymbolEntry*) <= SymbolGroup::Width);
        static_assert(SymbolGroup::Width % alignof(SymbolEntry*) == 0);

        [[nodiscard]]
        bool FillProcessHashSecret(
            void* data,
            const size_t dataSize)
        {
            BetterPseudoRandom random;
            return random.GetRandom(data, dataSize);
        }

        [[nodiscard]]
        bool EntriesMatch(
            const SymbolEntry& entry,
            const AZStd::string_view value,
            const u64 tableHash)
        {
            if (entry.m_tableHash != tableHash || entry.m_size != value.size())
            {
                return false;
            }
            return std::memcmp(entry.GetData(), value.data(), value.size()) == 0;
        }

        [[nodiscard]]
        size_t FirstSetLane(const u16 mask)
        {
            AZ_Assert(mask != 0, "Cannot select a lane from an empty AZ::Symbol mask");
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
            unsigned long lane{};
            _BitScanForward(&lane, mask);
            return static_cast<size_t>(lane);
#else
            return static_cast<size_t>(std::countr_zero(static_cast<unsigned int>(mask)));
#endif
        }
    } // namespace

    SymbolTable::SymbolTable()
        : m_storageBudget{SymbolStorageBudgetBytes}
        , m_hashSecret{GetProcessHashSecret()}
    {
        for (Shard& shard : m_shards)
        {
            shard.m_arena.SetStorage(m_allocator, m_storageBudget);
        }
    }

    SymbolTable::SymbolTable(AZ::IAllocator& allocator)
        : m_allocator{allocator}
        , m_storageBudget{(std::numeric_limits<size_t>::max)()}
        , m_hashSecret{GetProcessHashSecret()}
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
        , m_hashSecret{GetProcessHashSecret()}
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
            ReleaseTableStorage(shard.m_table);
        }
    }

    SymbolTable& SymbolTable::Instance()
    {
        static AZ::NoDestructor<SymbolTable> table;
        return table.Get();
    }

    const SymbolEntry* SymbolTable::InternValidated(const AZStd::string_view value)
    {
        if (value.empty() || value.size() > Symbol::MaxStringSize)
        {
            return nullptr;
        }

        const u64 tableHash = HashValue(value);
        return TryInternWithTableHash(value, tableHash);
    }

    const SymbolEntry* SymbolTable::TryIntern(const AZStd::string_view value)
    {
        if (value.empty() || value.size() > Symbol::MaxStringSize)
        {
            return nullptr;
        }

        const u64 tableHash = HashValue(value);
        const HashParts hashParts = SplitHash(tableHash);
        Shard& shard = m_shards[hashParts.m_shardIndex];
        {
            AZStd::lock_guard<AZStd::mutex> lock(shard.m_mutex);
            if (shard.m_table.m_controls)
            {
                const ProbeResult result = Probe(shard.m_table, value, tableHash);
                if (result.m_entry)
                {
                    return result.m_entry;
                }
            }
        }

        if (ValidateSymbolValue(value, Symbol::MaxStringSize) != SymbolValidationError::None)
        {
            return nullptr;
        }

        return TryInternWithTableHash(value, tableHash);
    }

    const SymbolEntry* SymbolTable::Find(const AZStd::string_view value)
    {
        if (value.size() > Symbol::MaxStringSize)
        {
            return nullptr;
        }

        return FindWithTableHash(value, HashValue(value));
    }

    SymbolStorageStats SymbolTable::GetStorageStats() const
    {
        AZStd::fixed_vector<AZStd::unique_lock<AZStd::mutex>, ShardCount> locks;
        for (size_t shardIndex = 0; shardIndex < ShardCount; ++shardIndex)
        {
            locks.emplace_back(m_shards[shardIndex].m_mutex);
        }

        SymbolStorageStats stats{
            .m_usedByteCount = m_storageBudget.GetUsed(),
            .m_limitByteCount = m_storageBudget.GetLimit(),
        };
        for (const Shard& shard : m_shards)
        {
            stats.m_arenaByteCount += shard.m_arena.GetStorageBytes();
            stats.m_entryCount += shard.m_size;
            if (shard.m_table.m_controls)
            {
                size_t storageByteSize = 0;
                const bool sizeCalculated = CalculateTableStorageByteSize(shard.m_table.m_capacity, storageByteSize);
                AZ_Assert(sizeCalculated, "Invalid AZ::Symbol table storage size");
                stats.m_tableByteCount += storageByteSize;
            }
        }
        return stats;
    }

    u64 SymbolTable::HashValue(const AZStd::string_view value) const
    {
        static_assert(HashSecretByteCount >= XXH3_SECRET_SIZE_MIN);
        return XXH3_64bits_withSecret(value.data(), value.size(), m_hashSecret, HashSecretByteCount);
    }

    SymbolTable::HashParts SymbolTable::SplitHash(const u64 tableHash)
    {
        constexpr u64 FingerprintMask = (u64{1} << FingerprintBitCount) - 1;
        constexpr u64 PlacementMask = (u64{1} << PlacementBitCount) - 1;
        const u64 h1 = tableHash >> FingerprintBitCount;
        return HashParts{
            .m_h2 = static_cast<u8>(tableHash & FingerprintMask),
            .m_shardIndex = static_cast<size_t>(h1 >> PlacementBitCount),
            .m_placement = h1 & PlacementMask,
        };
    }

    SymbolTable::ProbeResult SymbolTable::Probe(
        const TableStorage& table,
        const AZStd::string_view value,
        const u64 tableHash)
    {
        constexpr u64 FingerprintMask = (u64{1} << FingerprintBitCount) - 1;
        constexpr u64 PlacementMask = (u64{1} << PlacementBitCount) - 1;
        const size_t groupCount = table.m_capacity / SymbolGroup::Width;
        const size_t groupMask = groupCount - 1;
        const u8 fingerprint = static_cast<u8>(tableHash & FingerprintMask);
        const u64 placement = (tableHash >> FingerprintBitCount) & PlacementMask;
        size_t groupIndex = static_cast<size_t>(placement) & groupMask;

        for (size_t probeIndex = 0; probeIndex < groupCount; ++probeIndex)
        {
            const size_t groupStart = groupIndex * SymbolGroup::Width;
            const SymbolGroupMasks masks = SymbolGroup::Match(table.m_controls + groupStart, fingerprint);

            u16 matchingLanes = masks.m_matches;
            while (matchingLanes != 0)
            {
                const size_t lane = FirstSetLane(matchingLanes);
                matchingLanes &= static_cast<u16>(matchingLanes - 1);

                const SymbolEntry* entry = table.m_slots[groupStart + lane];
                AZ_Assert(entry, "Occupied AZ::Symbol control has no entry");
                if (entry && EntriesMatch(*entry, value, tableHash))
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

            groupIndex = (groupIndex + probeIndex + 1) & groupMask;
        }

        return ProbeResult{};
    }

    const SymbolEntry* SymbolTable::TryInternWithTableHash(
        const AZStd::string_view value,
        const u64 tableHash)
    {
        const HashParts hashParts = SplitHash(tableHash);
        Shard& shard = m_shards[hashParts.m_shardIndex];

        AZStd::lock_guard<AZStd::mutex> lock(shard.m_mutex);
        ProbeResult result;
        if (shard.m_table.m_controls)
        {
            result = Probe(shard.m_table, value, tableHash);
            if (result.m_entry)
            {
                return result.m_entry;
            }
        }

        const SymbolArena::Checkpoint arenaCheckpoint = shard.m_arena.GetCheckpoint();
        SymbolEntry* entry = shard.m_arena.AllocateEntry(value, tableHash);
        if (!entry)
        {
            return nullptr;
        }

        if (!shard.m_table.m_controls)
        {
            if (!InitializeShard(shard))
            {
                shard.m_arena.Rollback(arenaCheckpoint);
                return nullptr;
            }
            result = Probe(shard.m_table, value, tableHash);
        }

        if (shard.m_size >= shard.m_table.m_capacity - shard.m_table.m_capacity / 8)
        {
            if (!Resize(shard))
            {
                shard.m_arena.Rollback(arenaCheckpoint);
                return nullptr;
            }
            result = Probe(shard.m_table, value, tableHash);
        }

        if (result.m_emptySlot == SymbolTableInvalidSlot)
        {
            shard.m_arena.Rollback(arenaCheckpoint);
            return nullptr;
        }

        shard.m_table.m_slots[result.m_emptySlot] = entry;
        shard.m_table.m_controls[result.m_emptySlot] = hashParts.m_h2;
        ++shard.m_size;
        return entry;
    }

    const SymbolEntry* SymbolTable::FindWithTableHash(
        const AZStd::string_view value,
        const u64 tableHash)
    {
        const HashParts hashParts = SplitHash(tableHash);
        Shard& shard = m_shards[hashParts.m_shardIndex];

        AZStd::lock_guard<AZStd::mutex> lock(shard.m_mutex);
        if (!shard.m_table.m_controls)
        {
            return nullptr;
        }
        return Probe(shard.m_table, value, tableHash).m_entry;
    }

    bool SymbolTable::AllocateTableStorage(
        const size_t capacity,
        TableStorage& table)
    {
        size_t storageByteSize = 0;
        if (capacity < SymbolGroup::Width || capacity % SymbolGroup::Width != 0 ||
            !CalculateTableStorageByteSize(capacity, storageByteSize))
        {
            return false;
        }

        if (!m_storageBudget.TryReserve(storageByteSize))
        {
            return false;
        }

        void* storage = m_allocator.Allocate(storageByteSize, SymbolTableStorageAlignment, "AZ::Symbol table");
        if (!storage)
        {
            m_storageBudget.Release(storageByteSize);
            return false;
        }

        u8* controls = reinterpret_cast<u8*>(storage);
        const SymbolEntry** slots = reinterpret_cast<const SymbolEntry**>(controls + capacity);
        std::memset(controls, SymbolGroupEmptyControl, capacity);
        AZStd::uninitialized_default_construct_n(slots, capacity);
        table = TableStorage{
            .m_controls = controls,
            .m_slots = slots,
            .m_capacity = capacity,
        };
        return true;
    }

    void SymbolTable::ReleaseTableStorage(TableStorage& table)
    {
        if (!table.m_controls)
        {
            return;
        }

        size_t storageByteSize = 0;
        const bool sizeCalculated = CalculateTableStorageByteSize(table.m_capacity, storageByteSize);
        AZ_Assert(sizeCalculated, "Invalid AZ::Symbol table storage size");
        m_allocator.Deallocate(table.m_controls, storageByteSize, SymbolTableStorageAlignment);
        m_storageBudget.Release(storageByteSize);
        table = {};
    }

    bool SymbolTable::InitializeShard(Shard& shard)
    {
        return AllocateTableStorage(InitialCapacity, shard.m_table);
    }

    bool SymbolTable::Resize(Shard& shard)
    {
        const size_t oldCapacity = shard.m_table.m_capacity;
        if (oldCapacity > (std::numeric_limits<size_t>::max)() / 2)
        {
            return false;
        }

        TableStorage replacement;
        if (!AllocateTableStorage(oldCapacity * 2, replacement))
        {
            return false;
        }

        for (size_t slot = 0; slot < oldCapacity; ++slot)
        {
            if (shard.m_table.m_controls[slot] != SymbolGroupEmptyControl)
            {
                InsertExisting(replacement, shard.m_table.m_slots[slot]);
            }
        }

        TableStorage oldTable = shard.m_table;
        shard.m_table = replacement;
        ReleaseTableStorage(oldTable);
        return true;
    }

    void SymbolTable::InsertExisting(
        TableStorage& table,
        const SymbolEntry* entry)
    {
        const HashParts hashParts = SplitHash(entry->m_tableHash);
        const ProbeResult result = Probe(
            table,
            AZStd::string_view{entry->GetData(), entry->m_size},
            entry->m_tableHash);
        AZ_Assert(!result.m_entry && result.m_emptySlot != SymbolTableInvalidSlot, "Invalid AZ::Symbol rehash result");
        table.m_slots[result.m_emptySlot] = entry;
        table.m_controls[result.m_emptySlot] = hashParts.m_h2;
    }

    bool SymbolTable::CalculateTableStorageByteSize(
        const size_t capacity,
        size_t& storageByteSize)
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

        storageByteSize = capacity + slotByteSize;
        return true;
    }

    void SymbolTable::InitializeHashSecret(
        u8* hashSecret,
        const RandomFillFunction randomFill)
    {
        if (!randomFill(hashSecret, HashSecretByteCount))
        {
            FailSymbol("Failed to initialize the AZ::Symbol process hash secret");
        }
    }

    const u8* SymbolTable::GetProcessHashSecret()
    {
        using HashSecret = AZStd::array<u8, HashSecretByteCount>;
        static AZ::NoDestructor<HashSecret> hashSecret;
        static const bool initialized = []
        {
            InitializeHashSecret(hashSecret.Get().data(), &FillProcessHashSecret);
            return true;
        }();
        AZ_UNUSED(initialized);
        return hashSecret.Get().data();
    }

    SymbolStorageStats GetSymbolStorageStats()
    {
        return SymbolTable::Instance().GetStorageStats();
    }
} // namespace AZ::Internal
