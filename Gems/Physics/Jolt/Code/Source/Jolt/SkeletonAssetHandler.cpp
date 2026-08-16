/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/SkeletonAssetHandler.h>

#include <Jolt/AssetProduct.h>
#include <Jolt/SkeletonAsset.h>

#include <AzCore/std/utility/move.h>

namespace Jolt
{
    SkeletonAssetHandler::SkeletonAssetHandler()
    {
        AZ_Assert(AZ::Data::AssetManager::IsReady(), "Skeleton asset handler requires the asset manager.");
        AZ::Data::AssetManager::Instance().RegisterHandler(this, SkeletonAssetTypeId);
        AZ::AssetTypeInfoBus::Handler::BusConnect(SkeletonAssetTypeId);
    }

    SkeletonAssetHandler::~SkeletonAssetHandler()
    {
        AZ::AssetTypeInfoBus::Handler::BusDisconnect();
        if (AZ::Data::AssetManager::IsReady())
        {
            AZ::Data::AssetManager::Instance().UnregisterHandler(this);
        }
    }

    AZ::Data::AssetPtr SkeletonAssetHandler::CreateAsset(
        [[maybe_unused]] const AZ::Data::AssetId& assetId,
        const AZ::Data::AssetType& assetType)
    {
        if (assetType != SkeletonAssetTypeId)
        {
            return nullptr;
        }

        return aznew SkeletonAsset();
    }

    AZ::Data::AssetHandler::LoadResult SkeletonAssetHandler::LoadAssetData(
        const AZ::Data::Asset<AZ::Data::AssetData>& asset,
        AZStd::shared_ptr<AZ::Data::AssetDataStream> stream,
        [[maybe_unused]] const AZ::Data::AssetFilterCB& assetLoadFilter)
    {
        SkeletonAsset* skeletonAsset = asset.GetAs<SkeletonAsset>();
        if (!skeletonAsset || !stream)
        {
            return LoadResult::Error;
        }

        SkeletonAsset loadedAsset;
        if (!LoadAssetProduct(
                *stream,
                &loadedAsset,
                SkeletonAssetTypeId))
        {
            return LoadResult::Error;
        }

        skeletonAsset->m_data = AZStd::move(loadedAsset.m_data);
        return LoadResult::LoadComplete;
    }

    void SkeletonAssetHandler::DestroyAsset(
        AZ::Data::AssetPtr asset)
    {
        delete asset;
    }

    void SkeletonAssetHandler::GetHandledAssetTypes(
        AZStd::vector<AZ::Data::AssetType>& assetTypes)
    {
        assetTypes.push_back(SkeletonAssetTypeId);
    }

    AZ::Data::AssetType SkeletonAssetHandler::GetAssetType() const
    {
        return SkeletonAssetTypeId;
    }

    void SkeletonAssetHandler::GetAssetTypeExtensions(
        AZStd::vector<AZStd::string>& extensions)
    {
        extensions.emplace_back("jolt");
    }

    const char* SkeletonAssetHandler::GetAssetTypeDisplayName() const
    {
        return "Jolt Skeleton";
    }

    const char* SkeletonAssetHandler::GetBrowserIcon() const
    {
        return "Icons/Components/Physics.svg";
    }

    const char* SkeletonAssetHandler::GetGroup() const
    {
        return "Physics";
    }

    AZ::Uuid SkeletonAssetHandler::GetComponentTypeId() const
    {
        return EditorSkeletonComponentTypeId;
    }

    bool SkeletonAssetHandler::CanCreateComponent(
        [[maybe_unused]] const AZ::Data::AssetId& assetId) const
    {
        return true;
    }
} // namespace Jolt
