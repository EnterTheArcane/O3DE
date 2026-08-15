/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Jolt.h>

namespace Jolt
{
    [[nodiscard]]
    bool LoadGpuHairShader(
        const char* name,
        JPH::Array<JPH::uint8>& data,
        JPH::String& error);
} // namespace Jolt
