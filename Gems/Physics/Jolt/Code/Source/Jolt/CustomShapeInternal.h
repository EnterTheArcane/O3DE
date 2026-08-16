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
    void RegisterCustomShapeType();

    [[nodiscard]]
    NativeShapeResult CreateNativeCustomShape(
        const CustomShapeData& data,
        const CustomShapeInfo& info,
        ICustomShapeProvider& provider,
        const JPH::PhysicsMaterialList& materials,
        float density,
        AZ::u64 userData);

    [[nodiscard]]
    bool BindNativeCustomShapeProvider(
        JPH::Shape& shape,
        ICustomShapeProvider& provider);

    [[nodiscard]]
    bool GetNativeCustomShapeInfo(
        const JPH::Shape& shape,
        CustomShapeInfo& info);

    [[nodiscard]]
    bool IsNativeCustomShapeValid(const JPH::Shape& shape);
} // namespace Jolt
