/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


#pragma once

#include <algorithm>  // std::swap()
#include <limits>  // std::numeric_limits
#include <type_traits>  // std::make_unsigned
#include "BaseTypes.h"  // uint32, uint64
#include <AzCore/Math/MathUtils.h>  // AZ::InvSqrt (retained scalar helpers only)


namespace CryRandom_Internal
{
    template <class R, class T, size_t size>
    struct BoundedRandomUint
    {
        static_assert(std::numeric_limits<T>::is_integer);
        static_assert(!std::numeric_limits<T>::is_signed);
        static_assert(sizeof(T) == size);
        static_assert(sizeof(T) <= sizeof(uint32));

        inline static T Get(R& randomGenerator, const T maxValue)
        {
            const uint32 r = randomGenerator.GenerateUint32();
            // Note that the computation below is biased. An alternative computation
            // (also biased): uint32((uint64)r * ((uint64)maxValue + 1)) >> 32)
            return (T)((uint64)r % ((uint64)maxValue + 1));
        }
    };

    template <class R, class T>
    struct BoundedRandomUint<R, T, 8>
    {
        static_assert(std::numeric_limits<T>::is_integer);
        static_assert(!std::numeric_limits<T>::is_signed);
        static_assert(sizeof(T) == sizeof(uint64));

        inline static T Get(R& randomGenerator, const T maxValue)
        {
            const uint64 r = randomGenerator.GenerateUint64();
            if (maxValue >= (std::numeric_limits<uint64>::max)())
            {
                return r;
            }
            // Note that the computation below is biased.
            return (T)(r % ((uint64)maxValue + 1));
        }
    };

    //////////////////////////////////////////////////////////////////////////

    template <class R, class T, bool bInteger = std::numeric_limits<T>::is_integer>
    struct BoundedRandom;

    template <class R, class T>
    struct BoundedRandom<R, T, true>
    {
        static_assert(std::numeric_limits<T>::is_integer);
        typedef typename std::make_unsigned<T>::type UT;
        static_assert(sizeof(T) == sizeof(UT));
        static_assert(std::numeric_limits<UT>::is_integer);
        static_assert(!std::numeric_limits<UT>::is_signed);

        inline static T Get(R& randomGenerator, T minValue, T maxValue)
        {
            if (minValue > maxValue)
            {
                std::swap(minValue, maxValue);
            }
            return (T)((UT)minValue + (UT)BoundedRandomUint<R, UT, sizeof(UT)>::Get(randomGenerator, (UT)(maxValue - minValue)));
        }
    };

    template <class R, class T>
    struct BoundedRandom<R, T, false>
    {
        static_assert(!std::numeric_limits<T>::is_integer);

        inline static T Get(R& randomGenerator, const T minValue, const T maxValue)
        {
            return minValue + (maxValue - minValue) * randomGenerator.GenerateFloat();
        }
    };

    // CryCommon->AzCore migration: the BoundedRandomComponentwise / GetRandomUnitVector vector
    // helpers were removed (dead — never instantiated, and built on the retired Vec*_tpl API).
} // namespace CryRandom_Internal

// eof
