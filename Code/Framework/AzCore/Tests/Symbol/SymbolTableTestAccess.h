/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Symbol/Internal/SymbolTable.h>

#include <cstring>

namespace AZ::Internal
{
    //! Test-only access to deterministic hash-domain and storage-layout seams.
    class SymbolTableTestAccess final
    {
    public:
        struct HashParts final
        {
            u8 m_h2;
            size_t m_shardIndex;
            u64 m_placement;
        };

        struct TableStorageView final
        {
            const u8* m_controls;
            const SymbolEntry* const* m_slots;
            size_t m_capacity;
            size_t m_size;
        };

        [[nodiscard]]
        static constexpr size_t GetHashSecretByteCount()
        {
            return SymbolTable::HashSecretByteCount;
        }

        static void UseFixedHashSecret(
            SymbolTable& table,
            const u8* hashSecret)
        {
            std::memcpy(table.m_fixedHashSecretForTest.data(), hashSecret, SymbolTable::HashSecretByteCount);
            table.m_hashSecret = table.m_fixedHashSecretForTest.data();
        }

        [[nodiscard]]
        static u64 HashValue(
            const SymbolTable& table,
            const AZStd::string_view value)
        {
            return table.HashValue(value);
        }

        [[nodiscard]]
        static HashParts SplitHash(const u64 tableHash)
        {
            const SymbolTable::HashParts parts = SymbolTable::SplitHash(tableHash);
            return HashParts{
                .m_h2 = parts.m_h2,
                .m_shardIndex = parts.m_shardIndex,
                .m_placement = parts.m_placement,
            };
        }

        [[nodiscard]]
        static const SymbolEntry* InternWithTableHash(
            SymbolTable& table,
            const AZStd::string_view value,
            const u64 tableHash)
        {
            return table.TryInternWithTableHash(value, tableHash);
        }

        [[nodiscard]]
        static const SymbolEntry* FindWithTableHash(
            SymbolTable& table,
            const AZStd::string_view value,
            const u64 tableHash)
        {
            return table.FindWithTableHash(value, tableHash);
        }

        [[nodiscard]]
        static TableStorageView GetTableStorage(
            const SymbolTable& table,
            const size_t shardIndex)
        {
            AZ_Assert(shardIndex < SymbolTable::ShardCount, "Invalid AZ::Symbol shard index");
            const SymbolTable::Shard& shard = table.m_shards[shardIndex];
            return TableStorageView{
                .m_controls = shard.m_table.m_controls,
                .m_slots = shard.m_table.m_slots,
                .m_capacity = shard.m_table.m_capacity,
                .m_size = shard.m_size,
            };
        }

        [[nodiscard]]
        static bool CalculateTableStorageByteSize(
            const size_t capacity,
            size_t& storageByteSize)
        {
            return SymbolTable::CalculateTableStorageByteSize(capacity, storageByteSize);
        }

        static void InitializeHashSecret(
            u8* hashSecret,
            bool (*randomFill)(void*, size_t))
        {
            SymbolTable::InitializeHashSecret(hashSecret, randomFill);
        }
    };
} // namespace AZ::Internal
