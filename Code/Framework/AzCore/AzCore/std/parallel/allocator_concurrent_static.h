/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/std/base.h>
#include <AzCore/std/parallel/atomic.h>
#include <AzCore/std/typetraits/integral_constant.h>
#include <AzCore/std/typetraits/aligned_storage.h>
#include <AzCore/std/typetraits/alignment_of.h>

#include <bit>

namespace AZStd
{
    /**
     * Declares a static buffer of Node[NumNodes], and them pools them.
     * It provides concurrent safe access.
     *
     * This is a perfect allocator for pooling lists or hash table nodes.
     * Internally the buffer is allocated using aligned_storage.
     *
     * \note only allocate/deallocate are thread safe.
     * reset, leak_before_destroy and comparison operators are not thread safe.
     * get_allocated_size is thread safe but the returned value is not perfectly in sync
     * on the actual number of allocations (the number of allocations is incremented before the
     * allocation happens and decremented after the allocation happens, trying to give a conservative number)
     *
     * \note be careful if you use this on the stack, since many platforms do NOT support alignment more than 16 bytes.
     * In such cases you will need to do it manually.
     */
    template<class Node, size_t NumNodes>
    class static_pool_concurrent_allocator
    {
        static_assert(NumNodes > 0, "static_pool_concurrent_allocator requires at least one node");

        using free_node_word_type = uint64_t;
        static constexpr size_t nodes_per_free_word = sizeof(free_node_word_type) * 8;
        static constexpr size_t free_node_word_count = (NumNodes + nodes_per_free_word - 1) / nodes_per_free_word;

    public:
        using value_type = Node;
        using pointer = Node*;
        using size_type = size_t;
        using difference_type = ptrdiff_t;

        AZ_FORCE_INLINE static_pool_concurrent_allocator()
        {
            reset();
        }

        AZ_FORCE_INLINE ~static_pool_concurrent_allocator()
        {
            AZ_Assert(m_numOfAllocatedNodes == 0, "We still have allocated nodes. Call leak_before_destroy() before you destroy the container, to indicate this is ok.");
        }

        // When we copy the allocator we don't copy the allocated memory since it's the user responsibility.
        AZ_FORCE_INLINE static_pool_concurrent_allocator(const static_pool_concurrent_allocator& rhs)
        {
            reset();
        }

        AZ_FORCE_INLINE static_pool_concurrent_allocator& operator=(const static_pool_concurrent_allocator& rhs)
        {
            return *this;
        }

        constexpr size_type max_size() const
        {
            return NumNodes * sizeof(Node);
        }

        AZ_FORCE_INLINE size_type get_allocated_size() const
        {
            return m_numOfAllocatedNodes.load(memory_order_relaxed) * sizeof(Node);
        }

        [[nodiscard]] AZ_FORCE_INLINE Node* allocate()
        {
            const size_t firstFreeWord = m_nextFreeWord.fetch_add(1, memory_order_relaxed) % free_node_word_count;
            for (size_t wordOffset = 0; wordOffset < free_node_word_count; ++wordOffset)
            {
                const size_t wordIndex = (firstFreeWord + wordOffset) % free_node_word_count;
                free_node_word_type freeNodes = m_freeNodes[wordIndex].load(memory_order_acquire);
                while (freeNodes != 0)
                {
                    const size_t bitIndex = static_cast<size_t>(std::countr_zero(freeNodes));
                    const free_node_word_type nodeMask = free_node_word_type{ 1 } << bitIndex;
                    if (m_freeNodes[wordIndex].compare_exchange_weak(
                        freeNodes, freeNodes & ~nodeMask, memory_order_acq_rel, memory_order_acquire))
                    {
                        ++m_numOfAllocatedNodes;
                        const size_t nodeIndex = wordIndex * nodes_per_free_word + bitIndex;
                        return reinterpret_cast<Node*>(&m_data) + nodeIndex;
                    }
                }
            }

            AZ_Assert(false, "AZStd::static_pool_concurrent_allocator - No more free nodes!");
            return nullptr;
        }

