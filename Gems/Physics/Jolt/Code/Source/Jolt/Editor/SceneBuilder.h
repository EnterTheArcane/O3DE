/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Editor/SkeletonBuilder.h>
#include <Jolt/System.h>
#include <Jolt/TypeIds.h>

#include <AzCore/Component/Component.h>
#include <AzCore/std/parallel/atomic.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

#include <AssetBuilderSDK/AssetBuilderSDK.h>

namespace Jolt::Editor
{
    class SceneBuilder final
        : public AssetBuilderSDK::AssetBuilderCommandBus::Handler
    {
    public:
        AZ_RTTI(SceneBuilder, SceneBuilderTypeId);

        SceneBuilder() = default;
        ~SceneBuilder() override;

        void Register();

        void ShutDown() override;

        void CreateJobs(
            const AssetBuilderSDK::CreateJobsRequest& request,
            AssetBuilderSDK::CreateJobsResponse& response) const;

        void ProcessJob(
            const AssetBuilderSDK::ProcessJobRequest& request,
            AssetBuilderSDK::ProcessJobResponse& response) const;

    private:
        AZStd::unique_ptr<ISystem> m_ownedSystem;
        AZStd::atomic_bool m_isShuttingDown = false;
    };

    class BuilderComponent final
        : public AZ::Component
    {
    public:
        AZ_COMPONENT(BuilderComponent, BuilderComponentTypeId);

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        void Activate() override;

        void Deactivate() override;

    private:
        SceneBuilder m_builder;
        SkeletonBuilder m_skeletonBuilder;
    };
} // namespace Jolt::Editor
