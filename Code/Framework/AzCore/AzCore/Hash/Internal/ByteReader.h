/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/base.h>

namespace AZ::Hash::Private
{
    // Assembles a 32-bit little-endian value from four consecutive elements (endian-independent by construction).
    template <typename T>
    [[nodiscard]]
    constexpr u32 ReadLE32(const T* p) noexcept
    {
        return static_cast<u32>(static_cast<u8>(p[0]))
            | (static_cast<u32>(static_cast<u8>(p[1])) << 8)
            | (static_cast<u32>(static_cast<u8>(p[2])) << 16)
            | (static_cast<u32>(static_cast<u8>(p[3])) << 24);
    }

    // Assembles a 64-bit little-endian value from eight consecutive elements.
    template <typename T>
    [[nodiscard]]
    constexpr u64 ReadLE64(const T* p) noexcept
    {
        return static_cast<u64>(ReadLE32(p)) | (static_cast<u64>(ReadLE32(p + 4)) << 32);
    }
} // namespace AZ::Hash::Private
