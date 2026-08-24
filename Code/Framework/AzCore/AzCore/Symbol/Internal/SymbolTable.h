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
    //! Internal implementation surface exported for AzCore-owned collision, allocation, and budget qualification.
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

        [[nodiscard]]
        const SymbolEntry* Intern(AZStd::string_view value, u64 hash);

        [[nodiscard]]
        const SymbolEntry* TryIntern(AZStd::string_view value, u64 hash);

        [[nodiscard]]
        const SymbolEntry* Find(AZStd::string_view value, u64 hash);

        [[nodiscard]]
        size_t GetStorageBytes() const
        {
            return m_storageBudget.GetUsed();
        }

    private:
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

            AZStd::mutex m_mutex;
            SymbolArena m_arena;

            u8* m_controls = nullptr;
            const SymbolEntry** m_slots = nullptr;
            size_t m_capacity = 0;
            size_t m_size = 0;
        };

        [[nodiscard]]
        static u64 MixHash(u64 hash);

        [[nodiscard]]
        static u8 Fingerprint(u64 mixedHash);

        [[nodiscard]]
        static ProbeResult Probe(
            const Shard& shard,
            AZStd::string_view value,
            u64 hash,
            u64 mixedHash);

        [[nodiscard]]
        bool InitializeShard(Shard& shard);

        [[nodiscard]]
        bool Resize(Shard& shard);

        static void InsertExisting(
            Shard& shard,
            const SymbolEntry* entry);

        static constexpr size_t ShardCount = 32;
        static constexpr size_t ShardIndexBitCount = 5;
        static constexpr size_t ShardIndexShift = 64 - ShardIndexBitCount;
        static constexpr size_t InitialCapacity = 16;

        static_assert(ShardCount == (size_t{1} << ShardIndexBitCount));
        static_assert(alignof(Shard) == ShardAlignment);

        SymbolAllocator m_allocator;
        SymbolStorageBudget m_storageBudget;
        AZStd::array<Shard, ShardCount> m_shards;
    };
} // namespace AZ::Internal
