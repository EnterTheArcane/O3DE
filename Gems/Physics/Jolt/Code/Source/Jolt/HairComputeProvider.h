/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Hair.h>
#include <Jolt/SystemConfiguration.h>

#include <AzCore/RTTI/RTTI.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

#include <Jolt/Jolt.h>
#include <Jolt/Compute/ComputeSystem.h>

namespace AZ::RHI
{
    class FrameGraphBuilder;
} // namespace AZ::RHI

namespace JPH
{
    class Hair;
} // namespace JPH

namespace Jolt
{
    class IHairRenderBuffers
    {
    public:
        virtual ~IHairRenderBuffers() = default;

        [[nodiscard]]
        virtual bool Initialize(JPH::Hair& hair) = 0;

        [[nodiscard]]
        virtual bool BeginUpdate(JPH::Hair& hair) = 0;

        virtual void CompleteUpdate() = 0;

        [[nodiscard]]
        virtual bool Acquire(HairRenderBuffer& buffer) = 0;

        [[nodiscard]]
        virtual bool ImportHandoff(
            AZ::RHI::FrameGraphBuilder& frameGraphBuilder,
            AZ::u64 token) = 0;

        [[nodiscard]]
        virtual bool PrepareForDestruction() = 0;
    };

    class IHairComputeProvider
    {
    public:
        AZ_RTTI(IHairComputeProvider, "{34DBE303-D3BF-4062-919A-13D110AA19EB}");

        virtual ~IHairComputeProvider() = default;

        [[nodiscard]]
        virtual HairComputeBackend GetBackend() const = 0;

        [[nodiscard]]
        virtual JPH::ComputeSystemResult CreateComputeSystem() = 0;

        [[nodiscard]]
        virtual AZStd::unique_ptr<IHairRenderBuffers> CreateRenderBuffers(
            [[maybe_unused]] JPH::ComputeSystem& computeSystem,
            [[maybe_unused]] AZ::u32 vertexCount,
            [[maybe_unused]] AZ::u32 stride)
        {
            return {};
        }
    };
} // namespace Jolt
