/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/base.h>

#include <Jolt/Jolt.h>
#include <Jolt/Compute/ComputeSystem.h>

namespace AZ::RHI
{
    class DeviceBuffer;
} // namespace AZ::RHI

namespace Jolt::Platform::Vulkan
{
    [[nodiscard]]
    JPH::ComputeSystemResult CreateHairComputeSystem();

    [[nodiscard]]
    JPH::ComputeBufferResult CreateHairRenderBuffer(
        JPH::ComputeSystem& computeSystem,
        AZ::RHI::DeviceBuffer& buffer,
        AZ::u32 vertexCount,
        AZ::u32 stride);

    void SynchronizeHairRenderBufferAfterUpdate(AZ::RHI::DeviceBuffer& buffer);
} // namespace Jolt::Platform::Vulkan
