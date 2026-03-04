/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/std/concepts/concepts.h>
#include <AzCore/std/iterator/const_iterator.h>
#include <AzCore/std/ranges/subrange_fwd.h>
#include <AzCore/std/typetraits/add_pointer.h>
#include <AzCore/std/typetraits/is_convertible.h>
#include <AzCore/std/typetraits/is_lvalue_reference.h>
#include <AzCore/std/typetraits/is_void.h>
#include <AzCore/std/typetraits/is_same.h>
#include <AzCore/std/typetraits/is_signed.h>
#include <AzCore/std/typetraits/is_unsigned.h>
#include <AzCore/std/typetraits/remove_cv.h>
#include <AzCore/std/typetraits/remove_all_extents.h>

#include <algorithm>
#include <ranges>

namespace AZStd
{
    using std::make_reverse_iterator;
}

namespace AZStd::ranges
{
    // Range variable templates - kept as own definitions so downstream code can specialize them
    // in AZStd::ranges namespace. They default to the std::ranges values.
    template<class T>
    inline constexpr bool enable_borrowed_range = std::ranges::enable_borrowed_range<T>;

    template<class T>
    inline constexpr bool disable_sized_range = std::ranges::disable_sized_range<T>;

    namespace Internal
    {
        // Variadic template which maps types to true. For SFINAE
        template <class... Args>
        using sfinae_trigger = true_type;
        template <class... Args>
        constexpr bool sfinae_trigger_v = true;
    }

    // Customization point objects (C++20)
    using std::ranges::begin;
    using std::ranges::end;
    using std::ranges::rbegin;
    using std::ranges::rend;
    using std::ranges::size;
    using std::ranges::ssize;
    using std::ranges::empty;
    using std::ranges::data;
    using std::ranges::cdata;

    // Type aliases (C++20)
    using std::ranges::iterator_t;
    using std::ranges::sentinel_t;
    using std::ranges::range_value_t;
    using std::ranges::range_reference_t;
    using std::ranges::range_difference_t;
    using std::ranges::range_size_t;
    using std::ranges::range_rvalue_reference_t;
    using std::ranges::borrowed_iterator_t;
    using std::ranges::borrowed_subrange_t;

    // C++23 type alias - const_iterator_t (custom, not available in C++20 std)
    template<class R>
    using const_iterator_t = enable_if_t<std::ranges::range<R>, const_iterator<std::ranges::iterator_t<R>>>;

    // Concepts (C++20) - wrapped as constexpr bool variable templates to avoid
    // circular concept dependency issues in Clang (std::ranges concepts are actual
    // C++20 concepts that trigger strict cycle detection during overload resolution)
    using std::ranges::range;
    using std::ranges::borrowed_range;
    using std::ranges::sized_range;
    using std::ranges::output_range;
    using std::ranges::input_range;
    using std::ranges::forward_range;
    using std::ranges::bidirectional_range;
    using std::ranges::random_access_range;
    template<class T>
    /*concept*/ constexpr bool contiguous_range = std::ranges::contiguous_range<T>;
    template<class T>
    /*concept*/ constexpr bool common_range = std::ranges::common_range<T>;
    template<class T>
    /*concept*/ constexpr bool view = std::ranges::view<T>;
    template<class T>
    /*concept*/ constexpr bool viewable_range = std::ranges::viewable_range<T>;

    // C++23 concept - constant_range (custom, not available in C++20 std)
    template<class T>
    concept constant_range = input_range<T> &&
        ::AZStd::Internal::constant_iterator<iterator_t<T>>;

    namespace Internal
    {
        template<class T>
        concept is_lvalue_or_borrowable = is_lvalue_reference_v<T> ||
            enable_borrowed_range<remove_cv_t<T>>;

        template<class R>
            requires input_range<R>
        constexpr auto& possibly_const_range(R& r)
        {
            if constexpr (constant_range<const R> && !constant_range<R>)
            {
                return const_cast<const R&>(r);
            }
            else
            {
                return r;
            }
        }
    }

    // C++23 custom cbegin/cend/crbegin/crend CPOs
    // These use const_iterator/const_sentinel wrapping (not available in C++20 std::ranges)
    namespace Internal
    {
        struct cbegin_fn
        {
            template<class T>
                requires is_lvalue_or_borrowable<T>
            constexpr decltype(auto) operator()(T&& t) const noexcept(noexcept(ranges::begin(possibly_const_range(declval<T&>()))))
            {
                using iterator_type = decltype(ranges::begin(possibly_const_range(t)));
                return const_iterator<iterator_type>(ranges::begin(possibly_const_range(t)));
            }
        };
    }
    inline namespace customization_point_object
    {
        inline constexpr Internal::cbegin_fn cbegin{};
    }

