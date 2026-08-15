/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Query.h>
#include <Jolt/ShapeConfiguration.h>

#include <AzCore/std/string/string.h>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/PhysicsMaterial.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

namespace JPH
{
    class MeshShape;
} // namespace JPH

namespace Jolt
{
    struct NativeShapeResult final
    {
        explicit operator bool() const noexcept
        {
            return static_cast<bool>(m_shape);
        }

        JPH::RefConst<JPH::Shape> m_shape;
        AZStd::string m_error;
    };

    [[nodiscard]]
    NativeShapeResult CreateNativeShape(
        const ShapeConfiguration& configuration,
        const JPH::PhysicsMaterialList& materials);

    [[nodiscard]]
    bool IsPotentiallyValidSubShapeId(
        const JPH::Shape& shape,
        SubShapeId subShapeId);

    [[nodiscard]]
    bool GetNativeDirectChildShape(
        const JPH::Shape& shape,
        SubShapeId subShapeId,
        SubShapeTransform& transform,
        AZ::u32& childIndex);

    [[nodiscard]]
    const JPH::MeshShape* FindNativeMesh(const JPH::Shape& shape);

    [[nodiscard]]
    bool GetNativeMeshTriangleMaterialIndex(
        const JPH::Shape& shape,
        SubShapeId subShapeId,
        AZ::u32& materialIndex);

    [[nodiscard]]
    bool GetNativeMeshTriangleUserData(
        const JPH::Shape& shape,
        SubShapeId subShapeId,
        AZ::u32& userData);
} // namespace Jolt
