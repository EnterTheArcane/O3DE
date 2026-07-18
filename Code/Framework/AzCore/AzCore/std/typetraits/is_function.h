/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <AzCore/std/typetraits/integral_constant.h>
#include <AzCore/std/typetraits/is_pointer.h>
#include <AzCore/std/typetraits/remove_pointer.h>

namespace AZStd
{
    using std::is_function;
    using std::is_function_v;

    template<class T>
    struct is_function_pointer
        : AZStd::bool_constant<AZStd::is_pointer_v<T> && AZStd::is_function_v<AZStd::remove_pointer_t<T>>>
    {};

    template<class T>
    constexpr bool is_function_pointer_v = is_function_pointer<T>::value;
}
