/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/std/base.h>

#include <AzCore/std/concepts/concepts_constructible.h>
#include <AzCore/std/concepts/concepts_copyable.h>
#include <AzCore/std/concepts/concepts_movable.h>
#include <AzCore/std/ranges/iter_move.h>
#include <AzCore/std/typetraits/common_reference.h>
#include <AzCore/std/typetraits/conditional.h>
#include <AzCore/std/typetraits/conjunction.h>
#include <AzCore/std/typetraits/integral_constant.h>
#include <AzCore/std/typetraits/is_array.h>
#include <AzCore/std/typetraits/is_class.h>
#include <AzCore/std/typetraits/is_enum.h>
#include <AzCore/std/typetraits/is_integral.h>
#include <AzCore/std/typetraits/is_object.h>
#include <AzCore/std/typetraits/is_lvalue_reference.h>
#include <AzCore/std/typetraits/is_rvalue_reference.h>
#include <AzCore/std/typetraits/is_signed.h>
#include <AzCore/std/typetraits/is_void.h>
#include <AzCore/std/typetraits/remove_extent.h>
#include <AzCore/std/typetraits/void_t.h>

namespace AZStd
{
    using std::indirectly_readable_traits;
    using std::iter_value_t;
    using std::iter_reference_t;
    using std::incrementable_traits;
    using std::iter_difference_t;
    using std::iter_rvalue_reference_t;
    using std::indirectly_readable;
    using std::iter_common_reference_t;

    template<indirectly_readable It>
    using iter_const_reference_t = common_reference_t<const iter_value_t<It>&&, iter_reference_t<It>>;

    using std::indirectly_writable;
    using std::indirectly_movable;
    using std::indirectly_movable_storable;
    using std::indirectly_copyable;
    using std::indirectly_copyable_storable;
}

namespace AZStd
{
    // Bring in std utility functions into AZStd namespace
    using std::forward;
    using std::iterator_traits;
}

// C++20 range traits for iteratable types
namespace AZStd::Internal
{
    // Models the can-reference concept which isn't available until C++20
    // template <class T, class = void>
    template <class T>
    constexpr bool can_reference = true;
    template <>
    inline constexpr bool can_reference<void> = false;

    template <class T, class = void>
    constexpr bool is_primary_template_v = false;
    template <class T>
    constexpr bool is_primary_template_v<T, enable_if_t<is_same_v<T, typename T::_is_primary_template>>> = true;

    template <typename T, typename = void>
    constexpr bool has_element_type_v = false;
    template <typename T>
    constexpr bool has_element_type_v<T, void_t<typename T::element_type>> = true;
}
