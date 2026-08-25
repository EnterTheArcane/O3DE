/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/Editor/SourceDependencyAnalyzer.h>

#include <Jolt/Capabilities/Extensions.h>
#include <Jolt/CustomShapeDependencyUtils.h>

#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/IO/Path/Path.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/utility/move.h>

#include <AssetBuilderSDK/AssetBuilderSDK.h>

namespace Jolt::Editor
{
    namespace
    {
        constexpr AZ::u64 SourceDependencyFingerprintVersion = 1;

        AZ::Outcome<AnalyzedSourceDependency, AZStd::string> AnalyzeSourceDependency(
            const AZStd::string_view sourcePath,
            const AZStd::string_view watchFolder)
        {
            if (watchFolder.empty())
            {
                return AZ::Failure(AZStd::string(
                    "A custom-shape source dependency cannot be resolved without an asset watch folder."));
            }

            AZ::Outcome<AZStd::string, AZStd::string> canonicalPathResult =
                CanonicalizeCustomShapeDependencyPath(sourcePath);
            if (!canonicalPathResult.IsSuccess())
            {
                return AZ::Failure(canonicalPathResult.TakeError());
            }

            AnalyzedSourceDependency dependency;
            dependency.m_relativePath = canonicalPathResult.TakeValue();
            dependency.m_absolutePath =
                (AZ::IO::Path(watchFolder) / dependency.m_relativePath).LexicallyNormal().String();
            dependency.m_exists = AZ::IO::SystemFile::Exists(dependency.m_absolutePath.c_str());
            if (dependency.m_exists)
            {
                dependency.m_contentHash = AssetBuilderSDK::GetFileHash(dependency.m_absolutePath.c_str());
                if (dependency.m_contentHash == 0)
                {
                    return AZ::Failure(AZStd::string::format(
                        "Failed to hash custom-shape source dependency '%s'.",
                        dependency.m_absolutePath.c_str()));
                }
            }

            return AZ::Success(AZStd::move(dependency));
        }

        bool DependenciesMatch(
            const AZStd::span<const CustomShapeDependency> first,
            const AZStd::span<const CustomShapeDependency> second)
        {
            if (first.size() != second.size())
            {
                return false;
            }

            for (size_t dependencyIndex = 0; dependencyIndex < first.size(); ++dependencyIndex)
            {
                if (first[dependencyIndex].m_path != second[dependencyIndex].m_path
                    || first[dependencyIndex].m_contentHash != second[dependencyIndex].m_contentHash)
                {
                    return false;
                }
            }

            return true;
        }
    } // namespace

