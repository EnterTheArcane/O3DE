/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/std/typetraits/conditional.h>
#include <AzCore/std/typetraits/is_floating_point.h>
#include <AzCore/std/typetraits/is_integral.h>
#include <AzCore/std/typetraits/is_signed.h>
#include <AzCore/std/typetraits/is_unsigned.h>
#include <limits>

namespace NumericCastInternal
{
    template<typename ToType, typename FromType>
        requires (!AZStd::is_integral_v<FromType>) || (!AZStd::is_floating_point_v<ToType>)
    inline constexpr bool UnderflowsToType(const FromType& value)
    {
        return (value < static_cast<FromType>(std::numeric_limits<ToType>::lowest()));
    }

    template<typename ToType, typename FromType>
        requires AZStd::is_integral_v<FromType> && AZStd::is_floating_point_v<ToType>
    inline constexpr bool UnderflowsToType(const FromType& value)
    {
        return (static_cast<ToType>(value) < std::numeric_limits<ToType>::lowest());
    }

    template<typename ToType, typename FromType>
        requires (!AZStd::is_integral_v<FromType>) || (!AZStd::is_floating_point_v<ToType>)
    inline constexpr bool OverflowsToType(const FromType& value)
    {
        return (value > static_cast<FromType>(std::numeric_limits<ToType>::max()));
    }

    template<typename ToType, typename FromType>
        requires AZStd::is_integral_v<FromType> && AZStd::is_floating_point_v<ToType>
    inline constexpr bool OverflowsToType(const FromType& value)
    {
        return (static_cast<ToType>(value) > std::numeric_limits<ToType>::max());
    }

    template<typename ToType, typename FromType>
        requires AZStd::is_integral_v<FromType>
            && AZStd::is_integral_v<ToType>
            && (std::numeric_limits<FromType>::digits <= std::numeric_limits<ToType>::digits)
            && AZStd::is_signed_v<FromType>
            && AZStd::is_unsigned_v<ToType>
    inline constexpr bool FitsInToType(const FromType& value)
    {
        return !NumericCastInternal::UnderflowsToType<ToType>(value);
    }

    template<typename ToType, typename FromType>
        requires AZStd::is_integral_v<FromType>
            && AZStd::is_integral_v<ToType>
            && (std::numeric_limits<FromType>::digits > std::numeric_limits<ToType>::digits)
            && AZStd::is_unsigned_v<FromType>
    inline constexpr bool FitsInToType(const FromType& value)
    {
        return !NumericCastInternal::OverflowsToType<ToType>(value);
    }

    template<typename ToType, typename FromType>
        requires (!AZStd::is_integral_v<FromType> || !AZStd::is_integral_v<ToType>)
            || ((std::numeric_limits<FromType>::digits <= std::numeric_limits<ToType>::digits)
                && (AZStd::is_unsigned_v<FromType> || AZStd::is_signed_v<ToType>))
            || ((std::numeric_limits<FromType>::digits > std::numeric_limits<ToType>::digits) && AZStd::is_signed_v<FromType>)
    inline constexpr bool FitsInToType(const FromType& value)
    {
        return !NumericCastInternal::OverflowsToType<ToType>(value) && !NumericCastInternal::UnderflowsToType<ToType>(value);
    }

}
