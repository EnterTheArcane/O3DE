/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Math/ColorHalf.h>

#include <AzCore/std/typetraits/is_trivially_copyable.h>

#include <type_traits>

namespace AZ
{
    static_assert(sizeof(ColorHalf) == 8, "ColorHalf must be exactly 8 bytes");
    static_assert(alignof(ColorHalf) == 8, "ColorHalf must be 8 byte aligned");
    static_assert(AZStd::is_trivially_copyable_v<ColorHalf>, "ColorHalf must be trivially copyable");
    static_assert(std::is_standard_layout_v<ColorHalf>, "ColorHalf must be standard layout");
} // namespace AZ
