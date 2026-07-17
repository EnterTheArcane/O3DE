/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/base.h>

namespace AZ::RHI
{
    // [GFX TODO] ATOM-1668 This enum is a temporary copy of the RPI::ShaderStageType.
    // We need to decide if virtual stages are a good design for the RHI and expose one
    // unique shader stage enum that the RHI and RPI can use.
    enum ShaderHardwareStage : uint32_t
    {
        Invalid = static_cast<uint32_t>(-1),
        Vertex = 0,
        Geometry,
        Fragment,
        Compute,
        RayTracing,
    };
}
