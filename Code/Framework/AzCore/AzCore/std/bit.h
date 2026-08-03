/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/std/typetraits/is_trivially_copyable.h>
#include <AzCore/std/utils.h>

#include <cstring>

//! AZ_STD_HAS_CONSTEXPR_BIT_CAST is 1 when AZStd::bit_cast can be used in a constant expression.
//! Every toolchain O3DE currently targets provides __builtin_bit_cast (MSVC 19.27+, Clang 9+,
//! GCC 11+), so this is expected to be 1 everywhere; the fallback exists so a new or older
//! toolchain degrades to a runtime-only implementation rather than failing to build.
#if defined(__has_builtin)
#   if __has_builtin(__builtin_bit_cast)
#       define AZ_STD_HAS_CONSTEXPR_BIT_CAST 1
#   endif
#elif defined(_MSC_VER) && _MSC_VER >= 1927
#   define AZ_STD_HAS_CONSTEXPR_BIT_CAST 1
#endif

#if !defined(AZ_STD_HAS_CONSTEXPR_BIT_CAST)
#   define AZ_STD_HAS_CONSTEXPR_BIT_CAST 0
#endif

#if AZ_STD_HAS_CONSTEXPR_BIT_CAST
#   define AZ_STD_BIT_CAST_CONSTEXPR constexpr
#else
#   define AZ_STD_BIT_CAST_CONSTEXPR
#endif

namespace AZStd
{
    //! Reinterprets the object representation of @p from as a @p To.
    //!
    //! This is the well-defined replacement for reinterpret_cast / union punning / memcpy when
    //! reading the bits of one trivially copyable type as another -- for example pulling the
    //! IEEE 754 fields out of a float. Unlike a pointer cast it does not violate strict aliasing,
    //! and where the compiler supports it (see AZ_STD_HAS_CONSTEXPR_BIT_CAST) it works at compile time.
    template<class To, class From>
    AZ_STD_BIT_CAST_CONSTEXPR To bit_cast(const From& from) noexcept
    {
        static_assert(sizeof(To) == sizeof(From), "bit_cast requires both types to be the same size");
        static_assert(AZStd::is_trivially_copyable_v<To>, "bit_cast requires the destination type to be trivially copyable");
        static_assert(AZStd::is_trivially_copyable_v<From>, "bit_cast requires the source type to be trivially copyable");

#if AZ_STD_HAS_CONSTEXPR_BIT_CAST
        return __builtin_bit_cast(To, from);
#else
        static_assert(AZStd::is_trivially_default_constructible_v<To>,
            "Without __builtin_bit_cast the destination type must also be trivially default constructible");
        To result;
        ::memcpy(&result, &from, sizeof(To));
        return result;
#endif
    }
} // namespace AZStd
