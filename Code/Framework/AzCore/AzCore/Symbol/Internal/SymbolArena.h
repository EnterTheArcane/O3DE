/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Symbol/Internal/SymbolAllocator.h>
#include <AzCore/Symbol/Internal/SymbolEntry.h>
#include <AzCore/Symbol/Internal/SymbolStorageBudget.h>
#include <AzCore/std/string/string_view.h>

namespace AZ::Internal
{
    class SymbolTable;

    class SymbolArena final
    {
    public:
        AZ_DISABLE_COPY_MOVE(SymbolArena);

        SymbolArena() = default;
        ~SymbolArena();

        [[nodiscard]]
        SymbolEntry* AllocateEntry(AZStd::string_view value, u64 tableHash);

    private:
        friend class SymbolTable;

        struct Block final
        {
            Block* m_previous;
            size_t m_capacity;
            size_t m_used;
        };

        struct Checkpoint final
        {
            Block* m_block;
            size_t m_used;
            size_t m_nextBlockCapacity;
        };

        [[nodiscard]]
        static constexpr size_t AlignUp(size_t value, size_t alignment)
        {
            return (value + alignment - 1) & ~(alignment - 1);
        }

        [[nodiscard]]
        Block* AllocateBlock(size_t requiredCapacity);

        [[nodiscard]]
        Checkpoint GetCheckpoint() const;

        [[nodiscard]]
        size_t GetStorageBytes() const;

        void Rollback(const Checkpoint& checkpoint);

        void SetStorage(
            SymbolAllocator& allocator,
            SymbolStorageBudget& budget)
        {
            AZ_Assert(!m_currentBlock, "Cannot replace an AZ::Symbol arena allocator after allocation");
            m_allocator = &allocator;
            m_budget = &budget;
        }

        static constexpr size_t EntryAlignment = alignof(SymbolEntry);
        static constexpr size_t InitialBlockCapacity = 4 * 1024;
        static constexpr size_t MaximumBlockCapacity = 1024 * 1024;
        static constexpr size_t BlockDataOffset = (sizeof(Block) + EntryAlignment - 1) & ~(EntryAlignment - 1);

        Block* m_currentBlock = nullptr;
        size_t m_nextBlockCapacity = InitialBlockCapacity;
        SymbolAllocator* m_allocator = nullptr;
        SymbolStorageBudget* m_budget = nullptr;
    };
} // namespace AZ::Internal
