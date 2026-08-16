/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Handle.h>

namespace Jolt::Internal
{
    void AcquireTransformedShapeLease(
        void* owner,
        ShapeHandle shapeHandle);

    void ReleaseTransformedShapeLease(
        void* owner,
        ShapeHandle shapeHandle);
} // namespace Jolt::Internal
