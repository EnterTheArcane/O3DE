/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Utils/SystemUtilsApple_Platform.h>
#include <AzCore/Utils/Utils.h>
#include <dlfcn.h>

namespace AZ::Platform
{
    AZ::IO::FixedMaxPath GetModulePath()
    {
        // When running as an app bundle, return the directory containing the .app bundle
        // (e.g., bin/profile/) rather than the Contents/MacOS/ directory inside the bundle.
        // The build system places gem dylibs in the same directory as the .app bundle.
        AZ::IO::FixedMaxPath bundlePath;
        AZ::IO::FixedMaxPathString& bundlePathString = bundlePath.Native();
        auto GetBundlePath = [](char* buffer, size_t size) -> size_t
        {
            auto bundlePathOutcome = AZ::SystemUtilsApple::GetPathToApplicationBundle(AZStd::span(buffer, size));
            return bundlePathOutcome ? bundlePathOutcome.GetValue().size() : 0U;
        };
        bundlePathString.resize_and_overwrite(bundlePathString.capacity(), GetBundlePath);
        if (!bundlePath.empty())
        {
            // bundlePath is e.g. /path/to/bin/profile/Editor.app
            // ParentPath gives us /path/to/bin/profile/ where gem dylibs reside
            AZ::IO::FixedMaxPath bundleParent = bundlePath.ParentPath();
            if (!bundleParent.empty())
            {
                return bundleParent;
            }
        }

        return AZ::Utils::GetExecutableDirectory();
    }

    void ConstructModuleFullFileName(AZ::IO::FixedMaxPath&)
    {
    }

    AZ::IO::FixedMaxPath CreateFrameworkModulePath(const AZ::IO::PathView& moduleName)
    {
        AZ::IO::FixedMaxPath frameworksPath;
        AZ::IO::FixedMaxPathString& frameworksPathString = frameworksPath.Native();
        auto GetBundleFrameworkPath = [](char* buffer, size_t size) -> size_t
        {
            auto frameworkPathOutcome = AZ::SystemUtilsApple::GetPathToApplicationFrameworks(AZStd::span(buffer, size));
            return frameworkPathOutcome ? frameworkPathOutcome.GetValue().size() : 0U;
        };
        frameworksPathString.resize_and_overwrite(frameworksPathString.capacity(), GetBundleFrameworkPath);
        if (!frameworksPath.empty())
        {
            frameworksPath /= moduleName;
        }

        return frameworksPath;
    }
} // namespace AZ::Platform
