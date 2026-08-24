/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Symbol/Internal/SymbolFailure.h>

#include <AzCore/Debug/Trace.h>

#include <cstdlib>

namespace AZ::Internal
{
    [[noreturn]] void FailSymbol(const char* reason)
    {
        AZ_Assert(false, "%s", reason);
        std::abort();
    }
} // namespace AZ::Internal
