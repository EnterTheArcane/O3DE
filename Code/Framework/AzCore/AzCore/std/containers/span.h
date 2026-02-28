/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <AzCore/std/containers/span_fwd.h>

#include <AzCore/std/containers/array.h>
#include <AzCore/std/limits.h>
#include <AzCore/std/ranges/ranges.h>
#include <AzCore/std/typetraits/type_identity.h>

namespace AZStd
{
    using std::span;
    using std::as_bytes;
    using std::as_writable_bytes;

} // namespace AZStd

namespace AZStd::ranges
{
    template<class ElementType, size_t Extent>
    inline constexpr bool enable_view<span<ElementType, Extent>> = true;
    template<class ElementType, size_t Extent>
    inline constexpr bool enable_borrowed_range<span<ElementType, Extent>> = true;
}
