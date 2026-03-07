/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/IO/SystemFile.h>
#include <AzCore/Utils/SystemUtilsApple_Platform.h>
#include <AzCore/Utils/Utils.h>
#include <dlfcn.h>

namespace AZ::Platform
{
    AZ::IO::FixedMaxPath GetModulePath()
    {
        return AZ::Utils::GetExecutableDirectory();
    }

    void ConstructModuleFullFileName(AZ::IO::FixedMaxPath&)
    {
    }

    bool FindPlatformModule(const AZ::IO::PathView& moduleName, AZ::IO::FixedMaxPath& outPath)
    {
        // Check the bundle's Frameworks directory
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
            outPath = frameworksPath / moduleName;
            if (AZ::IO::SystemFile::Exists(outPath.c_str()))
            {
                return true;
            }
        }

        // Check the directory containing the .app bundle, where gem dylibs are placed
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
            AZ::IO::FixedMaxPath bundleParent = bundlePath.ParentPath();
            if (!bundleParent.empty())
            {
                outPath = bundleParent / moduleName;
                if (AZ::IO::SystemFile::Exists(outPath.c_str()))
                {
                    return true;
                }
            }
        }

        return false;
    }
} // namespace AZ::Platform