    namespace Internal
    {
        struct cend_fn
        {
            template<class T>
                requires is_lvalue_or_borrowable<T>
            constexpr decltype(auto) operator()(T&& t) const noexcept(noexcept(ranges::end(possibly_const_range(declval<T&>()))))
            {
                using sentinel_type = decltype(ranges::end(possibly_const_range(t)));
                return const_sentinel<sentinel_type>(ranges::end(possibly_const_range(t)));
            }
        };
    }
    inline namespace customization_point_object
    {
        inline constexpr Internal::cend_fn cend{};
    }

    namespace Internal
    {
        struct crbegin_fn
        {
            template<class T>
                requires is_lvalue_or_borrowable<T>
            constexpr auto operator()(T&& t) const noexcept(noexcept(ranges::rbegin(possibly_const_range(declval<T&>()))))
            {
                using iterator_type = decltype(ranges::rbegin(possibly_const_range(t)));
                return const_iterator<iterator_type>(ranges::rbegin(possibly_const_range(t)));
            }
        };
    }
    inline namespace customization_point_object
    {
        inline constexpr Internal::crbegin_fn crbegin{};
    }

    namespace Internal
    {
        struct crend_fn
        {
            template<class T>
                requires is_lvalue_or_borrowable<T>
            constexpr auto operator()(T&& t) const noexcept(noexcept(ranges::rend(possibly_const_range(declval<T&>()))))
            {
                using sentinel_type = decltype(ranges::rend(possibly_const_range(t)));
                return const_sentinel<sentinel_type>(ranges::rend(possibly_const_range(t)));
            }
        };
    }
    inline namespace customization_point_object
    {
        inline constexpr Internal::crend_fn crend{};
    }

    // Classes (C++20)
    using std::ranges::view_interface;
    using std::ranges::view_base;
    using std::ranges::dangling;

    // Variable template - enable_view (own copy for specialization by downstream code)
    template<class T>
    inline constexpr bool enable_view = std::ranges::enable_view<T>;

    // Iterator operations (C++20) - kept as custom CPOs to support AZStd custom iterators
    // that may not satisfy std::input_or_output_iterator but do satisfy AZStd concepts
    namespace Internal
    {
        struct advance_fn
        {
            template<class I>
                requires input_or_output_iterator<I>
            constexpr void operator()(I& i, iter_difference_t<I> n) const
            {
                if constexpr (random_access_iterator<I>)
                {
                    i += n;
                }
                else
                {
                    for (; n > 0; ++i, --n) {}
                    if constexpr (bidirectional_iterator<I>)
                    {
                        for (; n < 0; --i, ++n) {}
                    }
                }
            }

            template<class I, class S>
                requires input_or_output_iterator<I> && sentinel_for<S, I>
            constexpr void operator()(I& i, S bound) const
            {
                if constexpr (assignable_from<I&, S>)
                {
                    i = AZStd::move(bound);
                }
                else if constexpr (sized_sentinel_for<S, I>)
                {
                    operator()(i, bound - i);
                }
                else
                {
                    for (; i != bound; ++i) {}
                }
            }

            template<class I, class S>
                requires input_or_output_iterator<I> && sentinel_for<S, I>
            constexpr iter_difference_t<I> operator()(I& i, iter_difference_t<I> n, S bound) const
            {
                if constexpr (sized_sentinel_for<S, I>)
                {
                    if (const auto dist = bound - i;
                        (n > 0 && n > dist) || (n < 0 && n < dist))
                    {
                        operator()(i, bound);
                        return n - dist;
                    }
                    else if (n != 0)
                    {
                        operator()(i, n);
                        return 0;
                    }
                    return 0;
                }
                else
                {
                    for (; i != bound && n > 0; ++i, --n) {}
                    if constexpr (bidirectional_iterator<I> && same_as<I, S>)
                    {
                        for (; i != bound && n < 0; --i, ++n) {}
                    }
                    return n;
                }
            }
        };
    }
    inline namespace customization_point_object
    {
        inline constexpr Internal::advance_fn advance{};
    }

