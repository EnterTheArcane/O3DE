/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/CustomConvexShape.h>

#include <AzCore/Debug/Trace.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/std/utility/move.h>

#include <Jolt/Physics/Body/MassProperties.h>
#include <Jolt/Physics/Collision/CollideSoftBodyVertexIterator.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/TransformedShape.h>
#include <Jolt/Core/Color.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>
#if defined(JPH_DEBUG_RENDERER)
#include <Jolt/Renderer/DebugRenderer.h>
#endif

namespace Jolt
{
    namespace
    {
        class CustomConvexShape final
            : public JPH::ConvexShape
        {
        public:
            JPH_OVERRIDE_NEW_DELETE

            CustomConvexShape()
                : JPH::ConvexShape(JPH::EShapeSubType::UserConvex1)
            {
            }

            CustomConvexShape(
                const JPH::ConvexHullShape* hull,
                const CustomConvexShapeInfo& info,
                const JPH::PhysicsMaterial* material,
                const float density)
                : JPH::ConvexShape(JPH::EShapeSubType::UserConvex1, material)
                , m_hull(hull)
                , m_info(info)
            {
                SetDensity(density);
            }

            [[nodiscard]]
            const CustomConvexShapeInfo& GetInfo() const
            {
                return m_info;
            }

            [[nodiscard]]
            bool IsValid() const
            {
                return static_cast<bool>(m_hull);
            }

            JPH::Vec3 GetCenterOfMass() const override
            {
                if (m_hull)
                {
                    return m_hull->GetCenterOfMass();
                }

                return JPH::Vec3::sZero();
            }

            JPH::AABox GetLocalBounds() const override
            {
                if (m_hull)
                {
                    return m_hull->GetLocalBounds();
                }

                return JPH::AABox();
            }

            float GetInnerRadius() const override
            {
                if (m_hull)
                {
                    return m_hull->GetInnerRadius();
                }

                return 0.0f;
            }

            JPH::MassProperties GetMassProperties() const override
            {
                if (!m_hull)
                {
                    return {};
                }

                JPH::MassProperties result = m_hull->GetMassProperties();
                const float hullDensity = m_hull->GetDensity();
                if (hullDensity > 0.0f)
                {
                    result.ScaleToMass(result.mMass * GetDensity() / hullDensity);
                }
                return result;
            }

            JPH::Vec3 GetSurfaceNormal(
                const JPH::SubShapeID& subShapeId,
                const JPH::Vec3Arg localSurfacePosition) const override
            {
                if (m_hull)
                {
                    return m_hull->GetSurfaceNormal(subShapeId, localSurfacePosition);
                }

                return JPH::Vec3::sAxisY();
            }

            void GetSupportingFace(
                const JPH::SubShapeID& subShapeId,
                const JPH::Vec3Arg direction,
                const JPH::Vec3Arg scale,
                JPH::Mat44Arg centerOfMassTransform,
                SupportingFace& vertices) const override
            {
                if (m_hull)
                {
                    m_hull->GetSupportingFace(
                        subShapeId,
                        direction,
                        scale,
                        centerOfMassTransform,
                        vertices);
                }
            }

            const Support* GetSupportFunction(
                const ESupportMode mode,
                SupportBuffer& buffer,
                const JPH::Vec3Arg scale) const override
            {
                AZ_Assert(m_hull, "A custom convex shape must contain cooked geometry.");
                return m_hull->GetSupportFunction(mode, buffer, scale);
            }

            void CollidePoint(
                const JPH::Vec3Arg point,
                const JPH::SubShapeIDCreator& subShapeIdCreator,
                JPH::CollidePointCollector& collector,
                const JPH::ShapeFilter& shapeFilter) const override
            {
                JPH::ConvexShape::CollidePoint(point, subShapeIdCreator, collector, shapeFilter);
            }

            void CollideSoftBodyVertices(
                JPH::Mat44Arg centerOfMassTransform,
                const JPH::Vec3Arg scale,
                const JPH::CollideSoftBodyVertexIterator& vertices,
                const JPH::uint vertexCount,
                const int collidingShapeIndex) const override
            {
                if (m_hull)
                {
                    m_hull->CollideSoftBodyVertices(
                        centerOfMassTransform,
                        scale,
                        vertices,
                        vertexCount,
                        collidingShapeIndex);
                }
            }

            void GetTrianglesStart(
                GetTrianglesContext& context,
                const JPH::AABox& bounds,
                const JPH::Vec3Arg centerOfMassPosition,
                const JPH::QuatArg rotation,
                const JPH::Vec3Arg scale) const override
            {
                if (m_hull)
                {
                    m_hull->GetTrianglesStart(
                        context,
                        bounds,
                        centerOfMassPosition,
                        rotation,
                        scale);
                }
            }

            int GetTrianglesNext(
                GetTrianglesContext& context,
                const int maximumTriangleCount,
                JPH::Float3* triangleVertices,
                const JPH::PhysicsMaterial** materials) const override
            {
                if (!m_hull)
                {
                    return 0;
                }

                const int triangleCount = m_hull->GetTrianglesNext(
                    context,
                    maximumTriangleCount,
                    triangleVertices,
                    nullptr);
                if (materials)
                {
                    for (int triangleIndex = 0; triangleIndex < triangleCount; ++triangleIndex)
                    {
                        materials[triangleIndex] = GetMaterial();
                    }
                }
                return triangleCount;
            }

#if defined(JPH_DEBUG_RENDERER)
            void Draw(
                JPH::DebugRenderer* renderer,
                JPH::RMat44Arg centerOfMassTransform,
                const JPH::Vec3Arg scale,
                const JPH::ColorArg color,
                const bool useMaterialColors,
                const bool drawWireframe) const override
            {
                if (m_hull)
                {
                    JPH::Color drawColor = color;
                    if (useMaterialColors)
                    {
                        drawColor = GetMaterial()->GetDebugColor();
                    }
                    m_hull->Draw(
                        renderer,
                        centerOfMassTransform,
                        scale,
                        drawColor,
                        false,
                        drawWireframe);
                }
            }
#endif

            void SaveBinaryState(JPH::StreamOut& stream) const override
            {
                JPH::ConvexShape::SaveBinaryState(stream);
                stream.WriteBytes(&m_info.m_providerId, sizeof(m_info.m_providerId));
                stream.Write(m_info.m_providerVersion);
                stream.Write(m_info.m_sourceHash);
                m_hull->SaveBinaryState(stream);
            }

            Stats GetStats() const override
            {
                if (!m_hull)
                {
                    return {sizeof(*this), 0};
                }

                const Stats hullStats = m_hull->GetStats();
                return {sizeof(*this) + hullStats.mSizeBytes, hullStats.mNumTriangles};
            }

            float GetVolume() const override
            {
                if (m_hull)
                {
                    return m_hull->GetVolume();
                }

                return 0.0f;
            }

            bool IsValidScale(const JPH::Vec3Arg scale) const override
            {
                return m_hull && m_hull->IsValidScale(scale);
            }

            JPH::Vec3 MakeScaleValid(const JPH::Vec3Arg scale) const override
            {
                if (m_hull)
                {
                    return m_hull->MakeScaleValid(scale);
                }

                return JPH::Vec3::sOne();
            }

        protected:
            void RestoreBinaryState(JPH::StreamIn& stream) override
            {
                JPH::ConvexShape::RestoreBinaryState(stream);
                stream.ReadBytes(&m_info.m_providerId, sizeof(m_info.m_providerId));
                stream.Read(m_info.m_providerVersion);
                stream.Read(m_info.m_sourceHash);

                JPH::Shape::ShapeResult result = JPH::Shape::sRestoreFromBinaryState(stream);
                if (!result.HasError()
                    && result.Get()->GetSubType() == JPH::EShapeSubType::ConvexHull)
                {
                    m_hull = static_cast<const JPH::ConvexHullShape*>(result.Get().GetPtr());
                }
            }

        private:
            JPH::RefConst<JPH::ConvexHullShape> m_hull;
            CustomConvexShapeInfo m_info;
        };
    } // namespace

