/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

namespace AZStd
{
    // Variant constructor #5
    template <class... Types>
    template <class T, class... Args, size_t Index>
        requires is_constructible_v<T, Args...>
    inline constexpr variant<Types...>::variant(in_place_type_t<T>, Args&&... args)
        : m_impl(in_place_index_t<Index>{}, AZStd::forward<Args>(args)...)

    {
    }

    // Variant constructor #6
    template <class... Types>
    template <class T, class U, class... Args, size_t Index>
        requires is_constructible_v<T, std::initializer_list<U>&, Args...>
    inline constexpr variant<Types...>::variant(in_place_type_t<T>, std::initializer_list<U> il, Args&&... args)
        : m_impl(in_place_index_t<Index>{}, il, AZStd::forward<Args>(args)...)
    {
    }

    // std::variant member functions
    // Variant emplace #1
    template <class... Types>
    template <class T, class... Args, size_t Index>
        requires is_constructible_v<T, Args...>
    inline constexpr T& variant<Types...>::emplace(Args&&... args)
    {
        return m_impl.template emplace<Index>(AZStd::forward<Args>(args)...);
    }

    // Variant emplace #2
    template <class... Types>
    template <class T, class U, class... Args, size_t Index>
        requires is_constructible_v<T, std::initializer_list<U>&, Args...>
    inline constexpr T& variant<Types...>::emplace(std::initializer_list<U> il, Args&&... args)
    {
        return m_impl.template emplace<Index>(il, AZStd::forward<Args>(args)...);
    }

    template <class... Types>
    inline constexpr bool variant<Types...>::valueless_by_exception() const
    {
        return m_impl.valueless_by_exception();
    }

    template <class... Types>
    inline constexpr size_t variant<Types...>::index() const
    {
        return m_impl.index();
    }

    // Variant holds_alternative function
    template <class T, class... Types>
    inline constexpr bool holds_alternative(const variant<Types...>& variantInst)
    {
        return variant_detail::holds_alternative_at_index<find_type::find_exactly_one_alternative_v<T, Types...>>(variantInst);
    }

    // Variant AZStd::get<variant> specializations
    template <size_t Index, class... Types>
    inline constexpr variant_alternative_t<Index, variant<Types...>>& get(variant<Types...>& variantInst)
    {
        static_assert(Index < sizeof...(Types), "index is out of bounds of variant alternatives");
        static_assert(!is_void_v<variant_alternative_t<Index, variant<Types...>>>, "Cannot retrieve a variant with alternative void type");
        return variant_detail::generic_get<Index>(variantInst);
    }

    template <size_t Index, class... Types>
    inline constexpr variant_alternative_t<Index, variant<Types...>>&& get(variant<Types...>&& variantInst)
    {
        static_assert(Index < sizeof...(Types), "index is out of bounds of variant alternatives");
        static_assert(!is_void_v<variant_alternative_t<Index, variant<Types...>>>, "Cannot retrieve a variant with alternative void type");
        return variant_detail::generic_get<Index>(AZStd::move(variantInst));
    }

    template <size_t Index, class... Types>
    inline constexpr const variant_alternative_t<Index, variant<Types...>>& get(const variant<Types...>& variantInst)
    {
        static_assert(Index < sizeof...(Types), "index is out of bounds of variant alternatives");
        static_assert(!is_void_v<variant_alternative_t<Index, variant<Types...>>>, "Cannot retrieve a variant with alternative void type");
        return variant_detail::generic_get<Index>(variantInst);
    }

    template <size_t Index, class... Types>
    inline constexpr const variant_alternative_t<Index, variant<Types...>>&& get(const variant<Types...>&& variantInst)
    {
        static_assert(Index < sizeof...(Types), "index is out of bounds of variant alternatives");
        static_assert(!is_void_v<variant_alternative_t<Index, variant<Types...>>>, "Cannot retrieve a variant with alternative void type");
        return variant_detail::generic_get<Index>(AZStd::move(variantInst));
    }

    template <class T, class... Types>
    inline constexpr T& get(variant<Types...>& variantInst)
    {
        static_assert(!is_void_v<T>, "Cannot retrieve a variant with alternative void type");
        return get<find_type::find_exactly_one_alternative_v<T, Types...>>(variantInst);
    }

    template <class T, class... Types>
    inline constexpr T&& get(variant<Types...>&& variantInst)
    {
        static_assert(!is_void_v<T>, "Cannot retrieve a variant with alternative void type");
        return get<find_type::find_exactly_one_alternative_v<T, Types...>>(AZStd::move(variantInst));
    }

    template <class T, class... Types>
    inline constexpr const T& get(const variant<Types...>& variantInst)
    {
        static_assert(!is_void_v<T>, "Cannot retrieve a variant with alternative void type");
        return get<find_type::find_exactly_one_alternative_v<T, Types...>>(variantInst);
    }

    template <class T, class... Types>
    inline constexpr const T&& get(const variant<Types...>&& variantInst)
    {
        static_assert(!is_void_v<T>, "Cannot retrieve a variant with alternative void type");
        return get<find_type::find_exactly_one_alternative_v<T, Types...>>(AZStd::move(variantInst));
    }

