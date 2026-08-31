/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Memory/IAllocator.h>
#include <AzCore/Symbol/Internal/SymbolArena.h>
#include <AzCore/Symbol/Internal/SymbolStorageBudget.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/parallel/mutex.h>
#include <AzCore/std/string/string_view.h>

namespace AZ::Internal
{
    class SymbolTableTestAccess;
    struct SymbolStorageStats;

    //! Process-local concurrent canonical string table. Hashes and the hash secret are implementation details.
    class AZCORE_API SymbolTable final
    {
    public:
        AZ_DISABLE_COPY_MOVE(SymbolTable);

        SymbolTable();
        explicit SymbolTable(AZ::IAllocator& allocator);
        SymbolTable(
            AZ::IAllocator& allocator,
            size_t storageBudgetBytes);
        ~SymbolTable();

        [[nodiscard]]
        static SymbolTable& Instance();

        //! Interns a value already checked by ValidateSymbolValue. Returns null only on storage failure.
        [[nodiscard]]
        const SymbolEntry* InternValidated(AZStd::string_view value);

        //! Returns an existing entry before validation, or validates and attempts to intern a miss.
        [[nodiscard]]
        const SymbolEntry* TryIntern(AZStd::string_view value);

        [[nodiscard]]
        const SymbolEntry* Find(AZStd::string_view value);

        [[nodiscard]]
        SymbolStorageStats GetStorageStats() const;

        [[nodiscard]]
        size_t GetStorageBytes() const
        {
            return m_storageBudget.GetUsed();
        }

    private:
        friend class SymbolTableTestAccess;

        struct HashParts final
        {
            u8 m_h2;
            size_t m_shardIndex;
            u64 m_placement;
        };

        struct TableStorage final
        {
            u8* m_controls = nullptr;
            const SymbolEntry** m_slots = nullptr;
            size_t m_capacity = 0;
        };

        struct ProbeResult final
        {
            const SymbolEntry* m_entry = nullptr;
            size_t m_emptySlot = static_cast<size_t>(-1);
        };

        static constexpr size_t ShardAlignment = 64;

        struct alignas(ShardAlignment) Shard final
        {
            AZ_DISABLE_COPY_MOVE(Shard);

            Shard() = default;

            mutable AZStd::mutex m_mutex;
            SymbolArena m_arena;
            TableStorage m_table;
            size_t m_size = 0;
        };

        using RandomFillFunction = bool (*)(void*, size_t);

        [[nodiscard]]
        u64 HashValue(AZStd::string_view value) const;

        [[nodiscard]]
        static HashParts SplitHash(u64 tableHash);

        [[nodiscard]]
        static ProbeResult Probe(
            const TableStorage& table,
            AZStd::string_view value,
            u64 tableHash);

        [[nodiscard]]
        const SymbolEntry* TryInternWithTableHash(
            AZStd::string_view value,
            u64 tableHash);

        [[nodiscard]]
        const SymbolEntry* FindWithTableHash(
            AZStd::string_view value,
            u64 tableHash);

        [[nodiscard]]
        bool AllocateTableStorage(
            size_t capacity,
            TableStorage& table);

        void ReleaseTableStorage(TableStorage& table);

        [[nodiscard]]
        bool InitializeShard(Shard& shard);

        [[nodiscard]]
        bool Resize(Shard& shard);

        static void InsertExisting(
            TableStorage& table,
            const SymbolEntry* entry);

        [[nodiscard]]
        static bool CalculateTableStorageByteSize(
            size_t capacity,
            size_t& storageByteSize);

        static void InitializeHashSecret(
            u8* hashSecret,
            RandomFillFunction randomFill);

        [[nodiscard]]
        static const u8* GetProcessHashSecret();

        static constexpr size_t ShardCount = 32;
        static constexpr size_t ShardIndexBitCount = 5;
        static constexpr size_t FingerprintBitCount = 7;
        static constexpr size_t PlacementBitCount = 64 - ShardIndexBitCount - FingerprintBitCount;
        static constexpr size_t HashSecretByteCount = 192;
        static constexpr size_t InitialCapacity = 16;

        static_assert(ShardCount == (size_t{1} << ShardIndexBitCount));
        static_assert(alignof(Shard) == ShardAlignment);

        SymbolAllocator m_allocator;
        SymbolStorageBudget m_storageBudget;
        AZStd::array<Shard, ShardCount> m_shards;
        AZStd::array<u8, HashSecretByteCount> m_fixedHashSecretForTest{};
        const u8* m_hashSecret = nullptr;
    };
} // namespace AZ::Internal
