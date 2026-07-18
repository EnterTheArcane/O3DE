/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once
#include <AzCore/std/typetraits/is_constructible.h>
#include <AzCore/std/utility/pair_fwd.h>

namespace AZStd
{
    template<class T, class Allocator>
    class vector;

    template<class T, class Allocator>
    class list;
    template<class T, class Allocator>
    class forward_list;

    template<class Key, class MappedType, class Compare, class Allocator>
    class map;
    template<class Key, class MappedType, class Compare, class Allocator>
    class multimap;

    template<class Key, class MappedType, class Hasher, class EqualKey, class Allocator>
    class unordered_map;
    template<class Key, class MappedType, class Hasher, class EqualKey, class Allocator>
    class unordered_multimap;

    template <class Key, class Compare, class Allocator>
    class set;
    template <class Key, class Compare, class Allocator>
    class multiset;

    template<class Key, class Hasher, class EqualKey, class Allocator>
    class unordered_set;
    template<class Key, class Hasher, class EqualKey, class Allocator>
    class unordered_multiset;

    namespace Internal
    {
        // C++ Container types always defines a copy constructor even when it's elements aren't copyable.
        // Account for that by inspecting the contained types for known AZStd containers.
        template<typename T>
        inline constexpr bool template_is_copy_constructible_impl = is_copy_constructible_v<T>;

        template<class T, class Allocator>
        inline constexpr bool template_is_copy_constructible_impl<vector<T, Allocator>> =
            is_copy_constructible_v<T> && is_copy_constructible_v<Allocator>;

        template<class T, class Allocator>
        inline constexpr bool template_is_copy_constructible_impl<list<T, Allocator>> =
            is_copy_constructible_v<T> && is_copy_constructible_v<Allocator>;

        template<class T, class Allocator>
        inline constexpr bool template_is_copy_constructible_impl<forward_list<T, Allocator>> =
            is_copy_constructible_v<T> && is_copy_constructible_v<Allocator>;

        template<class Key, class MappedType, class Compare, class Allocator>
        inline constexpr bool template_is_copy_constructible_impl<map<Key, MappedType, Compare, Allocator>> =
            is_copy_constructible_v<Key> && is_copy_constructible_v<MappedType>
            && is_copy_constructible_v<Compare> && is_copy_constructible_v<Allocator>;

        template<class Key, class MappedType, class Compare, class Allocator>
        inline constexpr bool template_is_copy_constructible_impl<multimap<Key, MappedType, Compare, Allocator>> =
            is_copy_constructible_v<Key> && is_copy_constructible_v<MappedType>
            && is_copy_constructible_v<Compare> && is_copy_constructible_v<Allocator>;

        template<class Key, class MappedType, class Hasher, class EqualKey, class Allocator>
        inline constexpr bool template_is_copy_constructible_impl<unordered_map<Key, MappedType, Hasher, EqualKey, Allocator>> =
            is_copy_constructible_v<Key> && is_copy_constructible_v<MappedType>
            && is_copy_constructible_v<Hasher> && is_copy_constructible_v<EqualKey> && is_copy_constructible_v<Allocator>;

        template<class Key, class MappedType, class Hasher, class EqualKey, class Allocator>
        inline constexpr bool template_is_copy_constructible_impl<unordered_multimap<Key, MappedType, Hasher, EqualKey, Allocator>> =
            is_copy_constructible_v<Key> && is_copy_constructible_v<MappedType>
            && is_copy_constructible_v<Hasher> && is_copy_constructible_v<EqualKey> && is_copy_constructible_v<Allocator>;

        template<class Key, class Compare, class Allocator>
        inline constexpr bool template_is_copy_constructible_impl<set<Key, Compare, Allocator>> =
            is_copy_constructible_v<Key> && is_copy_constructible_v<Compare> && is_copy_constructible_v<Allocator>;

        template<class Key, class Compare, class Allocator>
        inline constexpr bool template_is_copy_constructible_impl<multiset<Key, Compare, Allocator>> =
            is_copy_constructible_v<Key> && is_copy_constructible_v<Compare> && is_copy_constructible_v<Allocator>;

        template<class Key, class Hasher, class EqualKey, class Allocator>
        inline constexpr bool template_is_copy_constructible_impl<unordered_set<Key, Hasher, EqualKey, Allocator>> =
            is_copy_constructible_v<Key> && is_copy_constructible_v<Hasher>
            && is_copy_constructible_v<EqualKey> && is_copy_constructible_v<Allocator>;

        template<class Key, class Hasher, class EqualKey, class Allocator>
        inline constexpr bool template_is_copy_constructible_impl<unordered_multiset<Key, Hasher, EqualKey, Allocator>> =
            is_copy_constructible_v<Key> && is_copy_constructible_v<Hasher>
            && is_copy_constructible_v<EqualKey> && is_copy_constructible_v<Allocator>;

        template<class T1, class T2>
        inline constexpr bool template_is_copy_constructible_impl<pair<T1, T2>> =
            is_copy_constructible_v<T1> && is_copy_constructible_v<T2>;

        template<typename T>
        concept template_is_copy_constructible = template_is_copy_constructible_impl<T>;
    }
}
