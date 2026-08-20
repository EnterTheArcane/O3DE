/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <Jolt/SkeletonComponentBus.h>
#include <Jolt/SkeletonComponentConfiguration.h>
#include <Jolt/TypeIds.h>

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Component/Component.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace Jolt
{
    class RuntimeImplementation;

    class JOLT_API SkeletonComponent final
        : public AZ::Component
        , public SkeletonComponentRequestBus::Handler
        , private AZ::Data::AssetBus::Handler
    {
    public:
        AZ_COMPONENT(SkeletonComponent, SkeletonComponentTypeId);

        SkeletonComponent();
        explicit SkeletonComponent(SkeletonComponentConfiguration configuration);
        ~SkeletonComponent() override;

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        [[nodiscard]]
        bool IsReady() const override;

        [[nodiscard]]
        SkeletonDefinitionHandle GetSkeletonHandle() const override;

        [[nodiscard]]
        SkeletalAnimationHandle FindAnimation(AZ::Name name) const override;

        [[nodiscard]]
        BufferResult GetAnimationNames(AZStd::span<AZ::Name> names) const override;

        [[nodiscard]]
        AZStd::vector<AZ::Name> CopyAnimationNames() const override;

    private:
        struct RuntimeResources;

        void Activate() override;

        void Deactivate() override;

        void OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset) override;

        void OnAssetReloaded(AZ::Data::Asset<AZ::Data::AssetData> asset) override;

        void OnAssetError(AZ::Data::Asset<AZ::Data::AssetData> asset) override;

        bool LoadAsset(const SkeletonAsset& asset);

        void ReleaseResources(bool notify);

        SkeletonComponentConfiguration m_configuration;
        AZStd::unique_ptr<RuntimeResources> m_resources;
        const SkeletonAsset* m_loadedAsset = nullptr;
        RuntimeImplementation* m_system = nullptr;
    };
} // namespace Jolt
