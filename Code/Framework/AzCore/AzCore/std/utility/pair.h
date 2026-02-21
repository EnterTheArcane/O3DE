/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/std/utility/pair_fwd.h>

#include <AzCore/std/typetraits/add_const.h>
#include <AzCore/std/typetraits/is_swappable.h>
#include <AzCore/std/utility/declval.h>
#include <AzCore/std/utility/tuple_concepts.h>

namespace AZStd
{
    using std::tuple_element;
    using std::index_sequence;
}

namespace AZStd
{
    using std::tuple_element_t;
    using std::get;
} // namespace AZStd

namespace AZStd::Internal
{
    template<size_t I, class P, bool TupleElementValid = !is_void_v<tuple_element_t<0, remove_cvref_t<P>>> >
    struct tuple_element_preserve_cvref;

    template<size_t I, class P>
    struct tuple_element_preserve_cvref<I, P, true>
    {
    private:
        static constexpr bool is_lvalue_reference = is_lvalue_reference_v<P>;
        static constexpr bool is_const = is_const_v<remove_reference<P>>;
        using raw_type = tuple_element_t<0, remove_cvref_t<P>>;
        using const_type = conditional_t<is_const, add_const_t<raw_type>, raw_type>;
        using reference_type = conditional_t<is_lvalue_reference, add_lvalue_reference_t<const_type>, add_rvalue_reference_t<const_type>>;
    public:
        using type = reference_type;
    };

    template<class PairType, class P, class = void>
    constexpr bool is_pair_like_constructible_for_t = false;

    template<class T1, class T2, class P>
    constexpr bool is_pair_like_constructible_for_t<pair<T1, T2>, P, enable_if_t<is_constructible_v<T1, typename tuple_element_preserve_cvref<0, P>::type> && is_constructible_v<T2, typename tuple_element_preserve_cvref<1, P>::type>>> = true;

    template<class PairType, class P, class = void>
    constexpr bool is_pair_like_assignable_for_t = false;

    template<class T1, class T2, class P>
    constexpr bool is_pair_like_assignable_for_t<
        pair<T1, T2>,
        P,
        enable_if_t<is_assignable_v<T1&, typename tuple_element_preserve_cvref<0, P>::type> && is_assignable_v<T2&, typename tuple_element_preserve_cvref<1, P>::type> >> = true;

    template<class T1, class T2, class P>
    constexpr bool is_pair_like_assignable_for_t<
        const pair<T1, T2>,
        P,
        enable_if_t<
            is_assignable_v<const T1&, typename tuple_element_preserve_cvref<0, P>::type> && is_assignable_v<const T2&, typename tuple_element_preserve_cvref<1, P>::type>>> =
        true;
}

namespace AZStd
{
    using std::pair;
    using std::swap;
    using std::make_pair;

    // C++23-style swap for proxy reference pairs (P2321R2).
    // When both elements of a pair are references, the pair is a proxy reference type
    // (e.g., returned by proxy iterators like PairIterator). std::swap cannot bind
    // prvalue proxy references to lvalue reference parameters. These overloads accept
    // rvalue references, enabling swap through AZStd::iter_swap and standard algorithms.
    template<class T1, class T2>
    constexpr void swap(pair<T1&, T2&>&& a, pair<T1&, T2&>&& b)
        noexcept(is_nothrow_swappable_v<T1> && is_nothrow_swappable_v<T2>)
    {
        AZStd::swap(a.first, b.first);
        AZStd::swap(a.second, b.second);
    }

    template<class T1, class T2>
    constexpr void swap(pair<T1&, T2&>&& a, pair<T1&, T2&>& b)
        noexcept(is_nothrow_swappable_v<T1> && is_nothrow_swappable_v<T2>)
    {
        AZStd::swap(a.first, b.first);
        AZStd::swap(a.second, b.second);
    }

    template<class T1, class T2>
    constexpr void swap(pair<T1&, T2&>& a, pair<T1&, T2&>&& b)
        noexcept(is_nothrow_swappable_v<T1> && is_nothrow_swappable_v<T2>)
    {
        AZStd::swap(a.first, b.first);
        AZStd::swap(a.second, b.second);
    }
} // namespace AZStd
