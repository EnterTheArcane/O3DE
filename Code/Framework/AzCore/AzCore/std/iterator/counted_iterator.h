/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <AzCore/std/base.h>
#include <AzCore/std/iterator/iterator_primitives.h>

#include <iterator>

namespace AZStd
{
    using std::counted_iterator;

    namespace Internal
    {
        template<class I, class = void>
        constexpr bool counted_iterator_trait_requirements = false;

        template<class I>
        constexpr bool counted_iterator_trait_requirements<I, enable_if_t<
            input_iterator<I>
            && same_as<ITER_TRAITS<I>, iterator_traits<I>>
            && Internal::sfinae_trigger_v<typename I::_is_primary_template>>> = true;
    }

    template<class I>
    struct iterator_traits<counted_iterator<I>, enable_if_t<Internal::counted_iterator_trait_requirements<I>>>
        : iterator_traits<I>
    {
        using pointer = conditional_t<contiguous_iterator<I>, add_pointer_t<iter_reference_t<I>>, void>;
    };
}
