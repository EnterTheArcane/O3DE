/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/base.h>
#include <AzCore/Debug/Trace.h>
#include <AzCore/std/parallel/atomic.h>

namespace AZ::Internal
{
    //! Accounts for every requested dynamically allocated byte owned by one SymbolTable.
    class SymbolStorageBudget final
    {
    public:
        explicit SymbolStorageBudget(size_t limit)
            : m_limit{limit}
        {
        }

        AZ_DISABLE_COPY_MOVE(SymbolStorageBudget);

        [[nodiscard]]
        bool TryReserve(size_t byteSize)
        {
            size_t used = m_used.load(AZStd::memory_order_relaxed);
            while (used <= m_limit && byteSize <= m_limit - used)
            {
                if (m_used.compare_exchange_weak(
                        used,
                        used + byteSize,
                        AZStd::memory_order_acq_rel,
                        AZStd::memory_order_relaxed))
                {
                    return true;
                }
            }
            return false;
        }

        void Release(size_t byteSize)
        {
            const size_t previous = m_used.fetch_sub(byteSize, AZStd::memory_order_acq_rel);
            AZ_Assert(previous >= byteSize, "AZ::Symbol storage accounting underflow");
        }

        [[nodiscard]]
        size_t GetUsed() const
        {
            return m_used.load(AZStd::memory_order_acquire);
        }

        [[nodiscard]]
        size_t GetLimit() const
        {
            return m_limit;
        }

    private:
        const size_t m_limit;
        AZStd::atomic<size_t> m_used{0};
    };
} // namespace AZ::Internal
