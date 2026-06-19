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


namespace AZStd
{
    template<semiregular S>
    class move_sentinel
    {
    public:
        constexpr move_sentinel()
            requires default_initializable<S>
            : m_last{}
        {
        }
        constexpr explicit move_sentinel(S s)
            : m_last{ AZStd::move(s) }
        {
        }

        template<class S2>
            requires convertible_to<const S2&, S>
        constexpr move_sentinel(const move_sentinel<S2>& other)
            : m_last{ other.m_last }
        {
        }

        template<class S2>
            requires assignable_from<S&, const S2&>
        constexpr move_sentinel& operator=(const move_sentinel<S2>& other)
        {
            m_last = other.m_last;
            return *this;
        }

        constexpr S base() const
        {
            return m_last;
        }

        template<class I>
            requires sentinel_for<S, I>
        friend constexpr bool operator==(const move_iterator<I>& x, const move_sentinel& y)
        {
            return x.base() == y.base();
        }
        template<class I>
            requires sentinel_for<S, I>
        friend constexpr bool operator!=(const move_iterator<I>& x, const move_sentinel& y)
        {
            return !operator==(x, y);
        }

        // Swap the compare arguments
        template<class I>
            requires sentinel_for<S, I>
        friend constexpr bool operator==(const move_sentinel& y, const move_iterator<I>& x)
        {
            return operator==(x, y);
        }
        template<class I>
            requires sentinel_for<S, I>
        friend constexpr bool operator!=(const move_sentinel& y, const move_iterator<I>& x)
        {
            return !operator==(x, y);
        }

        template<class I>
            requires sized_sentinel_for<S, I>
        friend constexpr iter_difference_t<I> operator-(const move_sentinel& x, const move_iterator<I>& y)
        {
            return x.base() - y.base();
        }
        template<class I>
            requires sized_sentinel_for<S, I>
        friend constexpr iter_difference_t<I> operator-(const move_iterator<I>& x, const move_sentinel& y)
        {
            return x.base() - y.base();
        }

    private:
        S m_last;
    };
}
