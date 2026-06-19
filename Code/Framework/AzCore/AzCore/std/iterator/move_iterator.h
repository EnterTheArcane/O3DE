/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/std/base.h>
#include <AzCore/std/concepts/concepts.h>
#include <AzCore/std/iterator/iterator_primitives.h>

namespace AZStd
{
    // Bring in std utility functions into AZStd namespace
    using std::forward;
}

namespace AZStd::Internal
{
    template<class I, class = void>
    struct move_iterator_iter_category {};

    template<class I>
        requires requires { typename ITER_TRAITS<I>::iterator_category; }
    struct move_iterator_iter_category<I>
    {
        using iterator_category = conditional_t<
            derived_from<typename ITER_TRAITS<I>::iterator_category, random_access_iterator_tag>,
            random_access_iterator_tag,
            typename ITER_TRAITS<I>::iterator_category
        >;
    };
}

namespace AZStd
{
    // C++20 compliant move_iterator implementation
    // https://eel.is/c++draft/iterators#move.iterators
    template<input_or_output_iterator I>
    class move_iterator
        : public Internal::move_iterator_iter_category<I>
    {
    public:
        using iterator_type = I;
        using iterator_concept = conditional_t<random_access_iterator<I>, random_access_iterator_tag,
                conditional_t<bidirectional_iterator<I>, bidirectional_iterator_tag,
                conditional_t<forward_iterator<I>, forward_iterator_tag, input_iterator_tag>
            >
        >;
        using value_type = iter_value_t<I>;
        using difference_type = iter_difference_t<I>;
        using pointer = I;
        using reference = iter_rvalue_reference_t<I>;

        constexpr move_iterator()
            requires default_initializable<I>
        = default;
        constexpr move_iterator(I i)
            : m_current{ AZStd::move(i) }
        {
        }

        template<class I2>
            requires (!same_as<I2, I>)
                && convertible_to<const I2&, I>
        constexpr move_iterator(const move_iterator<I2>& other)
            : m_current{ other.m_current }
        {
        }

        template<class I2>
            requires (!same_as<I2, I>)
                && convertible_to<const I2&, I>
        constexpr move_iterator& operator=(const move_iterator<I2>& other)
        {
            m_current = other.m_current;
            return *this;
        }

        constexpr const I& base() const& noexcept
        {
            return m_current;
        }

        constexpr I base() &&
        {
            return AZStd::move(m_current);
        }

        constexpr reference operator*() const
        {
            return ranges::iter_move(m_current);
        }

        constexpr reference operator[](difference_type n) const
        {
            return ranges::iter_move(m_current + n);
        }

        constexpr move_iterator& operator++()
        {
            ++m_current;
            return *this;
        }
        constexpr auto operator++(int)
        {
            if constexpr (forward_iterator<I>)
            {
                auto tmp = *this;
                ++m_current;
                return tmp;
            }
            else
            {
                ++m_current;
            }
        }

        constexpr move_iterator& operator--()
        {
            --m_current;
            return *this;
        }
        constexpr move_iterator operator--(int)
        {
            auto tmp = *this;
            --m_current;
            return tmp;
        }

        constexpr move_iterator operator+(difference_type n) const
        {
            return move_iterator(m_current + n);
        }
        constexpr move_iterator& operator+=(difference_type n)
        {
            m_current += n;
            return *this;
        }

        constexpr move_iterator operator-(difference_type n) const
        {
            return move_iterator(m_current - n);
        }
        constexpr move_iterator& operator-=(difference_type n)
        {
            m_current -= n;
            return *this;
        }

        // comparison
        template<class I2>
            requires equality_comparable_with<I, I2>
        friend constexpr bool operator==(const move_iterator& x, const move_iterator<I2>& y)
        {
            return x.base() == y.base();

        }
        template<class I2>
            requires equality_comparable_with<I, I2>
        friend constexpr bool operator!=(const move_iterator& x, const move_iterator<I2>& y)
        {
            return !operator==(x, y);
        }

        template<class I2>
            requires requires { { declval<I>() < declval<I2>() } -> convertible_to<bool>; }
        friend constexpr bool operator<(const move_iterator& x, const move_iterator<I2>& y)
        {
            return x.base() < y.base();
        }
        template<class I2>
            requires requires { { declval<I>() < declval<I2>() } -> convertible_to<bool>; }
        friend constexpr bool operator>(const move_iterator& x, const move_iterator<I2>& y)
        {
            return operator<(y, x);
        }
        template<class I2>
            requires requires { { declval<I>() < declval<I2>() } -> convertible_to<bool>; }
        friend constexpr bool operator<=(const move_iterator& x, const move_iterator<I2>& y)
        {
            return !operator<(y, x);
        }
        template<class I2>
            requires requires { { declval<I>() < declval<I2>() } -> convertible_to<bool>; }
        friend constexpr bool operator>=(const move_iterator& x, const move_iterator<I2>& y)
        {
            return !operator<(x, y);
        }

        template<class I2>
        friend constexpr auto operator-(const move_iterator& x, const move_iterator<I2>& y)
            -> decltype(declval<const I&>() - declval<const I2&>())
        {
            return x.base() - y.base();
        }

        friend constexpr reference iter_move(const move_iterator& i)
            noexcept(noexcept(ranges::iter_move(declval<const I&>())))
        {
            return ranges::iter_move(i.base());
        }

        template<class I2>
            requires indirectly_swappable<I2, I>
        friend constexpr void iter_swap(const move_iterator& x, const move_iterator<I2>& y)
            noexcept(noexcept(ranges::iter_swap(declval<const I&>(), declval<const I2&>())))
        {
            return ranges::iter_swap(x.base(), y.base());
        }

    private:
        I m_current{};
    };

    template <class I>
    constexpr move_iterator<I> make_move_iterator(I i)
    {
        return move_iterator<I>(i);
    }
}
