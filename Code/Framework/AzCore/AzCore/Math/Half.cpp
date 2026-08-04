/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Math/Half.h>

#include <AzCore/std/typetraits/is_trivially_copyable.h>

#include <type_traits>

namespace AZ
{
    static_assert(sizeof(Half) == 2, "Half must be exactly 2 bytes");
    static_assert(alignof(Half) == 2, "Half must be 2 byte aligned");
    static_assert(AZStd::is_trivially_copyable_v<Half>, "Half must be trivially copyable");
    // Reading 16-bit float texel buffers through reinterpret_cast<Half*> is a supported use.
    static_assert(std::is_standard_layout_v<Half>, "Half must be standard layout");
} // namespace AZ