    template <size_t Index, class... Types>
    inline constexpr add_pointer_t<variant_alternative_t<Index, variant<Types...>>> get_if(variant<Types...>* variantInst)
    {
        static_assert(Index < sizeof...(Types), "index is out of bounds of variant");
        static_assert(!is_void_v<variant_alternative_t<Index, variant<Types...>>>, "Cannot retrieve a variant with alternative void type");
        return variant_detail::generic_get_if<Index>(variantInst);
    }

    template <size_t Index, class... Types>
    inline constexpr add_pointer_t<const variant_alternative_t<Index, variant<Types...>>> get_if(const variant<Types...>* variantInst)
    {
        static_assert(Index < sizeof...(Types), "index is out of bounds of variant");
        static_assert(!is_void_v<variant_alternative_t<Index, variant<Types...>>>, "Cannot retrieve a variant with alternative void type");
        return variant_detail::generic_get_if<Index>(variantInst);
    }

    template <class T, class... Types>
    inline constexpr add_pointer_t<T> get_if(variant<Types...>* variantInst)
    {
        static_assert(!is_void_v<T>, "Cannot retrieve a variant with alternative void type");
        return get_if<find_type::find_exactly_one_alternative_v<T, Types...>>(variantInst);
    }

    template <class T, class... Types>
    inline constexpr add_pointer_t<const T> get_if(const variant<Types...>* variantInst)
    {
        static_assert(!is_void_v<T>, "Cannot retrieve a variant with alternative void type");
        return get_if<find_type::find_exactly_one_alternative_v<T, Types...>>(variantInst);
    }

    // Variant comparison operators and swap
    template <class... Types>
    inline constexpr bool operator==(const variant<Types...>& lhs, const variant<Types...>& rhs)
    {
        return lhs.index() == rhs.index() && (lhs.valueless_by_exception()
            || variant_detail::visitor::variant::visit_value_at(lhs.index(),
                [](auto&& altLeft, auto&& altRight) -> bool
                {
                    return altLeft == altRight;
                },
                lhs, rhs));
    }

    template <class... Types>
    inline constexpr bool operator!=(const variant<Types...>& lhs, const variant<Types...>& rhs)
    {
        return !operator==(lhs, rhs);
    }

    template <class... Types>
    inline constexpr bool operator<(const variant<Types...>& lhs, const variant<Types...>& rhs)
    {
        return !rhs.valueless_by_exception() && (lhs.valueless_by_exception() || lhs.index() < rhs.index()
            || (lhs.index() == rhs.index() && variant_detail::visitor::variant::visit_value_at(lhs.index(),
                [](auto&& altLeft, auto&& altRight) -> bool
                {
                    return altLeft < altRight;
                },
                lhs, rhs)));
    }

    template <class... Types>
    inline constexpr bool operator>(const variant<Types...>& lhs, const variant<Types...>& rhs)
    {
        return operator<(rhs, lhs);
    }

    template <class... Types>
    inline constexpr bool operator<=(const variant<Types...>& lhs, const variant<Types...>& rhs)
    {
        return !operator>(lhs, rhs);
    }

    template <class... Types>
    inline constexpr bool operator>=(const variant<Types...>& lhs, const variant<Types...>& rhs)
    {
        return !operator<(lhs, rhs);
    }

    template <typename... Types>
    inline constexpr void swap(variant<Types...>& lhs, variant<Types...>& rhs)
    {
        lhs.swap(rhs);
    }

    template <class Visitor, class... VariantTypes>
    inline constexpr decltype(auto) visit(Visitor&& visitor, VariantTypes&&... variants)
    {
        // The following code validates that a variant that is valueless due to an exception
        // being thrown in one of the alternative constructor is not being supplied to Visit
        return variant_detail::visitor::variant::visit_value(AZStd::forward<Visitor>(visitor), AZStd::forward<VariantTypes>(variants)...);
    }

    template <class R, class Visitor, class... VariantTypes>
    inline constexpr R visit(Visitor&& visitor, VariantTypes&&... variants)
    {
        // The following code validates that a variant that is valueless due to an exception
        // being thrown in one of the alternative constructor is not being supplied to Visit
        return variant_detail::visitor::variant::visit_value_r<R>(AZStd::forward<Visitor>(visitor), AZStd::forward<VariantTypes>(variants)...);
    }

    // monostate comparison functions
    inline constexpr bool operator<(monostate, monostate)
    {
        return false;
    }
    inline constexpr bool operator>(monostate, monostate)
    {
        return false;
    }
    inline constexpr bool operator<=(monostate, monostate)
    {
        return true;
    }
    inline constexpr bool operator>=(monostate, monostate)
    {
        return true;
    }
    inline constexpr bool operator==(monostate, monostate)
    {
        return true;
    }
    constexpr bool operator!=(monostate, monostate)
    {
        return false;
    }
} // namespace AZStd
