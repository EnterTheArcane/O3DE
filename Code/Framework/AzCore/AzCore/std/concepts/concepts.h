/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <AzCore/std/concepts/concepts_assignable.h>
#include <AzCore/std/concepts/concepts_constructible.h>
#include <AzCore/std/concepts/concepts_copyable.h>
#include <AzCore/std/concepts/concepts_movable.h>
#include <AzCore/std/function/invoke.h>
#include <AzCore/std/iterator/iterator_primitives.h>
#include <AzCore/std/ranges/swap.h>
#include <AzCore/std/ranges/iter_move.h>
#include <AzCore/std/ranges/iter_swap.h>

#include <AzCore/std/typetraits/add_pointer.h>
#include <AzCore/std/typetraits/common_reference.h>
#include <AzCore/std/typetraits/is_array.h>
#include <AzCore/std/typetraits/is_class.h>
#include <AzCore/std/typetraits/is_enum.h>
#include <AzCore/std/typetraits/is_function.h>
#include <AzCore/std/typetraits/is_integral.h>
#include <AzCore/std/typetraits/is_object.h>
#include <AzCore/std/typetraits/is_same.h>
#include <AzCore/std/typetraits/is_signed.h>
#include <AzCore/std/typetraits/is_void.h>
#include <AzCore/std/typetraits/remove_cvref.h>
#include <AzCore/std/typetraits/void_t.h>
#include <AzCore/std/utility/declval.h>
#include <AzCore/std/utility/move.h>

namespace AZStd
{
    using std::pointer_traits;

    using std::input_iterator_tag;
    using std::output_iterator_tag;
    using std::forward_iterator_tag;
    using std::bidirectional_iterator_tag;
    using std::random_access_iterator_tag;
    using std::contiguous_iterator_tag;

    namespace Internal
    {
        // Variadic template which maps types to true For SFINAE
        template<class... Args>
        using sfinae_trigger = true_type;
        template<class... Args>
        constexpr bool sfinae_trigger_v = true;
    }

    //! Implements the C++20 to_address function
    //! This obtains the address represented by ptr without forming a reference
    //! to the pointee type
    namespace Internal
    {
        // pointer_traits isn't SFINAE friendly https://cplusplus.github.io/LWG/lwg-active.html#3545
        // working around that by checking if type T has an element_type alias
        template<class T>
        concept pointer_traits_has_to_address =
            has_element_type_v<T>
            && !is_void_v<decltype(pointer_traits<T>::to_address(declval<const T&>()))>;

        // fancy pointer helper
        template<class T>
        concept to_address_has_arrow_operator = !is_void_v<decltype(declval<const T&>().operator->())>;

        template<class T>
        concept to_address_fancy_pointer =
            to_address_has_arrow_operator<T>
            || pointer_traits_has_to_address<T>;

        struct to_address_fn
        {
            template<class T>
            constexpr T* operator()(T* ptr) const noexcept
            {
                static_assert(!AZStd::is_function_v<T>, "Invoking to_address on a function pointer is not allowed");
                return ptr;
            }

            //! Fancy pointer overload which delegates to using a specialization of pointer_traits<T>::to_address
            //! if that is a well-formed expression, otherwise it returns ptr->operator->()
            //! For example invoking `to_address(AZStd::reverse_iterator<const char*>(char_ptr))`
            //! Returns an element of type const char*
            template<class T> requires
                (!is_pointer_v<T>)
                && (!is_array_v<T>)
                && (!is_function_v<T>)
                && to_address_fancy_pointer<T>
            constexpr auto operator()(const T& ptr) const noexcept
            {
                if constexpr (pointer_traits_has_to_address<T>)
                {
                    return pointer_traits<T>::to_address(ptr);
                } else
                {
                    return ptr.operator->();
                }
            }
        };
    }

    // NOTE: Do not replace with `using std::to_address` yet.
    // `std::to_address` can instantiate `std::pointer_traits<T>` in non-SFINAE-friendly ways
    // on currently supported toolchains (see LWG3545 discussion above), and this header uses
    // `to_address` from concept/SFINAE-heavy contexts. That turns substitution failure into hard
    // errors and causes widespread breakage.
    inline namespace customization_point_object
    {
        constexpr Internal::to_address_fn to_address{};
    }

    namespace Internal
    {
        template<class T, class U>
        concept different_from = !same_as<remove_cvref_t<T>, remove_cvref_t<U>>;

