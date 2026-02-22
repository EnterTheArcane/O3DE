/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/std/typetraits/integral_constant.h>
#include <AzCore/std/typetraits/decay.h>

#include <functional>
#include <type_traits>

namespace AZStd
{
    using std::reference_wrapper;
    using std::ref;
    using std::cref;

    using std::unwrap_reference;
    using std::unwrap_reference_t;

    using std::unwrap_ref_decay;
    using std::unwrap_ref_decay_t;

    template<typename T>
    struct is_reference_wrapper : public false_type
    {
    };

#define AUX_REFERENCE_WRAPPER_METAFUNCTIONS_DEF(X) template<typename T> struct is_reference_wrapper<X> : public AZStd::true_type {};
    AUX_REFERENCE_WRAPPER_METAFUNCTIONS_DEF(reference_wrapper<T>)
    AUX_REFERENCE_WRAPPER_METAFUNCTIONS_DEF(reference_wrapper<T> const)
    AUX_REFERENCE_WRAPPER_METAFUNCTIONS_DEF(reference_wrapper<T> volatile)
    AUX_REFERENCE_WRAPPER_METAFUNCTIONS_DEF(reference_wrapper<T> const volatile)
#undef AUX_REFERENCE_WRAPPER_METAFUNCTIONS_DEF
}
