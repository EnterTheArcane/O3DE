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

    template<class T>
    constexpr T min AZ_PREVENT_MACRO_SUBSTITUTION (const T& left, const T& right) { return (left < right) ? left : right; }

    template<class T>
    constexpr T GetMax(const T& left, const T& right) { return (left > right) ? left : right; }

    template<class T>
    constexpr T max AZ_PREVENT_MACRO_SUBSTITUTION (const T& left, const T& right) { return (left > right) ? left : right; }

    template<class T, class Compare>
    constexpr pair<T, T> minmax AZ_PREVENT_MACRO_SUBSTITUTION (const T& left, const T& right, Compare comp) { return comp(right, left) ? AZStd::make_pair(right, left) : AZStd::make_pair(left, right); }

    template<class T>
    constexpr pair<T, T> minmax AZ_PREVENT_MACRO_SUBSTITUTION (const T& left, const T& right) { return AZStd::minmax(left, right, AZStd::less<T>()); }

    using std::minmax_element;

    using ::floorf;
    using ::ceilf;
    using ::roundf;
    using ::rintf;

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

    template<class T>
    constexpr T clamp(const T& val, const T& lower, const T& upper) { return GetMin(upper, GetMax(val, lower)); }

    namespace Internal
    {
    }

    using std::mismatch;
    using std::equal;
    using std::for_each;

    using std::count_if;

    //////////////////////////////////////////////////////////////////////////
    // Find
    using std::find;

    using std::find_if;

    using std::find_if_not;

    using std::adjacent_find;
    using std::find_first_of;

    // find_end for forward iterators.
    // find_end for bidirectional iterators.

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

    //////////////////////////////////////////////////////////////////////////
    // Heap
    // \todo move to heap.h
    namespace Internal
    {
        template <class RandomAccessIterator, class Distance, class T>
        constexpr void push_heap(RandomAccessIterator first, Distance holeIndex, Distance topIndex, const T& value)
        {
            Distance parent = (holeIndex - 1) / 2;
            while (holeIndex > topIndex && *(first + parent) < value)
            {
                *(first + holeIndex) = *(first + parent);
                holeIndex = parent;
                parent = (holeIndex - 1) / 2;
            }
            *(first + holeIndex) = value;
        }

        template <class RandomAccessIterator, class Distance, class T, class Compare>
        constexpr void push_heap(RandomAccessIterator first, Distance holeIndex, Distance topIndex, const T& value, Compare comp)
        {
            Distance parent = (holeIndex - 1) / 2;
            while (holeIndex > topIndex && comp(*(first + parent), value))
            {
                *(first + holeIndex) = *(first + parent);
                holeIndex = parent;
                parent = (holeIndex - 1) / 2;
            }
            *(first + holeIndex) = value;
        }

        template <class RandomAccessIterator, class Distance, class T>
        constexpr void adjust_heap(RandomAccessIterator first, Distance holeIndex, Distance length, const T& value)
        {
            Distance topIndex = holeIndex;
            Distance secondChild = 2 * holeIndex + 2;
            while (secondChild < length)
            {
                if (*(first + secondChild) < *(first + (secondChild - 1)))
                {
                    --secondChild;
                }
                *(first + holeIndex) = *(first + secondChild);
                holeIndex = secondChild;
                secondChild = 2 * (secondChild + 1);
            }
            if (secondChild == length)
            {
                *(first + holeIndex) = *(first + (secondChild - 1));
                holeIndex = secondChild - 1;
            }
            AZStd::Internal::push_heap(first, holeIndex, topIndex, value);
        }

        template <class RandomAccessIterator, class Distance, class T, class Compare>
        constexpr void adjust_heap(RandomAccessIterator first, Distance holeIndex, Distance length, const T& value, Compare comp)
        {
            Distance topIndex = holeIndex;
            Distance secondChild = 2 * holeIndex + 2;
            while (secondChild < length)
            {
                if (comp(*(first + secondChild), *(first + (secondChild - 1))))
                {
                    --secondChild;
                }
                *(first + holeIndex) = *(first + secondChild);
                holeIndex = secondChild;
                secondChild = 2 * (secondChild + 1);
            }
            if (secondChild == length)
            {
                *(first + holeIndex) = *(first + (secondChild - 1));
                holeIndex = secondChild - 1;
            }
            AZStd::Internal::push_heap(first, holeIndex, topIndex, value, comp);
        }
    }

    /**
    * \defgroup Heaps Heap functions
    * @{
    */

    using std::push_heap;
    using std::pop_heap;
    using std::make_heap;
    using std::sort_heap;

    /// [Extension] Same as AZStd::pop_heap using AZStd::less predicate, but allows you to provide iterator where to store the result.
    template <class RandomAccessIterator>
    constexpr void pop_heap(RandomAccessIterator first, RandomAccessIterator last, RandomAccessIterator result)
    {
        typename AZStd::iterator_traits<RandomAccessIterator>::value_type value = *result;
        *result = *first;
        AZStd::Internal::adjust_heap(first, typename AZStd::iterator_traits<RandomAccessIterator>::difference_type(0), typename AZStd::iterator_traits<RandomAccessIterator>::difference_type(last - first), value);
    }
    /// [Extension] Same as AZStd::pop_heap using Compare predicate, but allows you to provide iterator where to store the result.
    template <class RandomAccessIterator, class Compare>
    constexpr void pop_heap(RandomAccessIterator first, RandomAccessIterator last, RandomAccessIterator result, Compare comp)
    {
        typename AZStd::iterator_traits<RandomAccessIterator>::value_type value = *result;
        *result = *first;
        AZStd::Internal::adjust_heap(first, typename AZStd::iterator_traits<RandomAccessIterator>::difference_type(0), typename AZStd::iterator_traits<RandomAccessIterator>::difference_type(last - first), value, comp);
    }

    /// @}
    //////////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////////
    // Search
    using std::equal_range;
    using std::lower_bound;
    using std::upper_bound;
    using std::search;
    using std::is_sorted;
    using std::unique;

    using std::binary_search;

    // todo search_n
    //////////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////////
    // set_difference
    using std::set_difference;
    //////////////////////////////////////////////////////////////////////////

    using std::lexicographical_compare;

    constexpr bool lexicographical_compare(const unsigned char* first1, const unsigned char* last1, const unsigned char* first2, const unsigned char* last2)
    {
        ptrdiff_t len1 = last1 - first1;
        ptrdiff_t len2 = last2 - first2;
        int res = __builtin_memcmp(first1, first2, len1 < len2 ? len1 : len2);
        return (res < 0 || (res == 0 && len1 < len2));
    }

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

    using std::erase;
    using std::erase_if;
}
