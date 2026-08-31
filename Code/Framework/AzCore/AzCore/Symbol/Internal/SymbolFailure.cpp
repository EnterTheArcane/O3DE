/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Symbol/Internal/SymbolFailure.h>

#include <AzCore/Debug/Trace.h>

#include <cstdio>
#include <cstdlib>

namespace AZ::Internal
{
    [[noreturn]] void FailSymbol(const char* reason)
    {
        std::fprintf(stderr, "AZ::Symbol fatal error: %s\n", reason);
        std::fflush(stderr);
        AZ_Assert(false, "%s", reason);
        std::abort();
    }

    [[noreturn]] void FailSymbolStorage(
        const size_t requestedValueBytes,
        const size_t usedBytes,
        const size_t limitBytes)
    {
        std::fprintf(
            stderr,
            "AZ::Symbol creation failed: valueBytes=%zu storageUsedBytes=%zu storageLimitBytes=%zu\n",
            requestedValueBytes,
            usedBytes,
            limitBytes);
        std::fflush(stderr);
        AZ_Assert(
            false,
            "AZ::Symbol creation failed: valueBytes=%zu storageUsedBytes=%zu storageLimitBytes=%zu",
            requestedValueBytes,
            usedBytes,
            limitBytes);
        std::abort();
    }
} // namespace AZ::Internal