        [[nodiscard]] AZ_FORCE_INLINE pointer allocate(
            [[maybe_unused]] size_type byteSize,
            [[maybe_unused]] size_type alignment,
            [[maybe_unused]] int flags = 0)
        {
            AZ_Assert(alignment > 0 && (alignment & (alignment - 1)) == 0, "AZStd::static_pool_concurrent_allocator::allocate - alignment must be > 0 and power of 2");
            AZ_Assert(byteSize == sizeof(Node), "AZStd::static_pool_concurrent_allocator - We can allocate only node sizes from the pool!");
            AZ_Assert(alignment <= AZStd::alignment_of_v<Node>, "AZStd::static_pool_concurrent_allocator - Invalid data alignment!");

            return allocate();
        }

        void deallocate(Node* ptr)
        {
            const uintptr_t dataAddress = reinterpret_cast<uintptr_t>(&m_data);
            const uintptr_t nodeAddress = reinterpret_cast<uintptr_t>(ptr);
            if (nodeAddress < dataAddress || nodeAddress >= dataAddress + sizeof(Node) * NumNodes
                || (nodeAddress - dataAddress) % sizeof(Node) != 0)
            {
                AZ_Assert(false, "AZStd::static_pool_concurrent_allocator - Pointer is out of range!");
                return;
            }

            const size_t nodeIndex = (nodeAddress - dataAddress) / sizeof(Node);
            const size_t wordIndex = nodeIndex / nodes_per_free_word;
            const free_node_word_type nodeMask = free_node_word_type{ 1 } << (nodeIndex % nodes_per_free_word);
            const free_node_word_type previousFreeNodes = m_freeNodes[wordIndex].fetch_or(nodeMask, memory_order_release);
            if ((previousFreeNodes & nodeMask) != 0)
            {
                AZ_Assert(false, "AZStd::static_pool_concurrent_allocator - Node has already been deallocated!");
                return;
            }
            --m_numOfAllocatedNodes;
        }

        AZ_FORCE_INLINE void deallocate(
            pointer ptr,
            [[maybe_unused]] size_type byteSize,
            [[maybe_unused]] size_type alignment)
        {
            AZ_Assert(alignment > 0 && (alignment & (alignment - 1)) == 0, "AZStd::static_pool_concurrent_allocator::deallocate - alignment must be > 0 and power of 2");
            AZ_Assert(byteSize <= sizeof(Node), "AZStd::static_pool_concurrent_allocator - We can allocate only node sizes from the pool!");
            deallocate(reinterpret_cast<Node*>(ptr));
        }

        AZ_FORCE_INLINE size_type resize(
            [[maybe_unused]] pointer ptr,
            [[maybe_unused]] size_type newSize) const
        {
            return sizeof(Node); // this is the max size we can have.
        }

        void reset()
        {
            m_numOfAllocatedNodes = 0;
            m_nextFreeWord = 0;
            for (size_t wordIndex = 0; wordIndex < free_node_word_count; ++wordIndex)
            {
                free_node_word_type freeNodes = ~static_cast<free_node_word_type>(0);
                if constexpr ((NumNodes % nodes_per_free_word) != 0)
                {
                    if (wordIndex == free_node_word_count - 1)
                    {
                        freeNodes = (free_node_word_type{ 1 } << (NumNodes % nodes_per_free_word)) - 1;
                    }
                }
                m_freeNodes[wordIndex].store(freeNodes, memory_order_relaxed);
            }
        }

        void leak_before_destroy()
        {
#ifdef AZ_Assert // used only to confirm that it is ok for use to have allocated nodes
            m_numOfAllocatedNodes = 0;
            for (atomic<free_node_word_type>& freeNodes : m_freeNodes)
            {
                freeNodes.store(0, memory_order_relaxed); // We are not allowed to allocate after we call leak_before_destroy.
            }
#endif
        }

        AZ_FORCE_INLINE void* data() const
        {
            return &m_data;
        }

        AZ_FORCE_INLINE constexpr size_type data_size() const
        {
            return sizeof(Node) * NumNodes;
        }

        friend AZ_FORCE_INLINE bool operator==(
            const static_pool_concurrent_allocator& a,
            const static_pool_concurrent_allocator& b)
        {
            return &a == &b;
        }

    private:
        aligned_storage_t<sizeof(Node)* NumNodes, alignment_of_v<Node>> m_data;
        atomic<free_node_word_type> m_freeNodes[free_node_word_count];
        atomic<size_t> m_nextFreeWord;
        atomic<size_type> m_numOfAllocatedNodes;
    };
}
