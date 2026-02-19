/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <AzCore/std/createdestroy.h>
#include <AzCore/std/iterator.h>
#include <AzCore/std/functional_basic.h>
#include <AzCore/std/typetraits/common_type.h>
#include <AzCore/std/typetraits/remove_cvref.h>
#include <math.h>
#include <cmath>

#include <algorithm>

namespace AZStd
{
    /**
    * \page Algorithms
    * \subpage SortAlgorithms Sort Algorithms
    *
    * Search algorithms
    */

    //////////////////////////////////////////////////////////////////////////
    // Min, max, clamp
    template<class T>
    constexpr T GetMin(const T& left, const T& right) { return (left < right) ? left : right; }

    using std::min;

    template<class T>
    constexpr T GetMax(const T& left, const T& right) { return (left > right) ? left : right; }

    using std::max;

    template<class T, class Compare>
    constexpr pair<T, T> minmax AZ_PREVENT_MACRO_SUBSTITUTION (const T& left, const T& right, Compare comp) { return comp(right, left) ? AZStd::make_pair(right, left) : AZStd::make_pair(left, right); }

    template<class T>
    constexpr pair<T, T> minmax AZ_PREVENT_MACRO_SUBSTITUTION (const T& left, const T& right) { return AZStd::minmax(left, right, AZStd::less<T>()); }

    using ::floorf;
    using ::ceilf;
    using ::roundf;
    using ::rintf;

    /*
    Finds the smallest and greatest element in the range of [first, last)
    returns a pair consisting of an iterator to the smallest element in .first and an iterator to the largest element in .second.
    If several elements are equivalent to the smallest element it returns the first such element
    If several elements are equivalent to the greatest element it returns the last such element
    */
    template<class ForwardIt, class Compare>
    constexpr pair<ForwardIt, ForwardIt> minmax_element(ForwardIt first, ForwardIt last, Compare comp)
    {
        pair<ForwardIt, ForwardIt> result(first, first);
        // Check for 0 elements in iterator range and return a pair of (first, first)
        if (first == last)
        {
            return result;
        }

        while (++first != last)
        {
            ForwardIt next = first;
            // 1 element left to iterate
            if (++next == last)
            {
                if (comp(*first, *result.first))
                {
                    result.first = first;
                }
                else if (!comp(*first, *result.second))
                {
                    result.second = first;
                }
            }
            // 2+ elements left to iterate
            else
            {
                if (comp(*next, *first))
                {
                    if (comp(*next, *result.first))
                    {
                        result.first = next;
                    }
                    if (!comp(*first, *result.second))
                    {
                        result.second = first;
                    }
                }
                else
                {
                    if (comp(*first, *result.first))
                    {
                        result.first = first;
                    }
                    if (!comp(*next, *result.second))
                    {
                        result.second = next;
                    }

                }
            }
        }

        return result;
    }

    template<class ForwardIt>
    constexpr pair<ForwardIt, ForwardIt> minmax_element(ForwardIt first, ForwardIt last)
    {
        return AZStd::minmax_element(first, last, AZStd::less<typename iterator_traits<ForwardIt>::value_type>());
    }

    template<class T, class Compare>
    constexpr pair<T, T> minmax AZ_PREVENT_MACRO_SUBSTITUTION (std::initializer_list<T> ilist, Compare comp)
    {
        auto minMaxPair = AZStd::minmax_element(ilist.begin(), ilist.end(), comp);
        return AZStd::make_pair(*minMaxPair.first, *minMaxPair.second);
    }

    template<class T>
    constexpr pair<T, T> minmax AZ_PREVENT_MACRO_SUBSTITUTION (std::initializer_list<T> ilist)
    {
        return AZStd::minmax(ilist, AZStd::less<T>());
    }

    using std::clamp;

    using std::mismatch;

    using std::equal;

    using std::for_each;

    using std::count_if;

    using std::find;

    using std::find_if;

    using std::find_if_not;

    using std::adjacent_find;

    using std::find_first_of;

    using std::find_end;

    using std::all_of;

    using std::any_of;

    using std::none_of;

