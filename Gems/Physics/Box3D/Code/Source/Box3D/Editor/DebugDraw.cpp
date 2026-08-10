/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/Editor/DebugDraw.h>

#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/Math/IntersectSegment.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/typetraits/is_same.h>
#include <AzCore/std/typetraits/remove_cvref.h>
#include <AzFramework/Entity/EntityDebugDisplayBus.h>

namespace Box3D::Editor
{
    namespace
    {
        template<typename Geometry>
        void DrawGeometry(AzFramework::DebugDisplayRequests& debugDisplay, const Geometry& geometry, const AZ::Matrix3x4& transform);

        void DrawTriangleMesh(
            AzFramework::DebugDisplayRequests& debugDisplay,
            const TriangleMeshShapeConfiguration& mesh,
            const AZ::Matrix3x4& transform)
        {
            debugDisplay.PushPremultipliedMatrix(transform);
            for (size_t index = 0; index + 2 < mesh.m_indices.size(); index += 3)
            {
                const AZ::u32 indexA = mesh.m_indices[index];
                const AZ::u32 indexB = mesh.m_indices[index + 1];
                const AZ::u32 indexC = mesh.m_indices[index + 2];
                if (indexA >= mesh.m_vertices.size() || indexB >= mesh.m_vertices.size() || indexC >= mesh.m_vertices.size())
                {
                    continue;
                }
                const AZStd::array triangle{mesh.m_vertices[indexA], mesh.m_vertices[indexB], mesh.m_vertices[indexC]};
                debugDisplay.DrawPolyLine(triangle, true);
            }
            debugDisplay.PopPremultipliedMatrix();
        }

        void DrawHeightfield(
            AzFramework::DebugDisplayRequests& debugDisplay,
            const HeightfieldShapeConfiguration& heightfield,
            const AZ::Matrix3x4& transform)
        {
            if (heightfield.m_columnCount < 2
                || heightfield.m_rowCount < 2
                || heightfield.m_samples.size() < heightfield.m_columnCount * heightfield.m_rowCount)
            {
                return;
            }

            debugDisplay.PushPremultipliedMatrix(transform);
            const auto point = [&heightfield](const AZ::u32 column, const AZ::u32 row)
            {
                const float height = heightfield.m_samples[row * heightfield.m_columnCount + column];
                return AZ::Vector3(
                    heightfield.m_sampleSpacing.GetX() * static_cast<float>(column),
                    heightfield.m_sampleSpacing.GetY() * static_cast<float>(row),
                    heightfield.m_heightScale * height);
            };
            for (AZ::u32 row = 0; row < heightfield.m_rowCount; ++row)
            {
                for (AZ::u32 column = 0; column < heightfield.m_columnCount; ++column)
                {
                    if (column + 1 < heightfield.m_columnCount)
                    {
                        debugDisplay.DrawLine(point(column, row), point(column + 1, row));
                    }
                    if (row + 1 < heightfield.m_rowCount)
                    {
                        debugDisplay.DrawLine(point(column, row), point(column, row + 1));
                    }
                }
            }
            debugDisplay.PopPremultipliedMatrix();
        }

