/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <AzCore/Asset/AssetManager.h>
#include <AzCore/Asset/AssetTypeInfoBus.h>
#include <AzCore/Memory/SystemAllocator.h>

namespace Jolt
{
    class JOLT_API SceneAssetHandler final
        : public AZ::Data::AssetHandler
        , public AZ::AssetTypeInfoBus::Handler
    {
    public:
        AZ_CLASS_ALLOCATOR(SceneAssetHandler, AZ::SystemAllocator);

        SceneAssetHandler();
        ~SceneAssetHandler() override;

        AZ_DISABLE_COPY_MOVE(SceneAssetHandler);

        [[nodiscard]]
        AZ::Data::AssetPtr CreateAsset(
            const AZ::Data::AssetId& assetId,
            const AZ::Data::AssetType& assetType) override;

        [[nodiscard]]
        LoadResult LoadAssetData(
            const AZ::Data::Asset<AZ::Data::AssetData>& asset,
            AZStd::shared_ptr<AZ::Data::AssetDataStream> stream,
            const AZ::Data::AssetFilterCB& assetLoadFilter) override;

        void DestroyAsset(AZ::Data::AssetPtr asset) override;

        void GetHandledAssetTypes(AZStd::vector<AZ::Data::AssetType>& assetTypes) override;

        [[nodiscard]]
        AZ::Data::AssetType GetAssetType() const override;

        void GetAssetTypeExtensions(AZStd::vector<AZStd::string>& extensions) override;

        [[nodiscard]]
        const char* GetAssetTypeDisplayName() const override;

        [[nodiscard]]
        const char* GetBrowserIcon() const override;

        [[nodiscard]]
        const char* GetGroup() const override;

        [[nodiscard]]
        AZ::Uuid GetComponentTypeId() const override;

        [[nodiscard]]
        bool CanCreateComponent(const AZ::Data::AssetId& assetId) const override;
    };
} // namespace Jolt
