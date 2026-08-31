/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Memory/IAllocator.h>
#include <AzCore/Memory/OSAllocator.h>

namespace AZ::Internal
{
    //! Keeps production Symbol storage independent of allocator singleton startup and shutdown order.
    class SymbolAllocator final
    {
    public:
        SymbolAllocator() = default;

        explicit SymbolAllocator(IAllocator& testAllocator)
            : m_testAllocator{&testAllocator}
        {
        }

        [[nodiscard]]
        void* Allocate(
            size_t byteSize,
            size_t alignment,
            [[maybe_unused]] const char* name)
        {
            if (m_testAllocator)
            {
                return m_testAllocator->allocate(byteSize, alignment).GetAddress();
            }
            return AZ_OS_MALLOC(byteSize, alignment);
        }

        void Deallocate(
            void* address,
            size_t byteSize,
            size_t alignment)
        {
            if (m_testAllocator)
            {
                m_testAllocator->deallocate(address, byteSize, alignment);
                return;
            }
            AZ_OS_FREE(address);
        }

    private:
        IAllocator* m_testAllocator = nullptr;
    };
} // namespace AZ::Internal
