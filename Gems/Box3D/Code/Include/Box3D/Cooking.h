/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Box3D/Handle.h>
#include <Box3D/Queries.h>
#include <Box3D/ShapeConfiguration.h>
#include <Box3D/TypeIds.h>

#include <AzCore/Interface/Interface.h>
#include <AzCore/Math/Aabb.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/RTTI/RTTI.h>

namespace Box3D
{
    //! Owns reusable native geometry without exposing Box3D C API data or allocation details.
    class ICooking
    {
    public:
        AZ_RTTI(ICooking, ICookingTypeId);

        virtual ~ICooking() = default;

        //! Bakes geometry, local transform, and transient material handles for repeated shape creation.
        //! Serialized material configurations must be resolved to handles before cooking.
        [[nodiscard]] virtual CookedShapeHandle CookShape(const ShapeConfiguration& configuration) = 0;
        virtual bool DestroyCookedShape(CookedShapeHandle cookedShapeHandle) = 0;
        [[nodiscard]] virtual bool IsValid(CookedShapeHandle cookedShapeHandle) const = 0;
        [[nodiscard]] virtual AZ::Aabb GetAabb(CookedShapeHandle cookedShapeHandle) const = 0;
        [[nodiscard]] virtual bool Raycast(
            CookedShapeHandle cookedShapeHandle,
            const AZ::Vector3& start,
            const AZ::Vector3& direction,
            float distance,
            GeometryHit& hit) const = 0;
    };
} // namespace Box3D
