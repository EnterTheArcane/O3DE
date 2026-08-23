/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <Jolt/Operation.h>
#include <Jolt/Query.h>
#include <Jolt/Scene.h>

namespace Jolt
{
    class Runtime;
    struct SceneAssetData;
    struct SceneSourceData;

    class JOLT_API Scenes
    {
    public:
        //! Returns the active capability, or nullptr if no global System is active.
        //! The pointer is non-owning and must not be acquired or used while System destruction can occur.
        [[nodiscard]]
        static Scenes* Get();

        [[nodiscard]]
        SceneDefinitionHandle CreateSceneDefinition(const SceneConfiguration& configuration);

        [[nodiscard]]
        SceneDefinitionHandle CreateSceneDefinition(const SceneAssetData& assetData);

        [[nodiscard]]
        bool BuildSceneAsset(
            const SceneSourceData& sourceData,
            SceneAssetData& assetData);

        bool DestroySceneDefinition(SceneDefinitionHandle definitionHandle);

        [[nodiscard]]
        bool IsValid(SceneDefinitionHandle definitionHandle) const;

        [[nodiscard]]
        bool GetSceneDefinitionState(
            SceneDefinitionHandle definitionHandle,
            SceneDefinitionState& state) const;

        [[nodiscard]]
        SceneInstanceHandle InstantiateScene(
            WorldHandle worldHandle,
            SceneDefinitionHandle definitionHandle);

        [[nodiscard]]
        Operation<SceneInstanceHandle> InstantiateSceneAsync(
            WorldHandle worldHandle,
            SceneDefinitionHandle definitionHandle);

        bool DestroySceneInstance(
            WorldHandle worldHandle,
            SceneInstanceHandle instanceHandle);

        [[nodiscard]]
        bool IsValid(
            WorldHandle worldHandle,
            SceneInstanceHandle instanceHandle) const;

        [[nodiscard]]
        bool GetSceneInstanceState(
            WorldHandle worldHandle,
            SceneInstanceHandle instanceHandle,
            SceneInstanceState& state) const;

        [[nodiscard]]
        QueryResult GetSceneBodies(
            WorldHandle worldHandle,
            SceneInstanceHandle instanceHandle,
            AZStd::span<BodyHandle> bodyHandles) const;

        [[nodiscard]]
        QueryResult GetSceneConstraints(
            WorldHandle worldHandle,
            SceneInstanceHandle instanceHandle,
            AZStd::span<ConstraintHandle> constraintHandles) const;

    private:
        friend class Runtime;

        Scenes() = default;
        ~Scenes() = default;
    };
} // namespace Jolt
