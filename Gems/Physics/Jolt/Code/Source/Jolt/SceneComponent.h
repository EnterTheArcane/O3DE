/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <Jolt/SceneBus.h>
#include <Jolt/SceneComponentConfiguration.h>
#include <Jolt/TypeIds.h>

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Component/Component.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace Jolt
{
    class RuntimeImplementation;

    class JOLT_API SceneComponent final
        : public AZ::Component
        , public SceneRequestBus::Handler
        , private AZ::Data::AssetBus::Handler
    {
    public:
        AZ_COMPONENT(SceneComponent, SceneComponentTypeId);

        SceneComponent();
        explicit SceneComponent(SceneComponentConfiguration configuration);
        ~SceneComponent() override;

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        [[nodiscard]]
        AZStd::vector<BodyHandle> CopyBodies() const override;

        [[nodiscard]]
        AZStd::vector<ConstraintHandle> CopyConstraints() const override;

        [[nodiscard]]
        SceneDefinitionHandle GetDefinitionHandle() const override;

        [[nodiscard]]
        SceneInstanceHandle GetInstanceHandle() const override;

        [[nodiscard]]
        bool IsReady() const override;

    private:
        struct RuntimeResources;

        void Activate() override;

        void Deactivate() override;

        void OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset) override;

        void OnAssetReloaded(AZ::Data::Asset<AZ::Data::AssetData> asset) override;

        void OnAssetError(AZ::Data::Asset<AZ::Data::AssetData> asset) override;

        bool LoadAsset(const SceneAsset& asset);

        void ReleaseResources(bool notify);

        SceneComponentConfiguration m_configuration;
        AZStd::unique_ptr<RuntimeResources> m_resources;
        const SceneAsset* m_loadedAsset = nullptr;
        RuntimeImplementation* m_system = nullptr;
    };
} // namespace Jolt
