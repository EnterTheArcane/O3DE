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
    [[noreturn]] AZCORE_API void FailSymbol(const char* reason);

    [[noreturn]] AZCORE_API void FailSymbolStorage(
        size_t requestedValueBytes,
        size_t usedBytes,
        size_t limitBytes);
} // namespace AZ::Internal
