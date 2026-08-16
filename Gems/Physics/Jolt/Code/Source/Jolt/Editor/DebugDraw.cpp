/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/Editor/DebugDraw.h>

#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/Math/IntersectSegment.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/typetraits/is_same.h>
#include <AzCore/std/typetraits/remove_cvref.h>
#include <AzFramework/Entity/EntityDebugDisplayBus.h>

namespace Jolt::Editor
{
    namespace
    {
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

        void DrawMesh(
            AzFramework::DebugDisplayRequests& debugDisplay,
            const MeshShapeConfiguration& mesh,
            const AZ::Matrix3x4& transform)
        {
            debugDisplay.PushPremultipliedMatrix(transform);
            for (const MeshTriangle& triangle : mesh.m_triangles)
            {
                if (triangle.m_firstVertex >= mesh.m_vertices.size()
                    || triangle.m_secondVertex >= mesh.m_vertices.size()
                    || triangle.m_thirdVertex >= mesh.m_vertices.size())
                {
                    continue;
                }
                const AZStd::array points = {
                    mesh.m_vertices[triangle.m_firstVertex],
                    mesh.m_vertices[triangle.m_secondVertex],
                    mesh.m_vertices[triangle.m_thirdVertex],
                };
                debugDisplay.DrawPolyLine(points, true);
            }
            debugDisplay.PopPremultipliedMatrix();
        }

        void DrawHeightfield(
            AzFramework::DebugDisplayRequests& debugDisplay,
            const HeightfieldShapeConfiguration& heightfield,
            const AZ::Matrix3x4& transform)
        {
            if (heightfield.m_sampleCount < 2
                || heightfield.m_heights.size()
                    < static_cast<size_t>(heightfield.m_sampleCount) * heightfield.m_sampleCount)
            {
                return;
            }

            const auto getPoint = [&heightfield](
                const AZ::u32 column,
                const AZ::u32 row)
            {
                return heightfield.m_origin + AZ::Vector3(
                    heightfield.m_spacing.GetX() * column,
                    heightfield.m_spacing.GetY() * row,
                    heightfield.m_heights[row * heightfield.m_sampleCount + column]);
            };

            debugDisplay.PushPremultipliedMatrix(transform);
            for (AZ::u32 row = 0; row < heightfield.m_sampleCount; ++row)
            {
                for (AZ::u32 column = 0; column < heightfield.m_sampleCount; ++column)
                {
                    if (column + 1 < heightfield.m_sampleCount)
                    {
                        debugDisplay.DrawLine(getPoint(column, row), getPoint(column + 1, row));
                    }
                    if (row + 1 < heightfield.m_sampleCount)
                    {
                        debugDisplay.DrawLine(getPoint(column, row), getPoint(column, row + 1));
                    }
                }
            }
            debugDisplay.PopPremultipliedMatrix();
        }

