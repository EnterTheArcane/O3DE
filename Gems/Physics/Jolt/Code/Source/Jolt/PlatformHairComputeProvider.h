/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/HairComputeProvider.h>

namespace Jolt
{
    class PlatformHairComputeProvider final
        : public IHairComputeProvider
    {
    public:
        AZ_RTTI(PlatformHairComputeProvider, "{3FA41F6F-CEB6-4A0B-A233-6B8F8839BE74}", IHairComputeProvider);

        [[nodiscard]]
        HairComputeBackend GetBackend() const override;

        [[nodiscard]]
        JPH::ComputeSystemResult CreateComputeSystem() override;

        [[nodiscard]]
        AZStd::unique_ptr<IHairRenderBuffers> CreateRenderBuffers(
            JPH::ComputeSystem& computeSystem,
            AZ::u32 vertexCount,
            AZ::u32 stride) override;
    };
} // namespace Jolt
