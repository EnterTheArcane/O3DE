/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <climits>
#include <cstddef>
#include <cstring>

namespace AZStd::Internal
{
    //! Accepts only a complete procfs TracerPid line with a nonzero, valid PID.
    inline bool IsDebuggerPresentInProcStatusLine(const char* line, size_t length) noexcept
    {
        constexpr char prefix[] = "TracerPid:";
        constexpr size_t prefixLength = sizeof(prefix) - 1;
        if (length <= prefixLength || line[length - 1] != '\n' || memcmp(line, prefix, prefixLength) != 0)
        {
            return false;
        }

        size_t offset = prefixLength;
        while (offset < length && (line[offset] == ' ' || line[offset] == '\t'))
        {
            ++offset;
        }

        int pid = 0;
        const size_t digitsBegin = offset;
        while (offset < length && line[offset] >= '0' && line[offset] <= '9')
        {
            const int digit = line[offset] - '0';
            if (pid > (INT_MAX - digit) / 10)
            {
                return false;
            }
            pid = pid * 10 + digit;
            ++offset;
        }
        if (offset == digitsBegin)
        {
            return false;
        }

        while (offset < length - 1 && (line[offset] == ' ' || line[offset] == '\t' || line[offset] == '\r'))
        {
            ++offset;
        }
        return offset == length - 1 && pid != 0;
    }
}