        template<class It>
        concept is_class_or_enum = disjunction_v<is_class<remove_cvref_t<It>>, is_enum<remove_cvref_t<It>>>;

        template<class T, class U>
        concept common_with_impl =
            requires
            {
                static_cast<common_type_t<T, U>>(declval<T>());
                static_cast<common_type_t<T, U>>(declval<U>());
            }
            && same_as<common_type_t<T, U>, common_type_t<U, T>>
            && common_reference_with<add_lvalue_reference_t<const T>, add_lvalue_reference_t<const U>>
            && common_reference_with<add_lvalue_reference_t<common_type_t<T, U>>, common_reference_t<add_lvalue_reference_t<const T>, add_lvalue_reference_t<const U>>>;
    }

    using std::common_with;
    using std::derived_from;

    using std::signed_integral;
    using std::unsigned_integral;

    using std::equality_comparable;
    using std::equality_comparable_with;

    namespace Internal
    {
        // boolean-testable concept (exposition only in the C++standard)
        template<class T>
        concept boolean_testable_impl = convertible_to<T, bool>;

        template<class T>
        concept boolean_testable =
            boolean_testable_impl<T>
            && requires(T&& t) { { !forward<T>(t) } -> boolean_testable_impl; };

        // weakly comparable ==, !=
        template<class T, class U>
        concept weakly_equality_comparable_with =
            requires(const remove_reference_t<T>& t, const remove_reference_t<U>& u)
            {
                { t == u } -> boolean_testable;
                { t != u } -> boolean_testable;
                { u == t } -> boolean_testable;
                { u != t } -> boolean_testable;
            };

        // partially ordered <, >, <=, >=
        template<class T, class U>
        concept partially_ordered_with_impl =
            requires(const remove_reference_t<T>& t, const remove_reference_t<U>& u)
            {
                { t < u } -> boolean_testable;
                { t > u } -> boolean_testable;
                { t <= u } -> boolean_testable;
                { t >= u } -> boolean_testable;
                { u < t } -> boolean_testable;
                { u > t } -> boolean_testable;
                { u <= t } -> boolean_testable;
                { u >= t } -> boolean_testable;
            };
    }

    template<class T, class U>
    concept partially_ordered_with = Internal::partially_ordered_with_impl<T, U>;

    using std::totally_ordered;
    using std::totally_ordered_with;

    using std::default_initializable;

    using std::semiregular;
    using std::regular;

    namespace Internal
    {
        template<class T>
        concept is_integer_like = integral<T> && !same_as<T, bool>;

        template<class T>
        concept is_signed_integer_like = signed_integral<T>;

        template<class T>
        concept weakly_incrementable_impl =
            movable<T>
            && is_signed_integer_like<iter_difference_t<T>>
            && requires(T t)
            {
                { ++t } -> same_as<T&>;
                t++;
            };
    }

    using std::weakly_incrementable;
    using std::input_or_output_iterator;

    using std::incrementable;

    template<class S, class I>
    /*concept*/ constexpr bool sentinel_for =
        semiregular<S>
        && input_or_output_iterator<I>
        && Internal::weakly_equality_comparable_with<S, I>;

    using std::disable_sized_sentinel_for;

    namespace Internal
    {
        template<class S, class I, class = void>
        /*concept*/ constexpr bool sized_sentinel_for_impl = false;
        template<class S, class I>
        /*concept*/ constexpr bool sized_sentinel_for_impl<S, I, enable_if_t<
            sentinel_for<S, I>
            && !disable_sized_sentinel_for<remove_cv_t<S>, remove_cv_t<I>>
            && same_as<decltype(declval<S>() - declval<I>()), iter_difference_t<I>>
            && same_as<decltype(declval<I>() - declval<S>()), iter_difference_t<I>>>> = true;
    }

    template<class S, class I>
    /*concept*/ constexpr bool sized_sentinel_for = Internal::sized_sentinel_for_impl<S, I>;

    namespace Internal
    {
        // ITER_TRAITS(I) general concept
        template<class I>
        using ITER_TRAITS = conditional_t<is_primary_template_v<iterator_traits<I>>, I, iterator_traits<I>>;

        // ITER_CONCEPT(I) general concept
        template<class I>
        concept use_traits_iterator_concept_for_concept = requires { typename ITER_TRAITS<I>::iterator_concept; };

