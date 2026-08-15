/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/System.h>
#include <Jolt/TypeIds.h>

#include <AzCore/std/parallel/atomic.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

#include <AssetBuilderSDK/AssetBuilderSDK.h>

namespace Jolt::Editor
{
    class SkeletonBuilder final
        : public AssetBuilderSDK::AssetBuilderCommandBus::Handler
    {
    public:
        AZ_RTTI(SkeletonBuilder, SkeletonBuilderTypeId);

        SkeletonBuilder() = default;
        ~SkeletonBuilder() override;

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
} // namespace Jolt::Editor
