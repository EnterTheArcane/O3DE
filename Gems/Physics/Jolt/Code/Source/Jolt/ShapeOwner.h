/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/ColliderBus.h>
#include <Jolt/Handle.h>

#include <AzCore/std/containers/span.h>
#include <AzCore/std/containers/vector.h>

namespace Jolt
{
    class ISystem;
}

namespace Jolt::Internal
{
    struct ShapeSet final
    {
        AZStd::vector<ShapeHandle> m_ownedShapes;
        AZStd::vector<ShapeHandle> m_shapeHandles;
        AZStd::vector<MaterialHandle> m_ownedMaterials;
        ShapeHandle m_rootShapeHandle;
    };

    [[nodiscard]]
    bool CreateShapeSet(
        ISystem& system,
        WorldHandle worldHandle,
        AZStd::span<const ColliderShapeConfiguration> configurations,
        float uniformScale,
        ShapeSet& shapeSet);

    void DestroyShapeSet(
        ISystem& system,
        WorldHandle worldHandle,
        ShapeSet& shapeSet);
} // namespace Jolt::Internal