    void RegisterCustomConvexShapeType()
    {
        JPH::ShapeFunctions& functions = JPH::ShapeFunctions::sGet(JPH::EShapeSubType::UserConvex1);
        functions.mConstruct = []() -> JPH::Shape*
        {
            return new CustomConvexShape();
        };
        functions.mColor = JPH::Color::sOrange;
    }

    NativeShapeResult CreateNativeCustomConvexShape(
        const CustomConvexShapeData& data,
        const CustomConvexShapeInfo& info,
        const JPH::PhysicsMaterial* material,
        const float density)
    {
        if (data.m_points.size() < 4)
        {
            return {.m_error = "Custom convex geometry requires at least four points."};
        }

        JPH::Array<JPH::Vec3> points;
        points.reserve(data.m_points.size());
        for (const AZ::Vector3& point : data.m_points)
        {
            if (!point.IsFinite())
            {
                return {.m_error = "Custom convex geometry points must be finite."};
            }
            points.emplace_back(point.GetX(), point.GetY(), point.GetZ());
        }

        if (!AZ::IsFiniteFloat(data.m_hullTolerance)
            || data.m_hullTolerance <= 0.0f
            || !AZ::IsFiniteFloat(data.m_maximumConvexRadius)
            || data.m_maximumConvexRadius < 0.0f
            || !AZ::IsFiniteFloat(data.m_maximumConvexRadiusError)
            || data.m_maximumConvexRadiusError < 0.0f)
        {
            return {.m_error = "Custom convex hull settings are invalid."};
        }

        JPH::ConvexHullShapeSettings settings(AZStd::move(points), data.m_maximumConvexRadius, material);
        settings.mHullTolerance = data.m_hullTolerance;
        settings.mMaxErrorConvexRadius = data.m_maximumConvexRadiusError;
        settings.mDensity = density;
        JPH::Shape::ShapeResult hullResult = settings.Create();
        if (hullResult.HasError())
        {
            return {.m_error = hullResult.GetError().c_str()};
        }

        const auto* hull = static_cast<const JPH::ConvexHullShape*>(hullResult.Get().GetPtr());
        return {.m_shape = new CustomConvexShape(hull, info, material, density)};
    }

    bool GetNativeCustomConvexShapeInfo(
        const JPH::Shape& shape,
        CustomConvexShapeInfo& info)
    {
        if (shape.GetSubType() != JPH::EShapeSubType::UserConvex1)
        {
            return false;
        }

        const auto& customShape = static_cast<const CustomConvexShape&>(shape);
        if (!customShape.IsValid())
        {
            return false;
        }

        info = customShape.GetInfo();
        return true;
    }

    bool IsNativeCustomConvexShapeValid(const JPH::Shape& shape)
    {
        if (shape.GetSubType() != JPH::EShapeSubType::UserConvex1)
        {
            return true;
        }

        return static_cast<const CustomConvexShape&>(shape).IsValid();
    }
} // namespace Jolt