    using std::transform;

    using std::replace;

    using std::replace_if;

    using std::replace_copy;

    using std::replace_copy_if;

    using std::generate;

    using std::generate_n;

    using std::remove_copy;

    using std::remove_copy_if;

    using std::remove;

    using std::remove_if;

    // Reverse
    // The std::reverse function will be constexpr as of C++20, for now the std:: versions will be aliased
    // into the AZStd namespace
    using std::reverse;

    // Rotate
    // The std::rotate function will be constexpr in C++20
    // Since AZStd code doesn't need it constexpr at the moment, the std:: version will be used
    using std::rotate;

    // nth-element
    using std::nth_element;

    using std::push_heap;

    using std::pop_heap;

    using std::make_heap;

    using std::sort_heap;

    using std::equal_range;

    using std::lower_bound;

    using std::upper_bound;

    using std::search;

    using std::is_sorted;
    using std::unique;
    using std::binary_search;
    using std::search_n;
    using std::set_difference;
    using std::lexicographical_compare;

    //////////////////////////////////////////////////////////////////////////
    /**
    * Endian swapping templates
    * note you can specialize any type anywhere in the code if you fell like it.
    */
    template<typename T, AZStd::size_t size>
    struct endian_swap_impl
    {
        // this function is implemented for each specialization.
        static constexpr void swap_data(T& data);
    };

    // specialization for 1 byte type (don't swap)
    template<typename T>
    struct endian_swap_impl<T, 1>
    {
        static constexpr void swap_data(T& data)  { (void)data; }
    };

    // specialization for 2 byte type
    template<typename T>
    struct endian_swap_impl<T, 2>
    {
        static AZ_FORCE_INLINE void swap_data(T& data)
        {
            union SafeCast
            {
                T               m_data;
                unsigned short  m_ushort;
            };
            SafeCast* sc = (SafeCast*)&data;
            unsigned short x = sc->m_ushort;
            sc->m_ushort = (x >> 8) | (x << 8);
        }
    };

    // specialization for 4 byte type
    template<typename T>
    struct endian_swap_impl<T, 4>
    {
        static AZ_FORCE_INLINE void swap_data(T& data)
        {
            union SafeCast
            {
                T               m_data;
                unsigned int    m_uint;
            };
            SafeCast* sc = (SafeCast*)&data;
            unsigned int x = sc->m_uint;
            sc->m_uint = (x >> 24) | ((x << 8) & 0x00FF0000) | ((x >> 8) & 0x0000FF00) | (x << 24);
        }
    };

#   define AZ_UINT64_CONST(_Value) _Value
#   define AZ_INT64_CONST(_Value) _Value

    template<typename T>
    struct endian_swap_impl<T, 8>
    {
        static AZ_FORCE_INLINE void swap_data(T& data)
        {
            union SafeCast
            {
                T           m_data;
                AZ::u64     m_uint64;
            };
            SafeCast* sc = (SafeCast*)&data;
            AZ::u64 x = sc->m_uint64;
            sc->m_uint64 = (x >> 56) |
                ((x << 40) & AZ_UINT64_CONST(0x00FF000000000000)) |
                ((x << 24) & AZ_UINT64_CONST(0x0000FF0000000000)) |
                ((x << 8)  & AZ_UINT64_CONST(0x000000FF00000000)) |
                ((x >> 8)  & AZ_UINT64_CONST(0x00000000FF000000)) |
                ((x >> 24) & AZ_UINT64_CONST(0x0000000000FF0000)) |
                ((x >> 40) & AZ_UINT64_CONST(0x000000000000FF00)) |
                (x << 56);
        }
    };

    template<typename T>
    AZ_FORCE_INLINE void endian_swap(T& data)
    {
        endian_swap_impl<T, sizeof(T)>::swap_data(data);
    }

    template<typename Iterator>
    AZ_FORCE_INLINE void endian_swap(Iterator first, Iterator last)
    {
        for (; first != last; ++first)
        {
            AZStd::endian_swap(*first);
        }
    }
    //
    //////////////////////////////////////////////////////////////////////////
}
