/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Symbol/Internal/SymbolAllocator.h>
#include <AzCore/Symbol/Internal/SymbolStorageBudget.h>
#include <AzCore/std/containers/array.h>

#include <cstring>

namespace AZ::Internal
{
    //! Temporary persistence buffer with 1 KiB inline storage and fallible heap growth.
    //! Heap storage is released on destruction and is separate from the permanent intern-table budget.
    class SymbolSerializerBuffer final
    {
    public:
        SymbolSerializerBuffer() = default;

        explicit SymbolSerializerBuffer(IAllocator& allocator)
            : m_allocator{allocator}
        {
        }

        AZ_DISABLE_COPY_MOVE(SymbolSerializerBuffer);

        ~SymbolSerializerBuffer()
        {
            if (m_data != m_inline.data())
            {
                m_allocator.Deallocate(m_data, m_capacity, alignof(void*));
            }
        }

        //! The first preserveSize bytes must be initialized.
        [[nodiscard]]
        bool Reserve(size_t capacity, size_t preserveSize = 0)
        {
            // Reject untrusted lengths that cannot fit in permanent storage.
            if (capacity > SymbolStorageBudgetBytes)
            {
                return false;
            }
            if (preserveSize > m_capacity || preserveSize > capacity)
            {
                return false;
            }
            if (capacity <= m_capacity)
            {
                return true;
            }

            char* data = static_cast<char*>(m_allocator.Allocate(capacity, alignof(void*), "AZ::Symbol serializer"));
            if (!data)
            {
                return false;
            }
            if (preserveSize != 0)
            {
                std::memcpy(data, m_data, preserveSize);
            }
            if (m_data != m_inline.data())
            {
                m_allocator.Deallocate(m_data, m_capacity, alignof(void*));
            }
            m_data = data;
            m_capacity = capacity;
            return true;
        }

        [[nodiscard]]
        char* GetData()
        {
            return m_data;
        }

        //! Index must be below capacity, and reads require an initialized byte.
        [[nodiscard]]
        char& operator[](size_t index)
        {
            return m_data[index];
        }

        [[nodiscard]]
        const char& operator[](size_t index) const
        {
            return m_data[index];
        }

        [[nodiscard]]
        size_t GetCapacity() const
        {
            return m_capacity;
        }

    private:
        SymbolAllocator m_allocator;
        AZStd::array<char, 1024> m_inline;
        char* m_data = m_inline.data();
        size_t m_capacity = m_inline.size();
    };
} // namespace AZ::Internal