    AZ::Outcome<SceneSourceAnalysis, AZStd::string> AnalyzeSceneSource(
        const SceneSourceData& sourceData,
        const AZStd::string_view watchFolder)
    {
        Extensions* extensions = nullptr;
        SceneSourceAnalysis analysis;
        analysis.m_customShapes.reserve(sourceData.m_shapes.size());
        for (size_t shapeIndex = 0; shapeIndex < sourceData.m_shapes.size(); ++shapeIndex)
        {
            const auto* sourceShape = AZStd::get_if<SceneSourceShapeData>(&sourceData.m_shapes[shapeIndex]);
            if (!sourceShape)
            {
                continue;
            }

            ExtensionKind providerKind = ExtensionKind::None;
            AZ::TypeId providerId = AZ::TypeId::CreateNull();
            if (const auto* customShape = AZStd::get_if<CustomShapeConfiguration>(&sourceShape->m_geometry))
            {
                providerKind = ExtensionKind::CustomShapeProvider;
                providerId = customShape->m_providerId;
            }
            else if (const auto* customConvexShape = AZStd::get_if<CustomConvexShapeConfiguration>(&sourceShape->m_geometry))
            {
                providerKind = ExtensionKind::CustomConvexShapeProvider;
                providerId = customConvexShape->m_providerId;
            }
            else
            {
                if (!sourceShape->m_sourceDependencies.empty())
                {
                    return AZ::Failure(AZStd::string::format(
                        "Scene shape %zu declares source dependencies but does not use a custom-shape provider.",
                        shapeIndex));
                }
                continue;
            }

            if (providerKind == ExtensionKind::CustomConvexShapeProvider
                && !sourceShape->m_sourceDependencies.empty())
            {
                return AZ::Failure(AZStd::string::format(
                    "Scene shape %zu declares source dependencies for a custom convex provider that cannot report dependency hashes.",
                    shapeIndex));
            }

            AnalyzedCustomShape& customShape = analysis.m_customShapes.emplace_back();
            customShape.m_shapeIndex = aznumeric_cast<AZ::u32>(shapeIndex);
            if (!extensions)
            {
                extensions = Extensions::Get();
                if (!extensions)
                {
                    return AZ::Failure(AZStd::string(
                        "Custom-shape source analysis requires an active physics system."));
                }
            }
            if (!extensions->FindExtensionInformation(providerKind, providerId, customShape.m_provider))
            {
                return AZ::Failure(AZStd::string::format(
                    "Scene shape %zu requires an unavailable custom-shape provider '%s'.",
                    shapeIndex,
                    providerId.ToString<AZStd::string>().c_str()));
            }

            customShape.m_dependencies.reserve(sourceShape->m_sourceDependencies.size());
            for (const AZStd::string& sourceDependency : sourceShape->m_sourceDependencies)
            {
                AZ::Outcome<AnalyzedSourceDependency, AZStd::string> dependencyResult =
                    AnalyzeSourceDependency(sourceDependency, watchFolder);
                if (!dependencyResult.IsSuccess())
                {
                    return AZ::Failure(dependencyResult.TakeError());
                }

                AnalyzedSourceDependency dependency = dependencyResult.TakeValue();
                customShape.m_dependencies.push_back({dependency.m_relativePath, dependency.m_contentHash});
                analysis.m_dependencies.push_back(AZStd::move(dependency));
            }

            AZStd::sort(
                customShape.m_dependencies.begin(),
                customShape.m_dependencies.end(),
                [](const CustomShapeDependency& first, const CustomShapeDependency& second)
                {
                    return first.m_path < second.m_path;
                });
            customShape.m_dependencies.erase(
                AZStd::unique(
                    customShape.m_dependencies.begin(),
                    customShape.m_dependencies.end(),
                    [](const CustomShapeDependency& first, const CustomShapeDependency& second)
                    {
                        return first.m_path == second.m_path;
                    }),
                customShape.m_dependencies.end());
        }

        AZStd::sort(
            analysis.m_dependencies.begin(),
            analysis.m_dependencies.end(),
            [](const AnalyzedSourceDependency& first, const AnalyzedSourceDependency& second)
            {
                return first.m_relativePath < second.m_relativePath;
            });
        for (size_t dependencyIndex = 1; dependencyIndex < analysis.m_dependencies.size(); ++dependencyIndex)
        {
            const AnalyzedSourceDependency& previousDependency = analysis.m_dependencies[dependencyIndex - 1];
            const AnalyzedSourceDependency& dependency = analysis.m_dependencies[dependencyIndex];
            if (dependency.m_relativePath == previousDependency.m_relativePath
                && (dependency.m_exists != previousDependency.m_exists
                    || dependency.m_contentHash != previousDependency.m_contentHash))
            {
                return AZ::Failure(AZStd::string::format(
                    "Custom-shape source dependency '%s' changed while it was being analyzed.",
                    dependency.m_relativePath.c_str()));
            }
        }
        analysis.m_dependencies.erase(
            AZStd::unique(
                analysis.m_dependencies.begin(),
                analysis.m_dependencies.end(),
                [](const AnalyzedSourceDependency& first, const AnalyzedSourceDependency& second)
                {
                    return first.m_relativePath == second.m_relativePath;
                }),
            analysis.m_dependencies.end());

        AZ::HashValue64 fingerprint = AZ::TypeHash64(SourceDependencyFingerprintVersion);
        for (const AnalyzedCustomShape& customShape : analysis.m_customShapes)
        {
            fingerprint = AZ::TypeHash64(customShape.m_provider.m_id, fingerprint);
            fingerprint = AZ::TypeHash64(customShape.m_provider.m_version, fingerprint);
            fingerprint = AZ::TypeHash64(customShape.m_provider.m_kind, fingerprint);
        }
        for (const AnalyzedSourceDependency& dependency : analysis.m_dependencies)
        {
            fingerprint = AZ::TypeHash64(
                reinterpret_cast<const AZ::u8*>(dependency.m_relativePath.data()),
                dependency.m_relativePath.size(),
                fingerprint);
            fingerprint = AZ::TypeHash64(dependency.m_contentHash, fingerprint);
            fingerprint = AZ::TypeHash64(dependency.m_exists, fingerprint);
        }
        analysis.m_fingerprint = fingerprint;
        return AZ::Success(AZStd::move(analysis));
    }

    bool ValidateSceneAssetDependencies(
        const SceneSourceAnalysis& analysis,
        const SceneAssetData& sceneAsset,
        AZStd::string& error)
    {
        for (const AnalyzedSourceDependency& dependency : analysis.m_dependencies)
        {
            if (!dependency.m_exists)
            {
                error = AZStd::string::format(
                    "Custom-shape source dependency '%s' is unavailable.",
                    dependency.m_relativePath.c_str());
                return false;
            }
        }

        for (const AnalyzedCustomShape& customShape : analysis.m_customShapes)
        {
            if (customShape.m_shapeIndex >= sceneAsset.m_shapes.size())
            {
                error = AZStd::string::format(
                    "Compiled custom shape %u is missing from the scene product.",
                    customShape.m_shapeIndex);
                return false;
            }

            const SceneAssetShape& assetShape = sceneAsset.m_shapes[customShape.m_shapeIndex];
            if (assetShape.m_providerId != customShape.m_provider.m_id
                || assetShape.m_providerVersion != customShape.m_provider.m_version)
            {
                error = AZStd::string::format(
                    "Compiled custom shape %u does not match the analyzed provider identity.",
                    customShape.m_shapeIndex);
                return false;
            }
            if (!DependenciesMatch(assetShape.m_dependencies, customShape.m_dependencies))
            {
                error = AZStd::string::format(
                    "Compiled custom shape %u reported source dependencies that do not match the authoring record.",
                    customShape.m_shapeIndex);
                return false;
            }

            if (assetShape.m_archive.m_providerId != assetShape.m_providerId
                || assetShape.m_archive.m_providerVersion != assetShape.m_providerVersion
                || !DependenciesMatch(assetShape.m_archive.m_dependencies, assetShape.m_dependencies))
            {
                error = AZStd::string::format(
                    "Compiled custom shape %u contains inconsistent portable and native dependency metadata.",
                    customShape.m_shapeIndex);
                return false;
            }
        }

        return true;
    }
} // namespace Jolt::Editor