        template<typename Geometry>
        void DrawGeometry(
            AzFramework::DebugDisplayRequests& debugDisplay,
            const Geometry& geometry,
            const AZ::Matrix3x4& transform)
        {
            AZStd::visit(
                [&debugDisplay, &transform](const auto& typedGeometry)
                {
                    using Type = AZStd::remove_cvref_t<decltype(typedGeometry)>;
                    if constexpr (AZStd::is_same_v<Type, TriangleMeshShapeConfiguration>)
                    {
                        DrawTriangleMesh(debugDisplay, typedGeometry, transform);
                    }
                    else if constexpr (AZStd::is_same_v<Type, HeightfieldShapeConfiguration>)
                    {
                        DrawHeightfield(debugDisplay, typedGeometry, transform);
                    }
                    else if constexpr (AZStd::is_same_v<Type, CompoundShapeConfiguration>)
                    {
                        for (const CompoundChildShapeConfiguration& child : typedGeometry.m_children)
                        {
                            const AZ::Matrix3x4 childTransform = transform * AZ::Matrix3x4::CreateFromTransform(child.m_localTransform);
                            DrawGeometry(debugDisplay, child.m_geometry, childTransform);
                        }
                    }
                    else
                    {
                        debugDisplay.PushPremultipliedMatrix(transform);
                        if constexpr (AZStd::is_same_v<Type, SphereShapeConfiguration>)
                        {
                            debugDisplay.DrawWireSphere(AZ::Vector3::CreateZero(), typedGeometry.m_radius);
                        }
                        else if constexpr (AZStd::is_same_v<Type, CapsuleShapeConfiguration>)
                        {
                            debugDisplay.DrawWireCapsule(
                                AZ::Vector3::CreateZero(),
                                AZ::Vector3::CreateAxisZ(),
                                typedGeometry.m_radius,
                                AZStd::max(typedGeometry.m_height - 2.0f * typedGeometry.m_radius, 0.0f));
                        }
                        else if constexpr (AZStd::is_same_v<Type, BoxShapeConfiguration>)
                        {
                            debugDisplay.DrawWireBox(-typedGeometry.m_halfExtents, typedGeometry.m_halfExtents);
                        }
                        else if constexpr (AZStd::is_same_v<Type, CylinderShapeConfiguration>)
                        {
                            debugDisplay.DrawWireCylinder(
                                AZ::Vector3::CreateZero(), AZ::Vector3::CreateAxisZ(), typedGeometry.m_radius, typedGeometry.m_height);
                        }
                        else if constexpr (AZStd::is_same_v<Type, ConvexHullShapeConfiguration>)
                        {
                            debugDisplay.DrawPolyLine(typedGeometry.m_vertices, true);
                        }
                        debugDisplay.PopPremultipliedMatrix();
                    }
                },
                geometry);
        }

        void AddBoxBounds(
            AZ::Aabb& bounds,
            const AZ::Vector3& minimum,
            const AZ::Vector3& maximum,
            const AZ::Matrix3x4& transform)
        {
            for (AZ::u32 corner = 0; corner < 8; ++corner)
            {
                AZ::Vector3 point = minimum;
                if ((corner & 1) != 0)
                {
                    point.SetX(maximum.GetX());
                }
                if ((corner & 2) != 0)
                {
                    point.SetY(maximum.GetY());
                }
                if ((corner & 4) != 0)
                {
                    point.SetZ(maximum.GetZ());
                }
                bounds.AddPoint(transform.TransformPoint(point));
            }
        }

