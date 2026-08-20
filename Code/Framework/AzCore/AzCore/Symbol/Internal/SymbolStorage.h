/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Symbol/Symbol.h>

namespace AZ::Internal
{
    struct ExternalSymbolAdmissionLimits final
    {
        u32 m_processSymbolCount = 0;
        u64 m_processStorageBytes = 0;
        u64 m_entryStorageBytes = 0;
    };

    //! Interns spelling already checked by ValidateSymbolSpelling. Allocation failure terminates.
    [[nodiscard]]
    AZCORE_API Symbol InternValidatedSymbol(AZStd::string_view value);

    //! Looks up spelling without creating permanent storage. Empty is always found as the null Symbol.
    [[nodiscard]]
    AZCORE_API bool FindSymbol(Symbol& result, AZStd::string_view value);

    //! Resolves an existing Symbol or admits a bounded external-origin spelling.
    [[nodiscard]]
    AZCORE_API bool AdmitExternalSymbol(
        Symbol& result,
        AZStd::string_view value,
        u32& scopedAdmissionCount,
        u32 scopedAdmissionLimit,
        const ExternalSymbolAdmissionLimits& processLimits);
} // namespace AZ::Internal
