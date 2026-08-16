/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/SceneAssetHandler.h>

#include <Jolt/AssetProduct.h>
#include <Jolt/SceneAsset.h>
#include <Jolt/TypeIds.h>

#include <AzCore/std/utility/move.h>

namespace Jolt
{
    SceneAssetHandler::SceneAssetHandler()
    {
        AZ_Assert(AZ::Data::AssetManager::IsReady(), "Scene asset handler requires the asset manager.");
        AZ::Data::AssetManager::Instance().RegisterHandler(this, SceneAssetTypeId);
        AZ::AssetTypeInfoBus::Handler::BusConnect(SceneAssetTypeId);
    }

    SceneAssetHandler::~SceneAssetHandler()
    {
        AZ::AssetTypeInfoBus::Handler::BusDisconnect();
        if (AZ::Data::AssetManager::IsReady())
        {
            AZ::Data::AssetManager::Instance().UnregisterHandler(this);
        }
    }

    AZ::Data::AssetPtr SceneAssetHandler::CreateAsset(
        [[maybe_unused]] const AZ::Data::AssetId& assetId,
        const AZ::Data::AssetType& assetType)
    {
        if (assetType != SceneAssetTypeId)
        {
            return nullptr;
        }
        return aznew SceneAsset();
    }

    AZ::Data::AssetHandler::LoadResult SceneAssetHandler::LoadAssetData(
        const AZ::Data::Asset<AZ::Data::AssetData>& asset,
        AZStd::shared_ptr<AZ::Data::AssetDataStream> stream,
        [[maybe_unused]] const AZ::Data::AssetFilterCB& assetLoadFilter)
    {
        SceneAsset* sceneAsset = asset.GetAs<SceneAsset>();
        if (!sceneAsset || !stream)
        {
            return LoadResult::Error;
        }

        SceneAsset loadedAsset;
        if (!LoadAssetProduct(
                *stream,
                &loadedAsset,
                SceneAssetTypeId))
        {
            return LoadResult::Error;
        }

        sceneAsset->m_data = AZStd::move(loadedAsset.m_data);
        return LoadResult::LoadComplete;
    }

    void SceneAssetHandler::DestroyAsset(
        AZ::Data::AssetPtr asset)
    {
        delete asset;
    }

    void SceneAssetHandler::GetHandledAssetTypes(
        AZStd::vector<AZ::Data::AssetType>& assetTypes)
    {
        assetTypes.push_back(SceneAssetTypeId);
    }

    AZ::Data::AssetType SceneAssetHandler::GetAssetType() const
    {
        return SceneAssetTypeId;
    }

    void SceneAssetHandler::GetAssetTypeExtensions(
        AZStd::vector<AZStd::string>& extensions)
    {
        extensions.emplace_back("jolt");
    }

    const char* SceneAssetHandler::GetAssetTypeDisplayName() const
    {
        return "Jolt Scene";
    }

    const char* SceneAssetHandler::GetBrowserIcon() const
    {
        return "Icons/Components/Physics.svg";
    }

    const char* SceneAssetHandler::GetGroup() const
    {
        return "Physics";
    }

    AZ::Uuid SceneAssetHandler::GetComponentTypeId() const
    {
        return EditorSceneComponentTypeId;
    }

    bool SceneAssetHandler::CanCreateComponent(
        [[maybe_unused]] const AZ::Data::AssetId& assetId) const
    {
        return true;
    }
} // namespace Jolt
