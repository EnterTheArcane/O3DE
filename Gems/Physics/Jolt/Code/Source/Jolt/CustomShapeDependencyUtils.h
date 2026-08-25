/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/CustomShape.h>

#include <AzCore/IO/Path/Path.h>
#include <AzCore/Outcome/Outcome.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/string/string_view.h>
#include <AzCore/std/utility/move.h>

namespace Jolt
{
    [[nodiscard]]
    inline AZ::Outcome<AZStd::string, AZStd::string> CanonicalizeCustomShapeDependencyPath(
        const AZStd::string_view sourcePath)
    {
        const AZ::IO::Path authoredPath(sourcePath);
        if (authoredPath.empty()
            || authoredPath.IsAbsolute()
            || authoredPath.HasRootName()
            || authoredPath.HasRootDirectory())
        {
            return AZ::Failure(AZStd::string::format(
                "Custom-shape source dependency '%.*s' must be a non-empty path relative to the asset watch folder.",
                AZ_STRING_ARG(sourcePath)));
        }

        const AZ::IO::Path normalizedPath = authoredPath.LexicallyNormal();
        for (const AZ::IO::PathView component : normalizedPath)
        {
            if (component == "..")
            {
                return AZ::Failure(AZStd::string::format(
                    "Custom-shape source dependency '%.*s' cannot leave the asset watch folder.",
                    AZ_STRING_ARG(sourcePath)));
            }
        }

        AZStd::string canonicalPath = normalizedPath.StringAsPosix();
        if (canonicalPath.empty() || canonicalPath == ".")
        {
            return AZ::Failure(AZStd::string::format(
                "Custom-shape source dependency '%.*s' does not identify a file.",
                AZ_STRING_ARG(sourcePath)));
        }

        return AZ::Success(AZStd::move(canonicalPath));
    }

    [[nodiscard]]
    inline bool CanonicalizeCustomShapeDependencies(
        AZStd::vector<CustomShapeDependency>& dependencies,
        AZStd::string& error)
    {
        for (CustomShapeDependency& dependency : dependencies)
        {
            if (dependency.m_contentHash == 0)
            {
                error = "Custom-shape dependencies require a nonzero content hash.";
                return false;
            }

            AZ::Outcome<AZStd::string, AZStd::string> canonicalPathResult =
                CanonicalizeCustomShapeDependencyPath(dependency.m_path);
            if (!canonicalPathResult.IsSuccess())
            {
                error = canonicalPathResult.TakeError();
                return false;
            }
            dependency.m_path = canonicalPathResult.TakeValue();
        }

        AZStd::sort(
            dependencies.begin(),
            dependencies.end(),
            [](const CustomShapeDependency& first, const CustomShapeDependency& second)
            {
                return first.m_path < second.m_path;
            });
        for (size_t dependencyIndex = 1; dependencyIndex < dependencies.size(); ++dependencyIndex)
        {
            const CustomShapeDependency& previousDependency = dependencies[dependencyIndex - 1];
            const CustomShapeDependency& dependency = dependencies[dependencyIndex];
            if (dependency.m_path == previousDependency.m_path
                && dependency.m_contentHash != previousDependency.m_contentHash)
            {
                error = AZStd::string::format(
                    "Custom-shape dependency '%s' has conflicting content hashes.",
                    dependency.m_path.c_str());
                return false;
            }
        }
        dependencies.erase(
            AZStd::unique(
                dependencies.begin(),
                dependencies.end(),
                [](const CustomShapeDependency& first, const CustomShapeDependency& second)
                {
                    return first.m_path == second.m_path;
                }),
            dependencies.end());
        return true;
    }
} // namespace Jolt
