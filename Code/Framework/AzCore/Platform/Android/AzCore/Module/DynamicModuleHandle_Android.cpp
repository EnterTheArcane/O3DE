/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/std/string/osstring.h>
#include <AzCore/Utils/Utils.h>
#include <dlfcn.h>

namespace AZ::Platform
{
    AZ::IO::FixedMaxPath GetModulePath()
    {
        return {};
    }

    void ConstructModuleFullFileName(AZ::IO::FixedMaxPath&)
    {
    }

    bool FindPlatformModule(const AZ::IO::PathView&, AZ::IO::FixedMaxPath&)
    {
        return false;
    }
} // namespace AZ::Platform
