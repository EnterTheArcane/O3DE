/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <cmath>

namespace AZStd
{
    using std::abs;
    using std::acos;
    using std::asin;
    using std::atan2;
    using std::atan;
    using std::ceil;
    using std::cos;
    using std::exp2;
    using std::exp;
    using std::floor;
    using std::fmod;
    using std::lerp;
    using std::llround;
    using std::lround;
    using std::pow;
    using std::round;
    using std::sin;
    using std::sqrt;
    using std::tan;
    using std::trunc;

// When using -ffast-math flag INFs and NaNs are not handled and
// it is expected that std::isinf() and std::isnan() have undefined behaviour.
// In this case we will provide a replacement following IEEE 754 standard.
#ifdef O3DE_USING_FAST_MATH
    constexpr bool isinf(float f) noexcept
    {
        union { float f; uint32_t x; } u = { f };
        return (u.x & 0x7FFFFFFFU) == 0x7F800000U;
    }
    constexpr bool isnan(float f) noexcept
    {
        union { float f; uint32_t x; } u = { f };
        return !isinf(f) && ((u.x) & 0x7F800000U) == 0x7F800000U;
    }
#else
    using std::isinf;
    using std::isnan;
#endif
} // namespace AZStd
