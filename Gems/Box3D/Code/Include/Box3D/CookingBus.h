/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Box3D/Cooking.h>

#include <AzCore/EBus/EBus.h>

namespace Box3D
{
    struct CookedRaycastResult final
    {
        AZ_TYPE_INFO(CookedRaycastResult, CookedRaycastResultTypeId);

        GeometryHit m_hit;
        bool m_found = false;
    };

    class CookingRequests
        : public AZ::EBusTraits
    {
    public:
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;

        [[nodiscard]] virtual CookedShapeHandle CookShape(const ShapeConfiguration& configuration) = 0;
        [[nodiscard]] virtual CookedShapeHandle CookSphere(const SphereShapeConfiguration& geometry, const ShapeProperties& properties)
        {
            return CookShape(ShapeConfiguration{ geometry, properties });
        }
        [[nodiscard]] virtual CookedShapeHandle CookCapsule(const CapsuleShapeConfiguration& geometry, const ShapeProperties& properties)
        {
            return CookShape(ShapeConfiguration{ geometry, properties });
        }
        [[nodiscard]] virtual CookedShapeHandle CookBox(const BoxShapeConfiguration& geometry, const ShapeProperties& properties)
        {
            return CookShape(ShapeConfiguration{ geometry, properties });
        }
        [[nodiscard]] virtual CookedShapeHandle CookCylinder(const CylinderShapeConfiguration& geometry, const ShapeProperties& properties)
        {
            return CookShape(ShapeConfiguration{ geometry, properties });
        }
        [[nodiscard]] virtual CookedShapeHandle CookConvexHull(
            const ConvexHullShapeConfiguration& geometry, const ShapeProperties& properties)
        {
            return CookShape(ShapeConfiguration{ geometry, properties });
        }
        [[nodiscard]] virtual CookedShapeHandle CookTriangleMesh(
            const TriangleMeshShapeConfiguration& geometry, const ShapeProperties& properties)
        {
            return CookShape(ShapeConfiguration{ geometry, properties });
        }
        [[nodiscard]] virtual CookedShapeHandle CookHeightfield(
            const HeightfieldShapeConfiguration& geometry, const ShapeProperties& properties)
        {
            return CookShape(ShapeConfiguration{ geometry, properties });
        }
        [[nodiscard]] virtual CookedShapeHandle CookCompound(const CompoundShapeConfiguration& geometry, const ShapeProperties& properties)
        {
            return CookShape(ShapeConfiguration{ geometry, properties });
        }
        virtual bool DestroyCookedShape(CookedShapeHandle cookedShapeHandle) = 0;
        [[nodiscard]] virtual bool IsCookedShapeValid(CookedShapeHandle cookedShapeHandle) const = 0;
        [[nodiscard]] virtual AZ::Aabb GetCookedShapeAabb(CookedShapeHandle cookedShapeHandle) const = 0;
        [[nodiscard]] virtual CookedRaycastResult RaycastCookedShape(
            CookedShapeHandle cookedShapeHandle, const AZ::Vector3& start, const AZ::Vector3& direction, float distance) const = 0;
    };

    using CookingRequestBus = AZ::EBus<CookingRequests>;
} // namespace Box3D