    namespace Internal
    {
        struct distance_fn
        {
            template<class I, class S>
                requires input_or_output_iterator<I> && sentinel_for<S, I> && (!sized_sentinel_for<S, I>)
            constexpr iter_difference_t<I> operator()(I first, S last) const
            {
                iter_difference_t<I> result{};
                for (; first != last; ++first, ++result) {}
                return result;
            }

            template<class I, class S>
                requires input_or_output_iterator<I> && sentinel_for<S, I> && sized_sentinel_for<S, I>
            constexpr iter_difference_t<I> operator()(const I& first, const S& last) const
            {
                return last - first;
            }

            template<class R>
                requires range<R>
            constexpr range_difference_t<R> operator()(R&& r) const
            {
                if constexpr (sized_range<R>)
                {
                    return ranges::size(r);
                }
                else
                {
                    return operator()(ranges::begin(r), ranges::end(r));
                }
            }
        };
    }
    inline namespace customization_point_object
    {
        inline constexpr Internal::distance_fn distance{};
    }

    namespace Internal
    {
        struct next_fn
        {
            template<class I>
                requires input_or_output_iterator<I>
            constexpr I operator()(I x) const
            {
                ++x;
                return x;
            }

            template<class I>
                requires input_or_output_iterator<I>
            constexpr I operator()(I x, iter_difference_t<I> n) const
            {
                ranges::advance(x, n);
                return x;
            }

            template<class I, class S>
                requires input_or_output_iterator<I> && sentinel_for<S, I>
            constexpr I operator()(I x, S bound) const
            {
                ranges::advance(x, bound);
                return x;
            }

            template<class I, class S>
                requires input_or_output_iterator<I> && sentinel_for<S, I>
            constexpr I operator()(I x, iter_difference_t<I> n, S bound) const
            {
                ranges::advance(x, n, bound);
                return x;
            }
        };
    }
    inline namespace customization_point_object
    {
        inline constexpr Internal::next_fn next{};
    }

    namespace Internal
    {
        struct prev_fn
        {
            template<class I>
                requires bidirectional_iterator<I>
            constexpr I operator()(I x) const
            {
                --x;
                return x;
            }

            template<class I>
                requires bidirectional_iterator<I>
            constexpr I operator()(I x, iter_difference_t<I> n) const
            {
                ranges::advance(x, -n);
                return x;
            }

            template<class I, class S>
                requires input_or_output_iterator<I> && sentinel_for<S, I>
            constexpr I operator()(I x, iter_difference_t<I> n, S bound) const
            {
                ranges::advance(x, -n, bound);
                return x;
            }
        };
    }
    inline namespace customization_point_object
    {
        inline constexpr Internal::prev_fn prev{};
    }

    namespace Internal
    {
        template<class T>
        constexpr bool is_initializer_list = false;
        template<class E>
        constexpr bool is_initializer_list<initializer_list<E>> = true;

        // Exposition-only helper: to-unsigned-like
        // https://eel.is/c++draft/ranges#iota.view
        template<class T>
        constexpr auto to_unsigned_like(T t) noexcept
        {
            return static_cast<make_unsigned_t<T>>(t);
        }
    }

    // Algorithm result types and swap_ranges (C++20, from <algorithm>)
    using std::ranges::in_in_result;
    using std::ranges::swap_ranges_result;
    using std::ranges::swap_ranges;

    // Helper Concepts - https://eel.is/c++draft/ranges#range.utility.helpers
    // These are used by the custom C++23 range adaptor/view implementations
    namespace Internal
    {
        template<bool Const, class T>
        using maybe_const = conditional_t<Const, const T, T>;

        template<class R>
        concept simple_view = view<R> && range<const R>
            && same_as<iterator_t<R>, iterator_t<const R>>
            && same_as<sentinel_t<R>, sentinel_t<const R>>;

        template<class I>
        concept has_arrow = input_iterator<I>
            && (is_pointer_v<I> || requires(I i) { i.operator->(); });

        template<class T, class U>
        concept different_from = !same_as<remove_cvref_t<T>, remove_cvref_t<U>>;
    }
}

// Opening AZStd::ranges::views namespace to provide access to it in AZStd
namespace AZStd::ranges::views{}

namespace AZStd
{
      namespace views = ranges::views;

      //! Adding C++23 from_range_t tag type (custom, not available in C++20 std)
      //! https://eel.is/c++draft/range.utility.conv
      struct from_range_t {};
      inline constexpr from_range_t from_range;
}
