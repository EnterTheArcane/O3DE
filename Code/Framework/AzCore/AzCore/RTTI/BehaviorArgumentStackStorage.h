/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <AzCore/RTTI/BehaviorContext.h>

namespace AZ::Internal
{
    //! Manages BehaviorArgument objects constructed in caller-owned stack storage.
    //! alloca is only guaranteed to provide fundamental alignment, while BehaviorArgument
    //! currently requires extended alignment because of its temporary value allocator.
    class BehaviorArgumentStackStorage
    {
    public:
        static constexpr size_t Alignment = alignof(BehaviorArgument);

        static size_t RequiredStorageSize(size_t argumentCount)
        {
            return sizeof(BehaviorArgument) * argumentCount + Alignment - 1;
        }

        void Initialize(void* storage, size_t argumentCount)
        {
            AZ_Assert(m_arguments == nullptr, "Behavior argument stack storage can only be initialized once");
            const uintptr_t storageAddress = reinterpret_cast<uintptr_t>(storage);
            const uintptr_t alignedAddress = (storageAddress + Alignment - 1) & ~(Alignment - 1);
            m_arguments = reinterpret_cast<BehaviorArgument*>(alignedAddress);
            m_capacity = argumentCount;
        }

        void Emplace(const BehaviorArgument& argument)
        {
            AZ_Assert(m_constructedCount < m_capacity, "Behavior argument stack storage capacity exceeded");
            new(m_arguments + m_constructedCount) BehaviorArgument(argument);
            ++m_constructedCount;
        }

        AZStd::span<BehaviorArgument> GetSpan() const
        {
            return { m_arguments, m_capacity };
        }

        ~BehaviorArgumentStackStorage()
        {
            while (m_constructedCount > 0)
            {
                m_arguments[--m_constructedCount].~BehaviorArgument();
            }
        }

    private:
        BehaviorArgument* m_arguments{};
        size_t m_capacity{};
        size_t m_constructedCount{};
    };
} // namespace AZ::Internal
