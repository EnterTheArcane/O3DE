/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <AzCore/Math/PackedVector2.h>
#include <AzCore/std/containers/intrusive_slist.h>

#include <cstddef>

namespace LyShine
{
    struct UCol
    {
        union {
            AZ::u32 dcolor;
            AZ::u8 bcolor[4];

            struct
            {
                AZ::u8 b, g, r, a;
            };
            struct
            {
                AZ::u8 z, y, x, w;
            };
        };
    };

    struct UiPrimitiveVertex
    {
        AZ::PackedVector2f xy;
        UCol color;
        AZ::PackedVector2f st;
        AZ::u8 texIndex;
        AZ::u8 texHasColorChannel;
        AZ::u8 texIndex2;
        AZ::u8 pad;
    };

    static_assert(sizeof(UCol) == 4);
    static_assert(alignof(UCol) == 4);
    static_assert(sizeof(UiPrimitiveVertex) == 24);
    static_assert(alignof(UiPrimitiveVertex) == 4);
    static_assert(offsetof(UiPrimitiveVertex, color) == 8);
    static_assert(offsetof(UiPrimitiveVertex, st) == 12);
    static_assert(offsetof(UiPrimitiveVertex, texIndex) == 20);

    using UiIndice = AZ::u16;

    struct UiPrimitive : public AZStd::intrusive_slist_node<UiPrimitive>
    {
        UiPrimitiveVertex* m_vertices = nullptr;
        AZ::u16* m_indices = nullptr;
        int m_numVertices = 0;
        int m_numIndices = 0;
    };
    using UiPrimitiveList = AZStd::intrusive_slist<UiPrimitive, AZStd::slist_base_hook<UiPrimitive>>;
}; // namespace LyShine