        template<class Geometry>
        void DrawGeometry(
            AzFramework::DebugDisplayRequests& debugDisplay,
            const Geometry& geometry,
            const AZ::Matrix3x4& transform)
        {
            AZStd::visit(
                [&debugDisplay, &transform](const auto& typedGeometry)
                {
                    using Type = AZStd::remove_cvref_t<decltype(typedGeometry)>;
                    if constexpr (AZStd::is_same_v<Type, MeshShapeConfiguration>)
                    {
                        DrawMesh(debugDisplay, typedGeometry, transform);
                    }
                    else if constexpr (AZStd::is_same_v<Type, HeightfieldShapeConfiguration>)
                    {
                        DrawHeightfield(debugDisplay, typedGeometry, transform);
                    }
                    else
                    {
                        debugDisplay.PushPremultipliedMatrix(transform);
                        if constexpr (AZStd::is_same_v<Type, BoxShapeConfiguration>)
                        {
                            const AZ::Vector3 halfExtents = 0.5f * typedGeometry.m_dimensions;
                            debugDisplay.DrawWireBox(-halfExtents, halfExtents);
                        }
                        else if constexpr (AZStd::is_same_v<Type, CapsuleShapeConfiguration>)
                        {
                            debugDisplay.DrawWireCapsule(
                                AZ::Vector3::CreateZero(),
                                AZ::Vector3::CreateAxisY(),
                                typedGeometry.m_radius,
                                typedGeometry.m_cylinderHeight);
                        }
                        else if constexpr (AZStd::is_same_v<Type, ConvexHullShapeConfiguration>)
                        {
                            debugDisplay.DrawPolyLine(typedGeometry.m_points, true);
                        }
                        else if constexpr (
                            AZStd::is_same_v<Type, CustomConvexShapeConfiguration>
                            || AZStd::is_same_v<Type, CustomShapeConfiguration>)
                        {
                            if (typedGeometry.m_editorBounds.IsValid())
                            {
                                debugDisplay.DrawWireBox(
                                    typedGeometry.m_editorBounds.GetMin(),
                                    typedGeometry.m_editorBounds.GetMax());
                            }
                        }
                        else if constexpr (AZStd::is_same_v<Type, CylinderShapeConfiguration>)
                        {
                            debugDisplay.DrawWireCylinder(
                                AZ::Vector3::CreateZero(),
                                AZ::Vector3::CreateAxisY(),
                                typedGeometry.m_radius,
                                typedGeometry.m_height);
                        }
                        else if constexpr (AZStd::is_same_v<Type, PlaneShapeConfiguration>)
                        {
                            const AZ::Vector3 center = typedGeometry.m_normal * typedGeometry.m_distance;
                            debugDisplay.DrawWireDisk(center, typedGeometry.m_normal, typedGeometry.m_halfExtent);
                        }
                        else if constexpr (AZStd::is_same_v<Type, SphereShapeConfiguration>)
                        {
                            debugDisplay.DrawWireSphere(AZ::Vector3::CreateZero(), typedGeometry.m_radius);
                        }
                        else if constexpr (AZStd::is_same_v<Type, TaperedCapsuleShapeConfiguration>)
                        {
                            const AZ::Vector3 bottom = AZ::Vector3::CreateAxisY(-0.5f * typedGeometry.m_height);
                            const AZ::Vector3 top = AZ::Vector3::CreateAxisY(0.5f * typedGeometry.m_height);
                            debugDisplay.DrawWireSphere(bottom, typedGeometry.m_bottomRadius);
                            debugDisplay.DrawWireSphere(top, typedGeometry.m_topRadius);
                            debugDisplay.DrawLine(
                                bottom + AZ::Vector3::CreateAxisX(typedGeometry.m_bottomRadius),
                                top + AZ::Vector3::CreateAxisX(typedGeometry.m_topRadius));
                            debugDisplay.DrawLine(
                                bottom - AZ::Vector3::CreateAxisX(typedGeometry.m_bottomRadius),
                                top - AZ::Vector3::CreateAxisX(typedGeometry.m_topRadius));
                        }
                        else if constexpr (AZStd::is_same_v<Type, TaperedCylinderShapeConfiguration>)
                        {
                            const AZ::Vector3 bottom = AZ::Vector3::CreateAxisY(-0.5f * typedGeometry.m_height);
                            const AZ::Vector3 top = AZ::Vector3::CreateAxisY(0.5f * typedGeometry.m_height);
                            debugDisplay.DrawWireDisk(bottom, AZ::Vector3::CreateAxisY(), typedGeometry.m_bottomRadius);
                            debugDisplay.DrawWireDisk(top, AZ::Vector3::CreateAxisY(), typedGeometry.m_topRadius);
                            debugDisplay.DrawLine(
                                bottom + AZ::Vector3::CreateAxisX(typedGeometry.m_bottomRadius),
                                top + AZ::Vector3::CreateAxisX(typedGeometry.m_topRadius));
                            debugDisplay.DrawLine(
                                bottom - AZ::Vector3::CreateAxisX(typedGeometry.m_bottomRadius),
                                top - AZ::Vector3::CreateAxisX(typedGeometry.m_topRadius));
                        }
                        else if constexpr (AZStd::is_same_v<Type, TriangleShapeConfiguration>)
                        {
                            const AZStd::array points = {
                                typedGeometry.m_firstVertex,
                                typedGeometry.m_secondVertex,
                                typedGeometry.m_thirdVertex,
                            };
                            debugDisplay.DrawPolyLine(points, true);
                        }
                        debugDisplay.PopPremultipliedMatrix();
                    }
                },
                geometry);
        }

