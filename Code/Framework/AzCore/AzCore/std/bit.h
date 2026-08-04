/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <bit>

namespace AZStd
{
    using std::bit_cast;

    using std::endian;

    using std::rotl;
    using std::rotr;

    using std::countl_zero;
    using std::countl_one;
    using std::countr_zero;
    using std::countr_one;
    using std::popcount;

    using std::has_single_bit;
    using std::bit_ceil;
    using std::bit_floor;
    using std::bit_width;

#if __cpp_lib_byteswap >= 202110L
    // C++23
    using std::byteswap;
#endif
} // namespace AZStd
