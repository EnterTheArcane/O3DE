/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/CustomShape.h>
#include <Jolt/NativeShapeFactory.h>

namespace Jolt
{
    void RegisterCustomConvexShapeType();

    [[nodiscard]]
    NativeShapeResult CreateNativeCustomConvexShape(
        const CustomConvexShapeData& data,
        const CustomConvexShapeInfo& info,
        const JPH::PhysicsMaterial* material,
        float density);

    [[nodiscard]]
    bool GetNativeCustomConvexShapeInfo(
        const JPH::Shape& shape,
        CustomConvexShapeInfo& info);

    [[nodiscard]]
    bool IsNativeCustomConvexShapeValid(const JPH::Shape& shape);
} // namespace Jolt
