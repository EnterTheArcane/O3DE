/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Box3D/Collision.h>
#include <Box3D/Handle.h>
#include <Box3D/Material.h>

#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Vector2.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/containers/variant.h>
#include <AzCore/std/containers/vector.h>

namespace AZ
{
    class ReflectContext;
}

namespace Box3D
{
    struct SphereShapeConfiguration final
    {
        AZ_TYPE_INFO(SphereShapeConfiguration, "{8C4A9A92-F96B-4920-A732-364DF6A9D5D4}");

        float m_radius = 0.5f;
    };

    struct CapsuleShapeConfiguration final
    {
        AZ_TYPE_INFO(CapsuleShapeConfiguration, "{5043C975-A482-412E-8294-0B5133159032}");

        float m_height = 1.0f;
        float m_radius = 0.25f;
    };

    struct BoxShapeConfiguration final
    {
        AZ_TYPE_INFO(BoxShapeConfiguration, "{0CD4E46B-3B95-41B2-8D93-C01982B74F0C}");

        AZ::Vector3 m_halfExtents = AZ::Vector3::CreateOne() * 0.5f;
    };

    struct CylinderShapeConfiguration final
    {
        AZ_TYPE_INFO(CylinderShapeConfiguration, "{12482589-A856-48D2-A77C-D70D8144EEDF}");

        float m_height = 1.0f;
        float m_radius = 0.5f;
        AZ::u32 m_sideCount = 16;
    };

    struct ConvexHullShapeConfiguration final
    {
        AZ_TYPE_INFO(ConvexHullShapeConfiguration, "{A15415E2-39C8-4AE4-9F99-2092E5EA198D}");

        AZStd::vector<AZ::Vector3> m_vertices;
    };

    struct TriangleMeshShapeConfiguration final
    {
        AZ_TYPE_INFO(TriangleMeshShapeConfiguration, "{5513044E-373C-48C8-A341-581348C92A7A}");

        AZStd::vector<AZ::Vector3> m_vertices;
        AZStd::vector<AZ::u32> m_indices;
        AZStd::vector<AZ::u8> m_materialIndices;

        float m_weldTolerance = 0.0f;

        bool m_weldVertices = false;
        bool m_useMedianSplit = false;
        bool m_identifyEdges = true;
    };

    struct HeightfieldShapeConfiguration final
    {
        AZ_TYPE_INFO(HeightfieldShapeConfiguration, "{19AD694A-F75B-4DDA-AC0D-D2AD51AD6B9A}");

        AZStd::vector<float> m_samples;
        AZStd::vector<AZ::u8> m_materialIndices;

        AZ::u32 m_columnCount = 0;
        AZ::u32 m_rowCount = 0;
        AZ::Vector2 m_sampleSpacing = AZ::Vector2::CreateOne();

        float m_heightScale = 1.0f;
        float m_minimumHeight = 0.0f;
        float m_maximumHeight = 0.0f;

        bool m_clockwise = false;
        bool m_useSharedHeightRange = false;
    };

    using CompoundChildGeometry = AZStd::variant<
        SphereShapeConfiguration,
        CapsuleShapeConfiguration,
        BoxShapeConfiguration,
        CylinderShapeConfiguration,
        ConvexHullShapeConfiguration,
        TriangleMeshShapeConfiguration>;

    enum class CompoundChildType : AZ::u8
    {
        Sphere,
        Capsule,
        Box,
        Cylinder,
        ConvexHull,
        TriangleMesh,
    };

    struct CompoundChildShapeConfiguration final
    {
        AZ_TYPE_INFO(CompoundChildShapeConfiguration, "{59A80A66-FE08-4010-A5DD-E25B25BC9970}");

        [[nodiscard]]
        CompoundChildType GetType() const;

        [[nodiscard]]
        SphereShapeConfiguration GetSphere() const;

        void SetSphere(const SphereShapeConfiguration& geometry);

        [[nodiscard]]
        CapsuleShapeConfiguration GetCapsule() const;

