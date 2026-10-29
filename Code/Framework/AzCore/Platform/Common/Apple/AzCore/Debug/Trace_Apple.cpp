/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Debug/Trace.h>
#include <AzCore/std/string/string_view.h>

#include <unistd.h>

namespace AZ
{
    namespace Debug
    {
        namespace Platform
        {
#if defined(AZ_ENABLE_DEBUG_TOOLS)
            bool AttachDebugger()
            {
                // Not supported yet
                AZ_Assert(false, "AttachDebugger() is not supported for Mac platform yet");
                return false;
            }

            void HandleExceptions(bool)
            {}

#endif // AZ_ENABLE_DEBUG_TOOLS

            void Terminate(int exitCode)
            {
                _exit(exitCode);
            }

            void OutputToDebugger(AZStd::string_view, AZStd::string_view)
            {
                // std::cout << title << ": " << message;
            }
        }
    }
}