        template<class I>
        concept use_traits_iterator_category_for_concept =
            requires { typename ITER_TRAITS<I>::iterator_category; }
            && !use_traits_iterator_concept_for_concept<I>;

        template<class I>
        concept use_random_access_iterator_tag_for_concept =
            requires { typename ITER_TRAITS<I>; }
            && !use_traits_iterator_concept_for_concept<I>
            && !use_traits_iterator_category_for_concept<I>;
    }

    using std::input_iterator;
    using std::output_iterator;
    using std::forward_iterator;
    using std::bidirectional_iterator;
    using std::random_access_iterator;
    using std::contiguous_iterator;

    namespace Internal
    {
        // cpp17 iterator concepts
        // https://eel.is/c++draft/iterator.traits#2
        template<class I>
        concept cpp17_iterator_impl =
            copyable<I>
            && requires(I i)
            {
                *i;
                requires can_reference<decltype(*i)>;
                { ++i } -> same_as<I&>;
                *i++;
                requires can_reference<decltype(*i++)>;
            };
    }

    // cpp17-iterator concept
    template<class I>
    concept cpp17_iterator = Internal::cpp17_iterator_impl<I>;

    namespace Internal
    {
        template<class I>
        concept cpp17_input_iterator_impl =
            cpp17_iterator<I>
            && equality_comparable<I>
            && signed_integral<typename incrementable_traits<I>::difference_type>
            && requires(I i)
            {
                typename incrementable_traits<I>::difference_type;
                typename indirectly_readable_traits<I>::value_type;
                typename common_reference_t<iter_reference_t<I>&&, typename indirectly_readable_traits<I>::value_type&>;
                { *i++ } -> common_reference_with<typename indirectly_readable_traits<I>::value_type&>;
            };
    }

    // cpp17-input-iterator concept
    template<class I>
    concept cpp17_input_iterator = Internal::cpp17_input_iterator_impl<I>;

    namespace Internal
    {
        template<class I>
        concept cpp17_forward_iterator_impl =
            cpp17_input_iterator<I>
            && constructible_from<I>
            && is_lvalue_reference_v<iter_reference_t<I>>
            && same_as<remove_cvref_t<iter_reference_t<I>>, typename indirectly_readable_traits<I>::value_type>
            && requires(I i)
            {
                { i++ } -> convertible_to<const I&>;
                { *i++ } -> same_as<iter_reference_t<I>>;
            };
    }

    // cpp17-forward-iterator concept
    template<class I>
    concept cpp17_forward_iterator = Internal::cpp17_forward_iterator_impl<I>;

    namespace Internal
    {
        template<class I>
        concept cpp17_bidirectional_iterator_impl =
            cpp17_forward_iterator<I>
            && requires(I i)
            {
                { --i } -> same_as<I&>;
                { i-- } -> convertible_to<const I&>;
                { *i-- } -> same_as<iter_reference_t<I>>;
            };
    }

    // cpp17-bidirectional-iterator concept
    template<class I>
    concept cpp17_bidirectional_iterator = Internal::cpp17_bidirectional_iterator_impl<I>;
}

namespace AZStd
{
    namespace Internal
    {
        template<class I>
        concept cpp17_random_access_iterator_impl =
            cpp17_bidirectional_iterator<I>
            && totally_ordered<I>
            && requires(I i, typename incrementable_traits<I>::difference_type n)
            {
                { i += n } -> same_as<I&>;
                { i -= n } -> same_as<I&>;
                { i + n } -> same_as<I>;
                { n + i } -> same_as<I>;
                { i - n } -> same_as<I>;
                { i - i } -> same_as<typename incrementable_traits<I>::difference_type>;
                { i[n] } -> same_as<iter_reference_t<I>>;
            };
    }

    // cpp17-random_access-iterator concept
    template<class I>
    concept cpp17_random_access_iterator = Internal::cpp17_random_access_iterator_impl<I>;

    using std::predicate;
    using std::relation;
    using std::equivalence_relation;
    using std::strict_weak_order;

    using std::indirectly_unary_invocable;
    using std::indirectly_regular_unary_invocable;

    using std::indirect_unary_predicate;
    using std::indirect_binary_predicate;

    using std::indirect_equivalence_relation;
    using std::indirect_strict_weak_order;

    using std::indirect_result_t;

    using std::projected;
}
