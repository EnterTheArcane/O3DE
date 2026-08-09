/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Box3D/ShapeConfiguration.h>

#include <AzCore/Math/Aabb.h>
#include <AzCore/Math/Matrix3x4.h>
#include <AzCore/Math/Vector3.h>

namespace AzFramework
{
    class DebugDisplayRequests;
}

namespace Box3D::Editor
{
    void DrawShapeGeometry(AzFramework::DebugDisplayRequests& debugDisplay, const ShapeGeometry& geometry, const AZ::Matrix3x4& transform);

    [[nodiscard]] AZ::Aabb CalculateShapeBounds(const ShapeGeometry& geometry, const AZ::Matrix3x4& transform);
    [[nodiscard]] bool IntersectEditorBounds(
        const AZ::Aabb& bounds, const AZ::Vector3& rayStart, const AZ::Vector3& rayDirection, float& distance);
} // namespace Box3D::Editor
