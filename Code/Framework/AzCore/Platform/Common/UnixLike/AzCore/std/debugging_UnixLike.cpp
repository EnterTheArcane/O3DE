/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/std/debugging.h>

#if !defined(__cpp_lib_debugging) || __cpp_lib_debugging < 202311L
#include <UnixLike/AzCore/std/internal/debugging_procfs.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

namespace AZStd
{
    bool is_debugger_present() noexcept
    {
        int statusFile;
        do
        {
            statusFile = open("/proc/self/status", O_RDONLY | O_CLOEXEC);
        } while (statusFile < 0 && errno == EINTR);
        if (statusFile < 0)
        {
            return false;
        }

        char buffer[1024];
        // A PID fits in an int. Overlong lines are skipped without truncating them into valid fields.
        char line[128];
        size_t lineLength = 0;
        bool fieldFound = false;
        bool debuggerPresent = false;
        while (!fieldFound)
        {
            const ssize_t bytesRead = read(statusFile, buffer, sizeof(buffer));
            if (bytesRead < 0 && errno == EINTR)
            {
                continue;
            }
            if (bytesRead <= 0)
            {
                break;
            }

            for (ssize_t offset = 0; offset < bytesRead; ++offset)
            {
                if (lineLength < sizeof(line))
                {
                    line[lineLength++] = buffer[offset];
                }
                if (buffer[offset] == '\n')
                {
                    constexpr char prefix[] = "TracerPid:";
                    if (lineLength >= sizeof(prefix) - 1 && memcmp(line, prefix, sizeof(prefix) - 1) == 0)
                    {
                        fieldFound = true;
                        debuggerPresent = Internal::IsDebuggerPresentInProcStatusLine(line, lineLength);
                        break;
                    }
                    lineLength = 0;
                }
            }
        }
        close(statusFile);
        return debuggerPresent;
    }
}
#endif
