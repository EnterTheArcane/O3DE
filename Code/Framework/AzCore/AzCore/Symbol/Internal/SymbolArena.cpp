/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Symbol/Internal/SymbolArena.h>

#include <AzCore/Symbol/Internal/SymbolFailure.h>

#include <cstring>
#include <limits>
#include <new>

namespace AZ::Internal
{
    SymbolArena::~SymbolArena()
    {
        while (m_currentBlock)
        {
            Block* previousBlock = m_currentBlock->m_previous;
            m_allocator->Deallocate(m_currentBlock, BlockDataOffset + m_currentBlock->m_capacity, EntryAlignment);
            m_currentBlock = previousBlock;
        }
    }

    SymbolEntry* SymbolArena::AllocateEntry(
        const AZStd::string_view value,
        const u64 hash)
    {
        if (value.size() > (std::numeric_limits<size_t>::max)() - sizeof(SymbolEntry) - EntryAlignment)
        {
            FailSymbol("AZ::Symbol entry size overflow");
        }

        const size_t entrySize = AlignUp(sizeof(SymbolEntry) + value.size() + 1, EntryAlignment);
        if (!m_currentBlock || m_currentBlock->m_capacity - m_currentBlock->m_used < entrySize)
        {
            Block* newBlock = AllocateBlock(entrySize);
            m_currentBlock = newBlock;
        }

        AZStd::byte* blockData = reinterpret_cast<AZStd::byte*>(m_currentBlock) + BlockDataOffset;
        void* entryMemory = blockData + m_currentBlock->m_used;
        m_currentBlock->m_used += entrySize;

        SymbolEntry* entry = ::new (entryMemory) SymbolEntry{
            .m_hash = hash,
            .m_size = static_cast<u32>(value.size()),
        };

        char* entryData = reinterpret_cast<char*>(entry + 1);
        if (!value.empty())
        {
            std::memcpy(entryData, value.data(), value.size());
        }
        entryData[value.size()] = '\0';
        return entry;
    }

    SymbolArena::Block* SymbolArena::AllocateBlock(const size_t requiredCapacity)
    {
        size_t capacity = m_nextBlockCapacity;
        if (capacity < requiredCapacity)
        {
            capacity = requiredCapacity;
        }

        if (capacity > (std::numeric_limits<size_t>::max)() - BlockDataOffset)
        {
            FailSymbol("AZ::Symbol arena block size overflow");
        }

        void* blockMemory = m_allocator->Allocate(BlockDataOffset + capacity, EntryAlignment, "AZ::Symbol arena");
        if (!blockMemory)
        {
            FailSymbol("Failed to allocate an AZ::Symbol arena block");
        }

        Block* block = ::new (blockMemory) Block{
            .m_previous = m_currentBlock,
            .m_capacity = capacity,
            .m_used = 0,
        };

        if (m_nextBlockCapacity < MaximumBlockCapacity)
        {
            m_nextBlockCapacity *= 2;
            if (m_nextBlockCapacity > MaximumBlockCapacity)
            {
                m_nextBlockCapacity = MaximumBlockCapacity;
            }
        }

        return block;
    }
} // namespace AZ::Internal
