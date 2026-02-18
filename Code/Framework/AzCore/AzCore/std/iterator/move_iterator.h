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

#include <iterator>

namespace AZStd::Internal
{
    template<class I, class = void>
    struct move_iterator_iter_category {};

    template<class I>
    struct move_iterator_iter_category<I, void_t<typename ITER_TRAITS<I>::iterator_category>>
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
    using std::move_iterator;
    using std::make_move_iterator;
}
