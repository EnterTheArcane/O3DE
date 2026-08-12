/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/std/limits.h>
#include <AzCore/std/typetraits/is_integral.h>
#include <AzCore/std/typetraits/is_unsigned.h>

#include <bit>

#if defined(AZ_COMPILER_MSVC)
#include <intrin.h>
#endif

namespace AZStd
{
    using std::bit_cast;
    using std::bit_ceil;
    using std::bit_floor;
    using std::bit_width;
    using std::countl_one;
    using std::countl_zero;
    using std::countr_one;
    using std::countr_zero;
    using std::endian;
    using std::has_single_bit;
    using std::popcount;
    using std::rotl;
    using std::rotr;

#if defined(__cpp_lib_byteswap) && __cpp_lib_byteswap >= 202110L
    using std::byteswap;
#else
    //! Reverses the bytes in an integral value. This supplies the C++23 std::byteswap API when compiling as C++20.
    template<AZStd::integral T>
    [[nodiscard]]
    constexpr T byteswap(const T value) noexcept
    {
        if constexpr (sizeof(T) == 1)
        {
            // This also admits bool, matching the one-byte identity paths in libc++ and the Microsoft STL.
            return value;
        }
        else
        {
            constexpr int BitsPerByte = AZStd::numeric_limits<unsigned char>::digits;
            static_assert(
                AZStd::numeric_limits<T>::digits + AZStd::numeric_limits<T>::is_signed == sizeof(T) * BitsPerByte,
                "byteswap requires an integral type without padding bits");

            using UnsignedType = AZStd::make_unsigned_t<T>;
            const UnsignedType unsignedValue = static_cast<UnsignedType>(value);

#if defined(AZ_COMPILER_MSVC)
            if (az_builtin_is_constant_evaluated())
            {
                if constexpr (sizeof(T) == 2)
                {
                    return static_cast<T>((unsignedValue << 8) | (unsignedValue >> 8));
                }
                else if constexpr (sizeof(T) == 4)
                {
                    return static_cast<T>(
                        (unsignedValue << 24)
                        | ((unsignedValue << 8) & 0x00FF0000u)
                        | ((unsignedValue >> 8) & 0x0000FF00u)
                        | (unsignedValue >> 24));
                }
                else if constexpr (sizeof(T) == 8)
                {
                    return static_cast<T>(
                        (unsignedValue << 56)
                        | ((unsignedValue << 40) & 0x00FF000000000000ull)
                        | ((unsignedValue << 24) & 0x0000FF0000000000ull)
                        | ((unsignedValue << 8) & 0x000000FF00000000ull)
                        | ((unsignedValue >> 8) & 0x00000000FF000000ull)
                        | ((unsignedValue >> 24) & 0x0000000000FF0000ull)
                        | ((unsignedValue >> 40) & 0x000000000000FF00ull)
                        | (unsignedValue >> 56));
                }
            }

            if constexpr (sizeof(T) == 2)
            {
                return static_cast<T>(_byteswap_ushort(static_cast<unsigned short>(unsignedValue)));
            }
            else if constexpr (sizeof(T) == 4)
            {
                return static_cast<T>(_byteswap_ulong(static_cast<unsigned long>(unsignedValue)));
            }
            else if constexpr (sizeof(T) == 8)
            {
                return static_cast<T>(_byteswap_uint64(static_cast<unsigned long long>(unsignedValue)));
            }
#elif defined(AZ_COMPILER_CLANG) || defined(AZ_COMPILER_GCC)
            if constexpr (sizeof(T) == 2)
            {
                return static_cast<T>(__builtin_bswap16(unsignedValue));
            }
            else if constexpr (sizeof(T) == 4)
            {
                return static_cast<T>(__builtin_bswap32(unsignedValue));
            }
            else if constexpr (sizeof(T) == 8)
            {
                return static_cast<T>(__builtin_bswap64(unsignedValue));
            }
#endif

            constexpr UnsignedType ByteMask = static_cast<UnsignedType>(AZStd::numeric_limits<unsigned char>::max());
            UnsignedType remainingValue = unsignedValue;
            UnsignedType swappedValue{};
            for (decltype(sizeof(T)) byteIndex = 0; byteIndex < sizeof(T); ++byteIndex)
            {
                swappedValue = static_cast<UnsignedType>((swappedValue << BitsPerByte) | (remainingValue & ByteMask));
                remainingValue >>= BitsPerByte;
            }
            return static_cast<T>(swappedValue);
        }
    }
#endif
} // namespace AZStd
