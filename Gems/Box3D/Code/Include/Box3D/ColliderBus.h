/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Box3D/ShapeConfiguration.h>

#include <AzCore/Component/ComponentBus.h>
#include <AzCore/Math/Aabb.h>
#include <AzCore/std/containers/span.h>

#include <cstddef>

namespace Box3D
{
    class ColliderRequests
        : public AZ::ComponentBus
    {
    public:
        [[nodiscard]] virtual AZStd::span<const ShapeConfiguration> GetShapeConfigurations() const = 0;
        [[nodiscard]] virtual AZStd::span<const ShapeHandle> GetShapeHandles() const = 0;
        [[nodiscard]] virtual size_t GetShapeCount() const
        {
            return GetShapeHandles().size();
        }
        [[nodiscard]] virtual ShapeHandle GetShapeHandleAt(size_t index) const
        {
            const AZStd::span<const ShapeHandle> handles = GetShapeHandles();
            return index < handles.size() ? handles[index] : ShapeHandle{};
        }
        [[nodiscard]] virtual AZ::Aabb GetAabb() const = 0;
        [[nodiscard]] virtual bool IsSensor() const = 0;
        virtual bool UpdateShape(size_t index, const ShapeConfiguration& configuration) = 0;
        virtual bool UpdateSphere(size_t index, const SphereShapeConfiguration& geometry, const ShapeProperties& properties)
        {
            return UpdateShape(index, ShapeConfiguration{ geometry, properties });
        }
        virtual bool UpdateCapsule(size_t index, const CapsuleShapeConfiguration& geometry, const ShapeProperties& properties)
        {
            return UpdateShape(index, ShapeConfiguration{ geometry, properties });
        }
        virtual bool UpdateBox(size_t index, const BoxShapeConfiguration& geometry, const ShapeProperties& properties)
        {
            return UpdateShape(index, ShapeConfiguration{ geometry, properties });
        }
        virtual bool UpdateCylinder(size_t index, const CylinderShapeConfiguration& geometry, const ShapeProperties& properties)
        {
            return UpdateShape(index, ShapeConfiguration{ geometry, properties });
        }
        virtual bool UpdateConvexHull(size_t index, const ConvexHullShapeConfiguration& geometry, const ShapeProperties& properties)
        {
            return UpdateShape(index, ShapeConfiguration{ geometry, properties });
        }
        virtual bool UpdateTriangleMesh(size_t index, const TriangleMeshShapeConfiguration& geometry, const ShapeProperties& properties)
        {
            return UpdateShape(index, ShapeConfiguration{ geometry, properties });
        }
        virtual bool UpdateHeightfield(size_t index, const HeightfieldShapeConfiguration& geometry, const ShapeProperties& properties)
        {
            return UpdateShape(index, ShapeConfiguration{ geometry, properties });
        }
        virtual bool UpdateCompound(size_t index, const CompoundShapeConfiguration& geometry, const ShapeProperties& properties)
        {
            return UpdateShape(index, ShapeConfiguration{ geometry, properties });
        }
        virtual bool SetCollisionFilter(size_t index, const CollisionFilter& collisionFilter) = 0;
        virtual bool SetMaterials(size_t index, AZStd::span<const MaterialHandle> materials) = 0;
        virtual bool SetMaterialsFromCollection(size_t index, const MaterialHandleCollection& materials)
        {
            return SetMaterials(index, materials.GetHandles());
        }
    };

    using ColliderRequestBus = AZ::EBus<ColliderRequests>;
} // namespace Box3D
