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
    //! Read-only snapshot of requested storage owned by the process-local SymbolTable.
    struct AZCORE_API SymbolStorageStats final
    {
        size_t m_usedByteCount = 0;
        size_t m_limitByteCount = 0;
        size_t m_arenaByteCount = 0;
        size_t m_tableByteCount = 0;
        size_t m_entryCount = 0;
    };

    [[nodiscard]]
    AZCORE_API SymbolStorageStats GetSymbolStorageStats();

    //! Interns a value already checked by ValidateSymbolValue. Allocation or budget failure terminates.
    [[nodiscard]]
    AZCORE_API Symbol InternValidatedSymbol(AZStd::string_view value);

    //! Validates and interns a value, preserving result on failure.
    [[nodiscard]]
    AZCORE_API bool TryCreateSymbol(Symbol& result, AZStd::string_view value);

    //! Looks up a value without creating permanent storage. Empty is always found as the null Symbol.
    [[nodiscard]]
    AZCORE_API bool FindSymbol(Symbol& result, AZStd::string_view value);
} // namespace AZ::Internal
