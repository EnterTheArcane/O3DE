/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Extension.h>
#include <Jolt/SceneAsset.h>

#include <AzCore/Outcome/Outcome.h>
#include <AzCore/Utils/TypeHash.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/string/string_view.h>

namespace Jolt::Editor
{
    struct AnalyzedSourceDependency final
    {
        AZStd::string m_relativePath;
        AZStd::string m_absolutePath;
        AZ::u64 m_contentHash = 0;
        bool m_exists = false;
    };

    struct AnalyzedCustomShape final
    {
        AZStd::vector<CustomShapeDependency> m_dependencies;
        ExtensionInformation m_provider;
        AZ::u32 m_shapeIndex = 0;
    };

    struct SceneSourceAnalysis final
    {
        AZStd::vector<AnalyzedSourceDependency> m_dependencies;
        AZStd::vector<AnalyzedCustomShape> m_customShapes;
        AZ::HashValue64 m_fingerprint{0};
    };

    [[nodiscard]]
    AZ::Outcome<SceneSourceAnalysis, AZStd::string> AnalyzeSceneSource(
        const SceneSourceData& sourceData,
        AZStd::string_view watchFolder);

    [[nodiscard]]
    bool ValidateSceneAssetDependencies(
        const SceneSourceAnalysis& analysis,
        const SceneAssetData& sceneAsset,
        AZStd::string& error);
} // namespace Jolt::Editor