        template<typename GeometryVariant>
        void AddGeometryBounds(
            AZ::Aabb& bounds,
            const GeometryVariant& geometry,
            const AZ::Matrix3x4& transform)
        {
            AZStd::visit(
                [&bounds, &transform](const auto& typedGeometry)
                {
                    using Type = AZStd::remove_cvref_t<decltype(typedGeometry)>;
                    if constexpr (AZStd::is_same_v<Type, SphereShapeConfiguration>)
                    {
                        const AZ::Vector3 extent(typedGeometry.m_radius);
                        AddBoxBounds(bounds, -extent, extent, transform);
                    }
                    else if constexpr (AZStd::is_same_v<Type, CapsuleShapeConfiguration>)
                    {
                        const AZ::Vector3 extent(
                            typedGeometry.m_radius,
                            typedGeometry.m_radius,
                            AZStd::max(0.5f * typedGeometry.m_height, typedGeometry.m_radius));
                        AddBoxBounds(bounds, -extent, extent, transform);
                    }
                    else if constexpr (AZStd::is_same_v<Type, BoxShapeConfiguration>)
                    {
                        AddBoxBounds(bounds, -typedGeometry.m_halfExtents, typedGeometry.m_halfExtents, transform);
                    }
                    else if constexpr (AZStd::is_same_v<Type, CylinderShapeConfiguration>)
                    {
                        const AZ::Vector3 extent(typedGeometry.m_radius, typedGeometry.m_radius, 0.5f * typedGeometry.m_height);
                        AddBoxBounds(bounds, -extent, extent, transform);
                    }
                    else if constexpr (AZStd::is_same_v<Type, ConvexHullShapeConfiguration>)
                    {
                        for (const AZ::Vector3& vertex : typedGeometry.m_vertices)
                        {
                            bounds.AddPoint(transform.TransformPoint(vertex));
                        }
                    }
                    else if constexpr (AZStd::is_same_v<Type, TriangleMeshShapeConfiguration>)
                    {
                        for (const AZ::Vector3& vertex : typedGeometry.m_vertices)
                        {
                            bounds.AddPoint(transform.TransformPoint(vertex));
                        }
                    }
                    else if constexpr (AZStd::is_same_v<Type, HeightfieldShapeConfiguration>)
                    {
                        const size_t sampleCount = AZStd::min(
                            typedGeometry.m_samples.size(),
                            static_cast<size_t>(typedGeometry.m_columnCount) * typedGeometry.m_rowCount);
                        for (size_t sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
                        {
                            const AZ::u32 column = aznumeric_cast<AZ::u32>(sampleIndex % typedGeometry.m_columnCount);
                            const AZ::u32 row = aznumeric_cast<AZ::u32>(sampleIndex / typedGeometry.m_columnCount);
                            const AZ::Vector3 point(
                                typedGeometry.m_sampleSpacing.GetX() * static_cast<float>(column),
                                typedGeometry.m_sampleSpacing.GetY() * static_cast<float>(row),
                                typedGeometry.m_heightScale * typedGeometry.m_samples[sampleIndex]);
                            bounds.AddPoint(transform.TransformPoint(point));
                        }
                    }
                    else if constexpr (AZStd::is_same_v<Type, CompoundShapeConfiguration>)
                    {
                        for (const CompoundChildShapeConfiguration& child : typedGeometry.m_children)
                        {
                            const AZ::Matrix3x4 childTransform = transform * AZ::Matrix3x4::CreateFromTransform(child.m_localTransform);
                            AddGeometryBounds(bounds, child.m_geometry, childTransform);
                        }
                    }
                },
                geometry);
        }
    } // namespace

    void DrawShapeGeometry(
        AzFramework::DebugDisplayRequests& debugDisplay,
        const ShapeGeometry& geometry,
        const AZ::Matrix3x4& transform)
    {
        DrawGeometry(debugDisplay, geometry, transform);
    }

    AZ::Aabb CalculateShapeBounds(
        const ShapeGeometry& geometry,
        const AZ::Matrix3x4& transform)
    {
        AZ::Aabb bounds = AZ::Aabb::CreateNull();
        AddGeometryBounds(bounds, geometry, transform);
        return bounds;
    }

    bool IntersectEditorBounds(
        const AZ::Aabb& bounds,
        const AZ::Vector3& rayStart,
        const AZ::Vector3& rayDirection,
        float& distance)
    {
        if (!bounds.IsValid() || rayDirection.IsZero())
        {
            return false;
        }

        constexpr float rayLength = 10000.0f;
        const AZ::Vector3 scaledDirection = rayDirection.GetNormalized() * rayLength;
        float start = 0.0f;
        float end = 0.0f;
        AZ::Vector3 normal;
        if (AZ::Intersect::IntersectRayAABB(rayStart, scaledDirection, scaledDirection.GetReciprocal(), bounds, start, end, normal)
            != AZ::Intersect::RayAABBIsectTypes::ISECT_RAY_AABB_ISECT)
        {
            return false;
        }

        distance = start * rayLength;
        return true;
    }
} // namespace Box3D::Editor
