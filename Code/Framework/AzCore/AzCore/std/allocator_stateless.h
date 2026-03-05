/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/std/base.h>
#include <AzCore/RTTI/TypeInfoSimple.h>
#include <AzCore/Memory/IAllocator.h>

namespace AZStd
{
    struct stateless_allocator
    {
        AZ_TYPE_INFO(stateless_allocator, "{E4976C53-0B20-4F39-8D41-0A76F59A7D68}");

        AZ_ALLOCATOR_DEFAULT_TRAITS

        AZCORE_API [[nodiscard]] pointer allocate(size_type byteSize, align_type alignment = 1);
        AZCORE_API void deallocate(pointer ptr, size_type byteSize = 0, align_type alignment = 0);
        AZCORE_API [[nodiscard]] pointer reallocate(pointer ptr, size_type newSize, align_type alignment = 1);

        static constexpr size_type resize(
            [[maybe_unused]] pointer ptr,
            [[maybe_unused]] size_type newSize)
        {
            return 0;
        }

        static constexpr size_type max_size()
        {
            return AZ_TRAIT_OS_MEMORY_MAX_ALLOCATOR_SIZE;
        }

        static constexpr bool is_lock_free()
        {
            return false;
        }

        static constexpr bool is_stale_read_allowed()
        {
            return false;
        }

        static constexpr bool is_delayed_recycling()
        {
            return false;
        }

        friend bool operator==(const stateless_allocator&, const stateless_allocator&) = default;
    };
}
