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
    [[nodiscard]]
    AZStd::unique_ptr<IHairRenderBuffers> CreatePlatformHairRenderBuffers(
        JPH::ComputeSystem& computeSystem,
        AZ::u32 vertexCount,
        AZ::u32 stride);
} // namespace Jolt
