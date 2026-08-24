/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/base.h>

namespace AZ::Internal
{
    struct SymbolGroupMasks final
    {
        u16 m_matches;
        u16 m_empty;
    };

    class AZCORE_API SymbolGroup final
    {
    public:
        static constexpr size_t Width = 16;

        [[nodiscard]]
        static SymbolGroupMasks Match(const u8* controls, u8 fingerprint);

        [[nodiscard]]
        static SymbolGroupMasks MatchScalar(const u8* controls, u8 fingerprint);
    };
} // namespace AZ::Internal