        void SetCapsule(const CapsuleShapeConfiguration& geometry);

        [[nodiscard]]
        BoxShapeConfiguration GetBox() const;

        void SetBox(const BoxShapeConfiguration& geometry);

        [[nodiscard]]
        CylinderShapeConfiguration GetCylinder() const;

        void SetCylinder(const CylinderShapeConfiguration& geometry);

        [[nodiscard]]
        ConvexHullShapeConfiguration GetConvexHull() const;

        void SetConvexHull(const ConvexHullShapeConfiguration& geometry);

        [[nodiscard]]
        TriangleMeshShapeConfiguration GetTriangleMesh() const;

        void SetTriangleMesh(const TriangleMeshShapeConfiguration& geometry);

        CompoundChildGeometry m_geometry;
        AZ::Transform m_localTransform = AZ::Transform::CreateIdentity();

        AZ::u8 m_materialIndex = 0;
    };

    struct CompoundShapeConfiguration final
    {
        AZ_TYPE_INFO(CompoundShapeConfiguration, "{7D3F81B4-7431-4A68-A3C6-0C40D5E6761A}");

        AZStd::vector<CompoundChildShapeConfiguration> m_children;
    };

    using ShapeGeometry = AZStd::variant<
        SphereShapeConfiguration,
        CapsuleShapeConfiguration,
        BoxShapeConfiguration,
        CylinderShapeConfiguration,
        ConvexHullShapeConfiguration,
        TriangleMeshShapeConfiguration,
        HeightfieldShapeConfiguration,
        CompoundShapeConfiguration>;

    //! Per-instance transform, filtering, material assignment, event policy, and bulk-creation policy.
    struct ShapeProperties final
    {
        AZ_TYPE_INFO(ShapeProperties, "{BFE8F091-53A5-4409-B1CF-319ABEC9DA7A}");

        void SetMaterials(
            const MaterialHandleCollection& materials)
        {
            const AZStd::span<const MaterialHandle> handles = materials.GetHandles();
            m_materials.assign(
                handles.begin(),
                handles.end());
        }

        [[nodiscard]]
        MaterialHandleCollection GetMaterials() const
        {
            return MaterialHandleCollection(m_materials);
        }

        //! Transient material handles for callers that manage materials through ISystem directly.
        AZStd::vector<MaterialHandle> m_materials;

        AZ::Transform m_localTransform = AZ::Transform::CreateIdentity();
        CollisionFilter m_collisionFilter;

        float m_density = 1000.0f;
        float m_explosionScale = 1.0f;

        bool m_isSensor = false;
        bool m_enableSensorEvents = false;
        bool m_enableContactEvents = true;
        bool m_enableHitEvents = true;
        bool m_enableCustomFiltering = false;
        bool m_enablePreSolveEvents = false;
        bool m_createContactsImmediately = true;
        bool m_updateBodyMass = true;
    };

    //! Geometry and per-instance properties for one attached shape.
    struct ShapeConfiguration final
    {
        AZ_TYPE_INFO(ShapeConfiguration, "{E047314A-C9D8-4F7F-8A4B-8F52F595B06A}");

        static void Reflect(AZ::ReflectContext* context);

        void SetMaterialConfigurations(
            const MaterialConfigurationCollection& materials)
        {
            const AZStd::span<const MaterialConfiguration> configurations = materials.GetConfigurations();
            m_materialConfigurations.assign(
                configurations.begin(),
                configurations.end());
        }

        [[nodiscard]]
        MaterialConfigurationCollection GetMaterialConfigurations() const
        {
            return MaterialConfigurationCollection(m_materialConfigurations);
        }

        ShapeGeometry m_geometry;
        ShapeProperties m_properties;

        //! Serialized authoring data. Components create transient material handles when the shape is attached.
        AZStd::vector<MaterialConfiguration> m_materialConfigurations;
    };
} // namespace Box3D

namespace AZ
{
    AZ_TYPE_INFO_SPECIALIZE(Box3D::CompoundChildType, Box3D::CompoundChildTypeId);
}
