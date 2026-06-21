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
        return AZ::Utils::GetExecutableDirectory();
    }

    void ConstructModuleFullFileName(AZ::IO::FixedMaxPath&)
    {
    }

    AZ::IO::FixedMaxPath CreateFrameworkModulePath(const AZ::IO::PathView& moduleName)
    {
        {
            AZ::IO::FixedMaxPath frameworksPath;
            auto GetBundleFrameworkPath = [](char* buffer, size_t size) -> size_t
            {
                auto frameworkPathOutcome = AZ::SystemUtilsApple::GetPathToApplicationFrameworks(AZStd::span(buffer, size));
                return frameworkPathOutcome ? frameworkPathOutcome.GetValue().size() : 0U;
            };
            frameworksPath.Native().resize_and_overwrite(frameworksPath.Native().capacity(), GetBundleFrameworkPath);
            if (!frameworksPath.empty())
            {
                AZ::IO::FixedMaxPath outPath = frameworksPath / moduleName;
                if (AZ::IO::SystemFile::Exists(outPath.c_str()))
                {
                    return outPath;
                }
            }
        }

        {
            AZ::IO::FixedMaxPath bundlePath;
            auto GetBundlePath = [](char* buffer, size_t size) -> size_t
            {
                if (auto bundlePathOutcome = AZ::SystemUtilsApple::GetPathToApplicationBundle(AZStd::span(buffer, size)))
                {
                    return bundlePathOutcome.GetValue().size();
                }
                return 0U;
            };
            bundlePath.Native().resize_and_overwrite(bundlePath.Native().capacity(), GetBundlePath);
            if (!bundlePath.empty())
            {
                if (AZ::IO::FixedMaxPath bundleParent = bundlePath.ParentPath(); !bundleParent.empty())
                {
                    AZ::IO::FixedMaxPath outPath = bundleParent / moduleName;
                    if (AZ::IO::SystemFile::Exists(outPath.c_str()))
                    {
                        return outPath;
                    }
                }
            }
        }

        return {};
    }
} // namespace AZ::Platform