        template<class Geometry>
        void AddGeometryBounds(
            AZ::Aabb& bounds,
            const Geometry& geometry,
            const AZ::Matrix3x4& transform)
        {
            AZStd::visit(
                [&bounds, &transform](const auto& typedGeometry)
                {
                    using Type = AZStd::remove_cvref_t<decltype(typedGeometry)>;
                    if constexpr (AZStd::is_same_v<Type, BoxShapeConfiguration>)
                    {
                        const AZ::Vector3 halfExtents = 0.5f * typedGeometry.m_dimensions;
                        AddBoxBounds(bounds, -halfExtents, halfExtents, transform);
                    }
                    else if constexpr (AZStd::is_same_v<Type, CapsuleShapeConfiguration>)
                    {
                        const AZ::Vector3 extent(
                            typedGeometry.m_radius,
                            0.5f * typedGeometry.m_cylinderHeight + typedGeometry.m_radius,
                            typedGeometry.m_radius);
                        AddBoxBounds(bounds, -extent, extent, transform);
                    }
                    else if constexpr (AZStd::is_same_v<Type, CylinderShapeConfiguration>)
                    {
                        const AZ::Vector3 extent(
                            typedGeometry.m_radius,
                            0.5f * typedGeometry.m_height,
                            typedGeometry.m_radius);
                        AddBoxBounds(bounds, -extent, extent, transform);
                    }
                    else if constexpr (AZStd::is_same_v<Type, SphereShapeConfiguration>)
                    {
                        const AZ::Vector3 extent(typedGeometry.m_radius);
                        AddBoxBounds(bounds, -extent, extent, transform);
                    }
                    else if constexpr (
                        AZStd::is_same_v<Type, TaperedCapsuleShapeConfiguration>
                        || AZStd::is_same_v<Type, TaperedCylinderShapeConfiguration>)
                    {
                        const float radius = AZStd::max(typedGeometry.m_bottomRadius, typedGeometry.m_topRadius);
                        float halfHeight = 0.5f * typedGeometry.m_height;
                        if constexpr (AZStd::is_same_v<Type, TaperedCapsuleShapeConfiguration>)
                        {
                            halfHeight += radius;
                        }
                        const AZ::Vector3 extent(radius, halfHeight, radius);
                        AddBoxBounds(bounds, -extent, extent, transform);
                    }
                    else if constexpr (AZStd::is_same_v<Type, PlaneShapeConfiguration>)
                    {
                        const AZ::Vector3 center = typedGeometry.m_normal * typedGeometry.m_distance;
                        const AZ::Vector3 extent(typedGeometry.m_halfExtent);
                        AddBoxBounds(bounds, center - extent, center + extent, transform);
                    }
                    else if constexpr (AZStd::is_same_v<Type, ConvexHullShapeConfiguration>)
                    {
                        for (const AZ::Vector3& point : typedGeometry.m_points)
                        {
                            bounds.AddPoint(transform.TransformPoint(point));
                        }
                    }
                    else if constexpr (
                        AZStd::is_same_v<Type, CustomConvexShapeConfiguration>
                        || AZStd::is_same_v<Type, CustomShapeConfiguration>)
                    {
                        if (typedGeometry.m_editorBounds.IsValid())
                        {
                            AddBoxBounds(
                                bounds,
                                typedGeometry.m_editorBounds.GetMin(),
                                typedGeometry.m_editorBounds.GetMax(),
                                transform);
                        }
                    }
                    else if constexpr (AZStd::is_same_v<Type, MeshShapeConfiguration>)
                    {
                        for (const AZ::Vector3& point : typedGeometry.m_vertices)
                        {
                            bounds.AddPoint(transform.TransformPoint(point));
                        }
                    }
                    else if constexpr (AZStd::is_same_v<Type, TriangleShapeConfiguration>)
                    {
                        bounds.AddPoint(transform.TransformPoint(typedGeometry.m_firstVertex));
                        bounds.AddPoint(transform.TransformPoint(typedGeometry.m_secondVertex));
                        bounds.AddPoint(transform.TransformPoint(typedGeometry.m_thirdVertex));
                    }
                    else if constexpr (AZStd::is_same_v<Type, HeightfieldShapeConfiguration>)
                    {
                        const size_t sampleCount = AZStd::min(
                            typedGeometry.m_heights.size(),
                            static_cast<size_t>(typedGeometry.m_sampleCount) * typedGeometry.m_sampleCount);
                        for (size_t sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
                        {
                            const AZ::u32 column = aznumeric_cast<AZ::u32>(sampleIndex % typedGeometry.m_sampleCount);
                            const AZ::u32 row = aznumeric_cast<AZ::u32>(sampleIndex / typedGeometry.m_sampleCount);
                            const AZ::Vector3 point = typedGeometry.m_origin + AZ::Vector3(
                                typedGeometry.m_spacing.GetX() * column,
                                typedGeometry.m_spacing.GetY() * row,
                                typedGeometry.m_heights[sampleIndex]);
                            bounds.AddPoint(transform.TransformPoint(point));
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

        constexpr float rayLength = 10'000.0f;
        const AZ::Vector3 scaledDirection = rayDirection.GetNormalized() * rayLength;
        float start = 0.0f;
        float end = 0.0f;
        AZ::Vector3 normal;
        if (AZ::Intersect::IntersectRayAABB(
            rayStart,
            scaledDirection,
            scaledDirection.GetReciprocal(),
            bounds,
            start,
            end,
            normal)
            != AZ::Intersect::RayAABBIsectTypes::ISECT_RAY_AABB_ISECT)
        {
            return false;
        }

        distance = start * rayLength;
        return true;
    }
} // namespace Jolt::Editor
