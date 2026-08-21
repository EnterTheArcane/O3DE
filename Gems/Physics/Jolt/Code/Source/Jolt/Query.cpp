/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/Query.h>

#include <Jolt/BehaviorReflection.h>
#include <Jolt/Reflection.h>
#include <Jolt/TransformedShapeLease.h>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/TransformedShape.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/createdestroy.h>
#include <AzCore/std/utility/move.h>

namespace Jolt
{
    static_assert(sizeof(JPH::TransformedShape) <= 96);
    static_assert(alignof(JPH::TransformedShape) <= alignof(TransformedShape));
#ifdef JPH_DOUBLE_PRECISION
    static_assert(sizeof(TransformedShape) <= 192);
#else
    static_assert(sizeof(TransformedShape) <= 176);
#endif

    TransformedShape::TransformedShape()
    {
        AZStd::construct_at(reinterpret_cast<JPH::TransformedShape*>(m_nativeStorage));
    }

    TransformedShape::TransformedShape(const TransformedShape& other)
        : m_worldOrigin(other.m_worldOrigin)
        , m_worldHandle(other.m_worldHandle)
        , m_bodyHandle(other.m_bodyHandle)
        , m_materialHandle(other.m_materialHandle)
        , m_shapeHandle(other.m_shapeHandle)
        , m_userData(other.m_userData)
        , m_shapeKind(other.m_shapeKind)
        , m_leaseOwner(other.m_leaseOwner)
    {
        if (m_leaseOwner)
        {
            Internal::AcquireTransformedShapeLease(m_leaseOwner, m_shapeHandle);
        }
        AZStd::construct_at(
            reinterpret_cast<JPH::TransformedShape*>(m_nativeStorage),
            *reinterpret_cast<const JPH::TransformedShape*>(other.m_nativeStorage));
    }

    TransformedShape::TransformedShape(TransformedShape&& other) noexcept
        : m_worldOrigin(other.m_worldOrigin)
        , m_worldHandle(other.m_worldHandle)
        , m_bodyHandle(other.m_bodyHandle)
        , m_materialHandle(other.m_materialHandle)
        , m_shapeHandle(other.m_shapeHandle)
        , m_userData(other.m_userData)
        , m_shapeKind(other.m_shapeKind)
        , m_leaseOwner(other.m_leaseOwner)
    {
        AZStd::construct_at(
            reinterpret_cast<JPH::TransformedShape*>(m_nativeStorage),
            AZStd::move(*reinterpret_cast<JPH::TransformedShape*>(other.m_nativeStorage)));
        other.m_worldHandle = WorldHandle::Invalid;
        other.m_bodyHandle = BodyHandle::Invalid;
        other.m_materialHandle = MaterialHandle::Invalid;
        other.m_shapeHandle = ShapeHandle::Invalid;
        other.m_userData = 0;
        other.m_shapeKind = ShapeKind::None;
        other.m_leaseOwner = nullptr;
    }

    TransformedShape::~TransformedShape()
    {
        AZStd::destroy_at(reinterpret_cast<JPH::TransformedShape*>(m_nativeStorage));
        if (m_leaseOwner)
        {
            Internal::ReleaseTransformedShapeLease(m_leaseOwner, m_shapeHandle);
        }
    }

    TransformedShape& TransformedShape::operator=(const TransformedShape& other)
    {
        if (this != &other)
        {
            if (other.m_leaseOwner)
            {
                Internal::AcquireTransformedShapeLease(other.m_leaseOwner, other.m_shapeHandle);
            }
            *reinterpret_cast<JPH::TransformedShape*>(m_nativeStorage) =
                *reinterpret_cast<const JPH::TransformedShape*>(other.m_nativeStorage);
            if (m_leaseOwner)
            {
                Internal::ReleaseTransformedShapeLease(m_leaseOwner, m_shapeHandle);
            }
            m_worldOrigin = other.m_worldOrigin;
            m_worldHandle = other.m_worldHandle;
            m_bodyHandle = other.m_bodyHandle;
            m_materialHandle = other.m_materialHandle;
            m_shapeHandle = other.m_shapeHandle;
            m_userData = other.m_userData;
            m_shapeKind = other.m_shapeKind;
            m_leaseOwner = other.m_leaseOwner;
        }
        return *this;
    }

    TransformedShape& TransformedShape::operator=(TransformedShape&& other) noexcept
    {
        if (this != &other)
        {
            *reinterpret_cast<JPH::TransformedShape*>(m_nativeStorage) =
                AZStd::move(*reinterpret_cast<JPH::TransformedShape*>(other.m_nativeStorage));
            if (m_leaseOwner)
            {
                Internal::ReleaseTransformedShapeLease(m_leaseOwner, m_shapeHandle);
            }
            m_worldOrigin = other.m_worldOrigin;
            m_worldHandle = other.m_worldHandle;
            m_bodyHandle = other.m_bodyHandle;
            m_materialHandle = other.m_materialHandle;
            m_shapeHandle = other.m_shapeHandle;
            m_userData = other.m_userData;
            m_shapeKind = other.m_shapeKind;
            m_leaseOwner = other.m_leaseOwner;
            other.m_worldHandle = WorldHandle::Invalid;
            other.m_bodyHandle = BodyHandle::Invalid;
            other.m_materialHandle = MaterialHandle::Invalid;
            other.m_shapeHandle = ShapeHandle::Invalid;
            other.m_userData = 0;
            other.m_shapeKind = ShapeKind::None;
            other.m_leaseOwner = nullptr;
        }
        return *this;
    }

    TransformedShape::operator bool() const
    {
        return static_cast<bool>(
            reinterpret_cast<const JPH::TransformedShape*>(m_nativeStorage)->mShape);
    }

    BodyHandle TransformedShape::GetBodyHandle() const
    {
        return m_bodyHandle;
    }

    BroadPhaseAabb TransformedShape::GetBounds() const
    {
        const JPH::AABox bounds =
            reinterpret_cast<const JPH::TransformedShape*>(m_nativeStorage)->GetWorldSpaceBounds();
        const JPH::Vec3 center = bounds.GetCenter();
        return {
            .m_center = {
                .m_x = static_cast<double>(center.GetX()) + m_worldOrigin.m_x,
                .m_y = static_cast<double>(center.GetY()) + m_worldOrigin.m_y,
                .m_z = static_cast<double>(center.GetZ()) + m_worldOrigin.m_z,
            },
            .m_halfExtents = {
                bounds.GetExtent().GetX(),
                bounds.GetExtent().GetY(),
                bounds.GetExtent().GetZ(),
            },
        };
    }

    MaterialHandle TransformedShape::GetMaterialHandle() const
    {
        return m_materialHandle;
    }

    AZ::Vector3 TransformedShape::GetScale() const
    {
        const JPH::Vec3 scale =
            reinterpret_cast<const JPH::TransformedShape*>(m_nativeStorage)->GetShapeScale();
        return {scale.GetX(), scale.GetY(), scale.GetZ()};
    }

    ShapeHandle TransformedShape::GetShapeHandle() const
    {
        return m_shapeHandle;
    }

    ShapeKind TransformedShape::GetShapeKind() const
    {
        return m_shapeKind;
    }

    SubShapeId TransformedShape::GetSubShapeId() const
    {
        const JPH::SubShapeID subShapeId =
            reinterpret_cast<const JPH::TransformedShape*>(m_nativeStorage)->mSubShapeIDCreator.GetID();
        return SubShapeId(subShapeId.GetValue());
    }

    WorldTransform TransformedShape::GetTransform() const
    {
        const auto* nativeShape =
            reinterpret_cast<const JPH::TransformedShape*>(m_nativeStorage);
        const JPH::RVec3 translation = nativeShape->GetWorldTransform().GetTranslation();
        const JPH::Quat rotation = nativeShape->mShapeRotation;
        return {
            .m_position = {
                .m_x = translation.GetX() + m_worldOrigin.m_x,
                .m_y = translation.GetY() + m_worldOrigin.m_y,
                .m_z = translation.GetZ() + m_worldOrigin.m_z,
            },
            .m_rotation = AZ::Quaternion(
                rotation.GetX(),
                rotation.GetY(),
                rotation.GetZ(),
                rotation.GetW()),
        };
    }

    AZ::u64 TransformedShape::GetUserData() const
    {
        return m_userData;
    }

    WorldHandle TransformedShape::GetWorldHandle() const
    {
        return m_worldHandle;
    }

    AZStd::span<WorldPosition> ShapeQueryFaceBuffers::GetQueryFace(
        const AZ::u32 hitIndex) const
    {
        const size_t offset = static_cast<size_t>(hitIndex) * MaximumSupportingFaceVertexCount;
        if (offset >= m_queryVertices.size())
        {
            return {};
        }

        return m_queryVertices.subspan(
            offset,
            AZStd::min<size_t>(MaximumSupportingFaceVertexCount, m_queryVertices.size() - offset));
    }

    AZStd::span<WorldPosition> ShapeQueryFaceBuffers::GetTargetFace(
        const AZ::u32 hitIndex) const
    {
        const size_t offset = static_cast<size_t>(hitIndex) * MaximumSupportingFaceVertexCount;
        if (offset >= m_targetVertices.size())
        {
            return {};
        }

        return m_targetVertices.subspan(
            offset,
            AZStd::min<size_t>(MaximumSupportingFaceVertexCount, m_targetVertices.size() - offset));
    }

    void ReflectQueries(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<SubShapeId>();

            serializeContext
                ->Class<SubShapeTransform>()
                ->Field("CenterOfMassPosition", &SubShapeTransform::m_centerOfMassPosition)
                ->Field("Rotation", &SubShapeTransform::m_rotation)
                ->Field("Scale", &SubShapeTransform::m_scale)
                ->Field("Remainder", &SubShapeTransform::m_remainder);

            serializeContext
                ->Class<SimulationShape>()
                ->Field("BodyHandle", &SimulationShape::m_bodyHandle)
                ->Field("RootShapeHandle", &SimulationShape::m_rootShapeHandle)
                ->Field("SubShapeId", &SimulationShape::m_subShapeId)
                ->Field("Kind", &SimulationShape::m_kind);

            serializeContext
                ->Class<QueryFilter>()
                ->Field("CollisionLayer", &QueryFilter::m_collisionLayer);

            serializeContext
                ->Class<RaycastRequest>()
                ->Field("Start", &RaycastRequest::m_start)
                ->Field("Displacement", &RaycastRequest::m_displacement)
                ->Field("Filter", &RaycastRequest::m_filter)
                ->Field("ConvexBackFaceMode", &RaycastRequest::m_convexBackFaceMode)
                ->Field("TriangleBackFaceMode", &RaycastRequest::m_triangleBackFaceMode)
                ->Field("TreatConvexAsSolid", &RaycastRequest::m_treatConvexAsSolid);

            serializeContext
                ->Class<PointOverlapRequest>()
                ->Field("Position", &PointOverlapRequest::m_position)
                ->Field("Filter", &PointOverlapRequest::m_filter);

            serializeContext
                ->Class<ShapeRaycastRequest>()
                ->Field("ShapeHandle", &ShapeRaycastRequest::m_shapeHandle)
                ->Field("Start", &ShapeRaycastRequest::m_start)
                ->Field("Displacement", &ShapeRaycastRequest::m_displacement)
                ->Field("ConvexBackFaceMode", &ShapeRaycastRequest::m_convexBackFaceMode)
                ->Field("TriangleBackFaceMode", &ShapeRaycastRequest::m_triangleBackFaceMode)
                ->Field("TreatConvexAsSolid", &ShapeRaycastRequest::m_treatConvexAsSolid);

            serializeContext
                ->Class<TransformedShapeRaycastRequest>()
                ->Field("Start", &TransformedShapeRaycastRequest::m_start)
                ->Field("Displacement", &TransformedShapeRaycastRequest::m_displacement)
                ->Field("ConvexBackFaceMode", &TransformedShapeRaycastRequest::m_convexBackFaceMode)
                ->Field("TriangleBackFaceMode", &TransformedShapeRaycastRequest::m_triangleBackFaceMode)
                ->Field("TreatConvexAsSolid", &TransformedShapeRaycastRequest::m_treatConvexAsSolid);

            serializeContext
                ->Class<ShapePlacement>()
                ->Field("ShapeHandle", &ShapePlacement::m_shapeHandle)
                ->Field("Transform", &ShapePlacement::m_transform)
                ->Field("UniformScale", &ShapePlacement::m_uniformScale);

            serializeContext
                ->Class<ShapeOverlapRequest>()
                ->Field("ShapeHandle", &ShapeOverlapRequest::m_shapeHandle)
                ->Field("Transform", &ShapeOverlapRequest::m_transform)
                ->Field("Scale", &ShapeOverlapRequest::m_scale)
                ->Field("Filter", &ShapeOverlapRequest::m_filter)
                ->Field("ActiveEdgeMovementDirection", &ShapeOverlapRequest::m_activeEdgeMovementDirection)
                ->Field("BackFaceMode", &ShapeOverlapRequest::m_backFaceMode)
                ->Field("CollisionTolerance", &ShapeOverlapRequest::m_collisionTolerance)
                ->Field(
                    "InternalEdgeRemovalVertexTolerance",
                    &ShapeOverlapRequest::m_internalEdgeRemovalVertexTolerance)
                ->Field("MaximumSeparationDistance", &ShapeOverlapRequest::m_maximumSeparationDistance)
                ->Field("PenetrationTolerance", &ShapeOverlapRequest::m_penetrationTolerance)
                ->Field("ActiveEdgeMode", &ShapeOverlapRequest::m_activeEdgeMode)
                ->Field("FaceCollectionMode", &ShapeOverlapRequest::m_faceCollectionMode)
                ->Field("RemoveInternalEdges", &ShapeOverlapRequest::m_removeInternalEdges);

            serializeContext
                ->Class<ShapeCastRequest>()
                ->Field("ShapeHandle", &ShapeCastRequest::m_shapeHandle)
                ->Field("Start", &ShapeCastRequest::m_start)
                ->Field("Scale", &ShapeCastRequest::m_scale)
                ->Field("Displacement", &ShapeCastRequest::m_displacement)
                ->Field("Filter", &ShapeCastRequest::m_filter)
                ->Field("ActiveEdgeMovementDirection", &ShapeCastRequest::m_activeEdgeMovementDirection)
                ->Field("ConvexBackFaceMode", &ShapeCastRequest::m_convexBackFaceMode)
                ->Field("TriangleBackFaceMode", &ShapeCastRequest::m_triangleBackFaceMode)
                ->Field("CollisionTolerance", &ShapeCastRequest::m_collisionTolerance)
                ->Field("ExtraConvexRadius", &ShapeCastRequest::m_extraConvexRadius)
                ->Field("PenetrationTolerance", &ShapeCastRequest::m_penetrationTolerance)
                ->Field("ActiveEdgeMode", &ShapeCastRequest::m_activeEdgeMode)
                ->Field("FaceCollectionMode", &ShapeCastRequest::m_faceCollectionMode)
                ->Field("ReturnDeepestPoint", &ShapeCastRequest::m_returnDeepestPoint)
                ->Field("UseShrunkenShapeAndConvexRadius", &ShapeCastRequest::m_useShrunkenShapeAndConvexRadius);

            serializeContext
                ->Class<TransformedShapeCollisionRequest>()
                ->Field(
                    "ActiveEdgeMovementDirection",
                    &TransformedShapeCollisionRequest::m_activeEdgeMovementDirection)
                ->Field("CollisionTolerance", &TransformedShapeCollisionRequest::m_collisionTolerance)
                ->Field(
                    "InternalEdgeRemovalVertexTolerance",
                    &TransformedShapeCollisionRequest::m_internalEdgeRemovalVertexTolerance)
                ->Field(
                    "MaximumSeparationDistance",
                    &TransformedShapeCollisionRequest::m_maximumSeparationDistance)
                ->Field("PenetrationTolerance", &TransformedShapeCollisionRequest::m_penetrationTolerance)
                ->Field("BackFaceMode", &TransformedShapeCollisionRequest::m_backFaceMode)
                ->Field("ActiveEdgeMode", &TransformedShapeCollisionRequest::m_activeEdgeMode)
                ->Field("FaceCollectionMode", &TransformedShapeCollisionRequest::m_faceCollectionMode)
                ->Field("RemoveInternalEdges", &TransformedShapeCollisionRequest::m_removeInternalEdges);

            serializeContext
                ->Class<TransformedShapeCastRequest>()
                ->Field("Displacement", &TransformedShapeCastRequest::m_displacement)
                ->Field(
                    "ActiveEdgeMovementDirection",
                    &TransformedShapeCastRequest::m_activeEdgeMovementDirection)
                ->Field("CollisionTolerance", &TransformedShapeCastRequest::m_collisionTolerance)
                ->Field("ExtraConvexRadius", &TransformedShapeCastRequest::m_extraConvexRadius)
                ->Field("MaximumFraction", &TransformedShapeCastRequest::m_maximumFraction)
                ->Field("PenetrationTolerance", &TransformedShapeCastRequest::m_penetrationTolerance)
                ->Field("ConvexBackFaceMode", &TransformedShapeCastRequest::m_convexBackFaceMode)
                ->Field("TriangleBackFaceMode", &TransformedShapeCastRequest::m_triangleBackFaceMode)
                ->Field("ActiveEdgeMode", &TransformedShapeCastRequest::m_activeEdgeMode)
                ->Field("FaceCollectionMode", &TransformedShapeCastRequest::m_faceCollectionMode)
                ->Field("ReturnDeepestPoint", &TransformedShapeCastRequest::m_returnDeepestPoint)
                ->Field(
                    "UseShrunkenShapeAndConvexRadius",
                    &TransformedShapeCastRequest::m_useShrunkenShapeAndConvexRadius);

            serializeContext
                ->Class<BroadPhaseAabb>()
                ->Field("Center", &BroadPhaseAabb::m_center)
                ->Field("HalfExtents", &BroadPhaseAabb::m_halfExtents);

            serializeContext
                ->Class<BroadPhaseOrientedBox>()
                ->Field("Transform", &BroadPhaseOrientedBox::m_transform)
                ->Field("HalfExtents", &BroadPhaseOrientedBox::m_halfExtents);

            serializeContext
                ->Class<ShapeCollectionRequest>()
                ->Field("Bounds", &ShapeCollectionRequest::m_bounds)
                ->Field("Filter", &ShapeCollectionRequest::m_filter);

            serializeContext
                ->Class<SupportingFaceRequest>()
                ->Field("BodyHandle", &SupportingFaceRequest::m_bodyHandle)
                ->Field("SubShapeId", &SupportingFaceRequest::m_subShapeId)
                ->Field("IncidentDirection", &SupportingFaceRequest::m_incidentDirection);

            serializeContext
                ->Class<TriangleCollectionRequest>()
                ->Field("BodyHandle", &TriangleCollectionRequest::m_bodyHandle)
                ->Field("Bounds", &TriangleCollectionRequest::m_bounds);

            serializeContext
                ->Class<ShapeTriangleCollectionRequest>()
                ->Field("ShapeHandle", &ShapeTriangleCollectionRequest::m_shapeHandle)
                ->Field("Transform", &ShapeTriangleCollectionRequest::m_transform)
                ->Field("BoundsCenter", &ShapeTriangleCollectionRequest::m_boundsCenter)
                ->Field("BoundsHalfExtents", &ShapeTriangleCollectionRequest::m_boundsHalfExtents);

            serializeContext
                ->Class<BroadPhasePoint>()
                ->Field("Position", &BroadPhasePoint::m_position);

            serializeContext
                ->Class<BroadPhaseSphere>()
                ->Field("Center", &BroadPhaseSphere::m_center)
                ->Field("Radius", &BroadPhaseSphere::m_radius);

            serializeContext
                ->Class<BroadPhaseOverlapRequest>()
                ->Field("Geometry", &BroadPhaseOverlapRequest::m_geometry)
                ->Field("Filter", &BroadPhaseOverlapRequest::m_filter);

            serializeContext
                ->Class<BroadPhaseAabbCast>()
                ->Field("Start", &BroadPhaseAabbCast::m_start)
                ->Field("Displacement", &BroadPhaseAabbCast::m_displacement);

            serializeContext
                ->Class<BroadPhaseRay>()
                ->Field("Start", &BroadPhaseRay::m_start)
                ->Field("Displacement", &BroadPhaseRay::m_displacement);

            serializeContext
                ->Class<BroadPhaseCastRequest>()
                ->Field("Geometry", &BroadPhaseCastRequest::m_geometry)
                ->Field("Filter", &BroadPhaseCastRequest::m_filter);

            serializeContext
                ->Class<RaycastHit>()
                ->Field("Position", &RaycastHit::m_position)
                ->Field("Normal", &RaycastHit::m_normal)
                ->Field("BodyHandle", &RaycastHit::m_bodyHandle)
                ->Field("MaterialHandle", &RaycastHit::m_materialHandle)
                ->Field("ShapeHandle", &RaycastHit::m_shapeHandle)
                ->Field("SubShapeId", &RaycastHit::m_subShapeId)
                ->Field("Fraction", &RaycastHit::m_fraction);

            serializeContext
                ->Class<ShapeRaycastHit>()
                ->Field("Position", &ShapeRaycastHit::m_position)
                ->Field("Normal", &ShapeRaycastHit::m_normal)
                ->Field("MaterialHandle", &ShapeRaycastHit::m_materialHandle)
                ->Field("SubShapeId", &ShapeRaycastHit::m_subShapeId)
                ->Field("Fraction", &ShapeRaycastHit::m_fraction);

            serializeContext
                ->Class<ShapePointHit>()
                ->Field("MaterialHandle", &ShapePointHit::m_materialHandle)
                ->Field("SubShapeId", &ShapePointHit::m_subShapeId);

            serializeContext
                ->Class<OverlapHit>()
                ->Field("BodyHandle", &OverlapHit::m_bodyHandle)
                ->Field("MaterialHandle", &OverlapHit::m_materialHandle)
                ->Field("ShapeHandle", &OverlapHit::m_shapeHandle)
                ->Field("SubShapeId", &OverlapHit::m_subShapeId);

            serializeContext
                ->Class<ShapeOverlapHit>()
                ->Field("QueryContactPosition", &ShapeOverlapHit::m_queryContactPosition)
                ->Field("TargetContactPosition", &ShapeOverlapHit::m_targetContactPosition)
                ->Field("PenetrationAxis", &ShapeOverlapHit::m_penetrationAxis)
                ->Field("BodyHandle", &ShapeOverlapHit::m_bodyHandle)
                ->Field("MaterialHandle", &ShapeOverlapHit::m_materialHandle)
                ->Field("ShapeHandle", &ShapeOverlapHit::m_shapeHandle)
                ->Field("QuerySubShapeId", &ShapeOverlapHit::m_querySubShapeId)
                ->Field("TargetSubShapeId", &ShapeOverlapHit::m_targetSubShapeId)
                ->Field("PenetrationDepth", &ShapeOverlapHit::m_penetrationDepth)
                ->Field("QueryFaceVertexCount", &ShapeOverlapHit::m_queryFaceVertexCount)
                ->Field("TargetFaceVertexCount", &ShapeOverlapHit::m_targetFaceVertexCount);

            serializeContext
                ->Class<ShapeCastHit>()
                ->Field("QueryContactPosition", &ShapeCastHit::m_queryContactPosition)
                ->Field("TargetContactPosition", &ShapeCastHit::m_targetContactPosition)
                ->Field("PenetrationAxis", &ShapeCastHit::m_penetrationAxis)
                ->Field("BodyHandle", &ShapeCastHit::m_bodyHandle)
                ->Field("MaterialHandle", &ShapeCastHit::m_materialHandle)
                ->Field("ShapeHandle", &ShapeCastHit::m_shapeHandle)
                ->Field("QuerySubShapeId", &ShapeCastHit::m_querySubShapeId)
                ->Field("TargetSubShapeId", &ShapeCastHit::m_targetSubShapeId)
                ->Field("Fraction", &ShapeCastHit::m_fraction)
                ->Field("PenetrationDepth", &ShapeCastHit::m_penetrationDepth)
                ->Field("QueryFaceVertexCount", &ShapeCastHit::m_queryFaceVertexCount)
                ->Field("TargetFaceVertexCount", &ShapeCastHit::m_targetFaceVertexCount)
                ->Field("IsBackFaceHit", &ShapeCastHit::m_isBackFaceHit);

            serializeContext
                ->Class<TransformedShapeCollisionHit>()
                ->Field("FirstContactPosition", &TransformedShapeCollisionHit::m_firstContactPosition)
                ->Field("SecondContactPosition", &TransformedShapeCollisionHit::m_secondContactPosition)
                ->Field("PenetrationAxis", &TransformedShapeCollisionHit::m_penetrationAxis)
                ->Field("FirstWorldHandle", &TransformedShapeCollisionHit::m_firstWorldHandle)
                ->Field("SecondWorldHandle", &TransformedShapeCollisionHit::m_secondWorldHandle)
                ->Field("FirstBodyHandle", &TransformedShapeCollisionHit::m_firstBodyHandle)
                ->Field("SecondBodyHandle", &TransformedShapeCollisionHit::m_secondBodyHandle)
                ->Field("FirstMaterialHandle", &TransformedShapeCollisionHit::m_firstMaterialHandle)
                ->Field("SecondMaterialHandle", &TransformedShapeCollisionHit::m_secondMaterialHandle)
                ->Field("FirstShapeHandle", &TransformedShapeCollisionHit::m_firstShapeHandle)
                ->Field("SecondShapeHandle", &TransformedShapeCollisionHit::m_secondShapeHandle)
                ->Field("FirstSubShapeId", &TransformedShapeCollisionHit::m_firstSubShapeId)
                ->Field("SecondSubShapeId", &TransformedShapeCollisionHit::m_secondSubShapeId)
                ->Field("FirstUserData", &TransformedShapeCollisionHit::m_firstUserData)
                ->Field("SecondUserData", &TransformedShapeCollisionHit::m_secondUserData)
                ->Field("PenetrationDepth", &TransformedShapeCollisionHit::m_penetrationDepth)
                ->Field("FirstFaceVertexCount", &TransformedShapeCollisionHit::m_firstFaceVertexCount)
                ->Field("SecondFaceVertexCount", &TransformedShapeCollisionHit::m_secondFaceVertexCount);

            serializeContext
                ->Class<TransformedShapeCastHit>()
                ->Field("Collision", &TransformedShapeCastHit::m_collision)
                ->Field("Fraction", &TransformedShapeCastHit::m_fraction)
                ->Field("IsBackFaceHit", &TransformedShapeCastHit::m_isBackFaceHit);

            serializeContext
                ->Class<BroadPhaseHit>()
                ->Field("BodyHandle", &BroadPhaseHit::m_bodyHandle);

            serializeContext
                ->Class<BroadPhaseCastHit>()
                ->Field("BodyHandle", &BroadPhaseCastHit::m_bodyHandle)
                ->Field("Fraction", &BroadPhaseCastHit::m_fraction);

            serializeContext
                ->Class<TransformedTriangle>()
                ->Field("FirstVertex", &TransformedTriangle::m_firstVertex)
                ->Field("SecondVertex", &TransformedTriangle::m_secondVertex)
                ->Field("ThirdVertex", &TransformedTriangle::m_thirdVertex)
                ->Field("MaterialHandle", &TransformedTriangle::m_materialHandle);

            serializeContext
                ->Class<ShapeTriangle>()
                ->Field("FirstVertex", &ShapeTriangle::m_firstVertex)
                ->Field("SecondVertex", &ShapeTriangle::m_secondVertex)
                ->Field("ThirdVertex", &ShapeTriangle::m_thirdVertex)
                ->Field("MaterialHandle", &ShapeTriangle::m_materialHandle);

            serializeContext
                ->Class<QueryResult>()
                ->Field("HitCount", &QueryResult::m_hitCount)
                ->Field("RequiredHitCount", &QueryResult::m_requiredHitCount);

            serializeContext
                ->Class<BufferResult>()
                ->Field("Count", &BufferResult::m_count)
                ->Field("RequiredCount", &BufferResult::m_requiredCount);
        }

        auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (!behaviorContext)
        {
            return;
        }

        JOLT_BEHAVIOR_ENUM(*behaviorContext, BackFaceMode, Ignore);
        JOLT_BEHAVIOR_ENUM(*behaviorContext, BackFaceMode, Collide);

        JOLT_BEHAVIOR_ENUM(*behaviorContext, ActiveEdgeMode, None);
        JOLT_BEHAVIOR_ENUM(*behaviorContext, ActiveEdgeMode, CollideOnlyWithActive);
        JOLT_BEHAVIOR_ENUM(*behaviorContext, ActiveEdgeMode, CollideWithAll);

        JOLT_BEHAVIOR_ENUM(*behaviorContext, FaceCollectionMode, None);
        JOLT_BEHAVIOR_ENUM(*behaviorContext, FaceCollectionMode, Collect);

        JOLT_BEHAVIOR_ENUM(*behaviorContext, ShapeKind, None);
        JOLT_BEHAVIOR_ENUM(*behaviorContext, ShapeKind, Box);
        JOLT_BEHAVIOR_ENUM(*behaviorContext, ShapeKind, Capsule);
        JOLT_BEHAVIOR_ENUM(*behaviorContext, ShapeKind, ConvexHull);
        JOLT_BEHAVIOR_ENUM(*behaviorContext, ShapeKind, Custom);
        JOLT_BEHAVIOR_ENUM(*behaviorContext, ShapeKind, CustomConvex);
        JOLT_BEHAVIOR_ENUM(*behaviorContext, ShapeKind, Cylinder);
        JOLT_BEHAVIOR_ENUM(*behaviorContext, ShapeKind, Empty);
        JOLT_BEHAVIOR_ENUM(*behaviorContext, ShapeKind, Heightfield);
        JOLT_BEHAVIOR_ENUM(*behaviorContext, ShapeKind, Mesh);
        JOLT_BEHAVIOR_ENUM(*behaviorContext, ShapeKind, MutableCompound);
        JOLT_BEHAVIOR_ENUM(*behaviorContext, ShapeKind, OffsetCenterOfMass);
        JOLT_BEHAVIOR_ENUM(*behaviorContext, ShapeKind, Plane);
        JOLT_BEHAVIOR_ENUM(*behaviorContext, ShapeKind, RotatedTranslated);
        JOLT_BEHAVIOR_ENUM(*behaviorContext, ShapeKind, Scaled);
        JOLT_BEHAVIOR_ENUM(*behaviorContext, ShapeKind, SoftBody);
        JOLT_BEHAVIOR_ENUM(*behaviorContext, ShapeKind, Sphere);
        JOLT_BEHAVIOR_ENUM(*behaviorContext, ShapeKind, StaticCompound);
        JOLT_BEHAVIOR_ENUM(*behaviorContext, ShapeKind, TaperedCapsule);
        JOLT_BEHAVIOR_ENUM(*behaviorContext, ShapeKind, TaperedCylinder);
        JOLT_BEHAVIOR_ENUM(*behaviorContext, ShapeKind, Triangle);

        if (ShouldReflect(
            *behaviorContext,
            behaviorContext->m_classes.contains("SubShapeId")))
        {
            behaviorContext->Class<SubShapeId>("SubShapeId")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Method("GetValue", &SubShapeId::GetValue);
        }

        behaviorContext->Class<SubShapeTransform>("SubShapeTransform")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Property(
                "centerOfMassPosition",
                BehaviorValueGetter(&SubShapeTransform::m_centerOfMassPosition),
                nullptr)
            ->Property("rotation", BehaviorValueGetter(&SubShapeTransform::m_rotation), nullptr)
            ->Property("scale", BehaviorValueGetter(&SubShapeTransform::m_scale), nullptr)
            ->Property("remainder", BehaviorValueGetter(&SubShapeTransform::m_remainder), nullptr);

        behaviorContext->Class<SimulationShape>("SimulationShape")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Property("bodyHandle", BehaviorValueGetter(&SimulationShape::m_bodyHandle), nullptr)
            ->Property("rootShapeHandle", BehaviorValueGetter(&SimulationShape::m_rootShapeHandle), nullptr)
            ->Property("subShapeId", BehaviorValueGetter(&SimulationShape::m_subShapeId), nullptr)
            ->Property("kind", BehaviorValueGetter(&SimulationShape::m_kind), nullptr);

        behaviorContext->Class<QueryFilter>("JoltQueryFilter")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Attribute(AZ::Script::Attributes::Alias, "QueryFilter")
            ->Attribute(AZ::Script::Attributes::ClassNameOverride, "QueryFilter")
            ->Constructor<>()
            ->Property(
                "collisionLayer",
                [](const QueryFilter* filter)
                {
                    return filter->m_collisionLayer;
                },
                [](QueryFilter* filter, const ObjectLayer collisionLayer)
                {
                    filter->m_collisionLayer = collisionLayer;
                });

        behaviorContext->Class<RaycastRequest>("JoltRaycastRequest")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Attribute(AZ::Script::Attributes::Alias, "RaycastRequest")
            ->Attribute(AZ::Script::Attributes::ClassNameOverride, "RaycastRequest")
            ->Constructor<>()
            ->Property("start", JOLT_BEHAVIOR_VALUE_PROPERTY(&RaycastRequest::m_start))
            ->Property("displacement", JOLT_BEHAVIOR_VALUE_PROPERTY(&RaycastRequest::m_displacement))
            ->Property("filter", JOLT_BEHAVIOR_VALUE_PROPERTY(&RaycastRequest::m_filter))
            ->Property("convexBackFaceMode", JOLT_BEHAVIOR_VALUE_PROPERTY(&RaycastRequest::m_convexBackFaceMode))
            ->Property("triangleBackFaceMode", JOLT_BEHAVIOR_VALUE_PROPERTY(&RaycastRequest::m_triangleBackFaceMode))
            ->Property("treatConvexAsSolid", JOLT_BEHAVIOR_VALUE_PROPERTY(&RaycastRequest::m_treatConvexAsSolid));

        behaviorContext->Class<PointOverlapRequest>("PointOverlapRequest")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Constructor<>()
            ->Property("position", JOLT_BEHAVIOR_VALUE_PROPERTY(&PointOverlapRequest::m_position))
            ->Property("filter", JOLT_BEHAVIOR_VALUE_PROPERTY(&PointOverlapRequest::m_filter));

        behaviorContext->Class<ShapeRaycastRequest>("ShapeRaycastRequest")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Constructor<>()
            ->Property("shapeHandle", JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeRaycastRequest::m_shapeHandle))
            ->Property("start", JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeRaycastRequest::m_start))
            ->Property("displacement", JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeRaycastRequest::m_displacement))
            ->Property(
                "convexBackFaceMode",
                JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeRaycastRequest::m_convexBackFaceMode))
            ->Property(
                "triangleBackFaceMode",
                JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeRaycastRequest::m_triangleBackFaceMode))
            ->Property(
                "treatConvexAsSolid",
                JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeRaycastRequest::m_treatConvexAsSolid));

        behaviorContext->Class<ShapeOverlapRequest>("ShapeOverlapRequest")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Constructor<>()
            ->Property("shapeHandle", JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeOverlapRequest::m_shapeHandle))
            ->Property("transform", JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeOverlapRequest::m_transform))
            ->Property("scale", JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeOverlapRequest::m_scale))
            ->Property("filter", JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeOverlapRequest::m_filter))
            ->Property(
                "activeEdgeMovementDirection",
                JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeOverlapRequest::m_activeEdgeMovementDirection))
            ->Property("backFaceMode", JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeOverlapRequest::m_backFaceMode))
            ->Property("collisionTolerance", JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeOverlapRequest::m_collisionTolerance))
            ->Property(
                "internalEdgeRemovalVertexTolerance",
                JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeOverlapRequest::m_internalEdgeRemovalVertexTolerance))
            ->Property(
                "maximumSeparationDistance",
                JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeOverlapRequest::m_maximumSeparationDistance))
            ->Property(
                "penetrationTolerance",
                JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeOverlapRequest::m_penetrationTolerance))
            ->Property("activeEdgeMode", JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeOverlapRequest::m_activeEdgeMode))
            ->Property("faceCollectionMode", JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeOverlapRequest::m_faceCollectionMode))
            ->Property(
                "removeInternalEdges",
                JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeOverlapRequest::m_removeInternalEdges));

        behaviorContext->Class<ShapeCastRequest>("JoltShapeCastRequest")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Attribute(AZ::Script::Attributes::Alias, "ShapeCastRequest")
            ->Attribute(AZ::Script::Attributes::ClassNameOverride, "ShapeCastRequest")
            ->Constructor<>()
            ->Property("shapeHandle", JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeCastRequest::m_shapeHandle))
            ->Property("start", JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeCastRequest::m_start))
            ->Property("scale", JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeCastRequest::m_scale))
            ->Property("displacement", JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeCastRequest::m_displacement))
            ->Property("filter", JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeCastRequest::m_filter))
            ->Property(
                "activeEdgeMovementDirection",
                JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeCastRequest::m_activeEdgeMovementDirection))
            ->Property("convexBackFaceMode", JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeCastRequest::m_convexBackFaceMode))
            ->Property("triangleBackFaceMode", JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeCastRequest::m_triangleBackFaceMode))
            ->Property("collisionTolerance", JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeCastRequest::m_collisionTolerance))
            ->Property("extraConvexRadius", JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeCastRequest::m_extraConvexRadius))
            ->Property("penetrationTolerance", JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeCastRequest::m_penetrationTolerance))
            ->Property("activeEdgeMode", JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeCastRequest::m_activeEdgeMode))
            ->Property("faceCollectionMode", JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeCastRequest::m_faceCollectionMode))
            ->Property("returnDeepestPoint", JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeCastRequest::m_returnDeepestPoint))
            ->Property(
                "useShrunkenShapeAndConvexRadius",
                JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeCastRequest::m_useShrunkenShapeAndConvexRadius));

        behaviorContext->Class<ShapeTriangleCollectionRequest>("ShapeTriangleCollectionRequest")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Constructor<>()
            ->Property(
                "shapeHandle",
                JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeTriangleCollectionRequest::m_shapeHandle))
            ->Property(
                "transform",
                JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeTriangleCollectionRequest::m_transform))
            ->Property(
                "boundsCenter",
                JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeTriangleCollectionRequest::m_boundsCenter))
            ->Property(
                "boundsHalfExtents",
                JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeTriangleCollectionRequest::m_boundsHalfExtents));

        behaviorContext->Class<BroadPhaseAabb>("BroadPhaseAabb")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Constructor<>()
            ->Property("center", JOLT_BEHAVIOR_VALUE_PROPERTY(&BroadPhaseAabb::m_center))
            ->Property("halfExtents", JOLT_BEHAVIOR_VALUE_PROPERTY(&BroadPhaseAabb::m_halfExtents));

        behaviorContext->Class<BroadPhaseOrientedBox>("BroadPhaseOrientedBox")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Constructor<>()
            ->Property("transform", JOLT_BEHAVIOR_VALUE_PROPERTY(&BroadPhaseOrientedBox::m_transform))
            ->Property("halfExtents", JOLT_BEHAVIOR_VALUE_PROPERTY(&BroadPhaseOrientedBox::m_halfExtents));

        behaviorContext->Class<ShapeCollectionRequest>("ShapeCollectionRequest")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Constructor<>()
            ->Property("bounds", JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeCollectionRequest::m_bounds))
            ->Property("filter", JOLT_BEHAVIOR_VALUE_PROPERTY(&ShapeCollectionRequest::m_filter));

        behaviorContext->Class<SupportingFaceRequest>("SupportingFaceRequest")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Constructor<>()
            ->Property("bodyHandle", JOLT_BEHAVIOR_VALUE_PROPERTY(&SupportingFaceRequest::m_bodyHandle))
            ->Property("subShapeId", JOLT_BEHAVIOR_VALUE_PROPERTY(&SupportingFaceRequest::m_subShapeId))
            ->Property(
                "incidentDirection",
                JOLT_BEHAVIOR_VALUE_PROPERTY(&SupportingFaceRequest::m_incidentDirection));

        behaviorContext->Class<TriangleCollectionRequest>("TriangleCollectionRequest")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Constructor<>()
            ->Property("bodyHandle", JOLT_BEHAVIOR_VALUE_PROPERTY(&TriangleCollectionRequest::m_bodyHandle))
            ->Property("bounds", JOLT_BEHAVIOR_VALUE_PROPERTY(&TriangleCollectionRequest::m_bounds));

        behaviorContext->Class<BroadPhasePoint>("BroadPhasePoint")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Constructor<>()
            ->Property("position", JOLT_BEHAVIOR_VALUE_PROPERTY(&BroadPhasePoint::m_position));

        behaviorContext->Class<BroadPhaseSphere>("BroadPhaseSphere")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Constructor<>()
            ->Property("center", JOLT_BEHAVIOR_VALUE_PROPERTY(&BroadPhaseSphere::m_center))
            ->Property("radius", JOLT_BEHAVIOR_VALUE_PROPERTY(&BroadPhaseSphere::m_radius));

        behaviorContext->Class<BroadPhaseOverlapRequest>("BroadPhaseOverlapRequest")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Constructor<>()
            ->Property("filter", JOLT_BEHAVIOR_VALUE_PROPERTY(&BroadPhaseOverlapRequest::m_filter))
            ->Method(
                "SetAabb",
                [](BroadPhaseOverlapRequest* request, BroadPhaseAabb geometry)
                {
                    request->m_geometry = AZStd::move(geometry);
                })
            ->Method(
                "SetOrientedBox",
                [](BroadPhaseOverlapRequest* request, BroadPhaseOrientedBox geometry)
                {
                    request->m_geometry = AZStd::move(geometry);
                })
            ->Method(
                "SetPoint",
                [](BroadPhaseOverlapRequest* request, BroadPhasePoint geometry)
                {
                    request->m_geometry = AZStd::move(geometry);
                })
            ->Method(
                "SetSphere",
                [](BroadPhaseOverlapRequest* request, BroadPhaseSphere geometry)
                {
                    request->m_geometry = AZStd::move(geometry);
                });

        behaviorContext->Class<BroadPhaseAabbCast>("BroadPhaseAabbCast")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Constructor<>()
            ->Property("start", JOLT_BEHAVIOR_VALUE_PROPERTY(&BroadPhaseAabbCast::m_start))
            ->Property("displacement", JOLT_BEHAVIOR_VALUE_PROPERTY(&BroadPhaseAabbCast::m_displacement));

        behaviorContext->Class<BroadPhaseRay>("BroadPhaseRay")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Constructor<>()
            ->Property("start", JOLT_BEHAVIOR_VALUE_PROPERTY(&BroadPhaseRay::m_start))
            ->Property("displacement", JOLT_BEHAVIOR_VALUE_PROPERTY(&BroadPhaseRay::m_displacement));

        behaviorContext->Class<BroadPhaseCastRequest>("BroadPhaseCastRequest")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Constructor<>()
            ->Property("filter", JOLT_BEHAVIOR_VALUE_PROPERTY(&BroadPhaseCastRequest::m_filter))
            ->Method(
                "SetAabbCast",
                [](BroadPhaseCastRequest* request, BroadPhaseAabbCast geometry)
                {
                    request->m_geometry = AZStd::move(geometry);
                })
            ->Method(
                "SetRay",
                [](BroadPhaseCastRequest* request, BroadPhaseRay geometry)
                {
                    request->m_geometry = AZStd::move(geometry);
                });

        behaviorContext->Class<RaycastHit>("RaycastHit")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Property("position", BehaviorValueGetter(&RaycastHit::m_position), nullptr)
            ->Property("normal", BehaviorValueGetter(&RaycastHit::m_normal), nullptr)
            ->Property("bodyHandle", BehaviorValueGetter(&RaycastHit::m_bodyHandle), nullptr)
            ->Property("materialHandle", BehaviorValueGetter(&RaycastHit::m_materialHandle), nullptr)
            ->Property("shapeHandle", BehaviorValueGetter(&RaycastHit::m_shapeHandle), nullptr)
            ->Property("subShapeId", BehaviorValueGetter(&RaycastHit::m_subShapeId), nullptr)
            ->Property("fraction", BehaviorValueGetter(&RaycastHit::m_fraction), nullptr);

        behaviorContext->Class<ShapeRaycastHit>("ShapeRaycastHit")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Property("position", BehaviorValueGetter(&ShapeRaycastHit::m_position), nullptr)
            ->Property("normal", BehaviorValueGetter(&ShapeRaycastHit::m_normal), nullptr)
            ->Property("materialHandle", BehaviorValueGetter(&ShapeRaycastHit::m_materialHandle), nullptr)
            ->Property("subShapeId", BehaviorValueGetter(&ShapeRaycastHit::m_subShapeId), nullptr)
            ->Property("fraction", BehaviorValueGetter(&ShapeRaycastHit::m_fraction), nullptr);

        behaviorContext->Class<ShapePointHit>("ShapePointHit")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Property("materialHandle", BehaviorValueGetter(&ShapePointHit::m_materialHandle), nullptr)
            ->Property("subShapeId", BehaviorValueGetter(&ShapePointHit::m_subShapeId), nullptr);

        behaviorContext->Class<ShapeTriangle>("ShapeTriangle")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Property("firstVertex", BehaviorValueGetter(&ShapeTriangle::m_firstVertex), nullptr)
            ->Property("secondVertex", BehaviorValueGetter(&ShapeTriangle::m_secondVertex), nullptr)
            ->Property("thirdVertex", BehaviorValueGetter(&ShapeTriangle::m_thirdVertex), nullptr)
            ->Property("materialHandle", BehaviorValueGetter(&ShapeTriangle::m_materialHandle), nullptr);

        behaviorContext->Class<OverlapHit>("JoltOverlapHit")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Attribute(AZ::Script::Attributes::Alias, "OverlapHit")
            ->Attribute(AZ::Script::Attributes::ClassNameOverride, "OverlapHit")
            ->Property("bodyHandle", BehaviorValueGetter(&OverlapHit::m_bodyHandle), nullptr)
            ->Property("materialHandle", BehaviorValueGetter(&OverlapHit::m_materialHandle), nullptr)
            ->Property("shapeHandle", BehaviorValueGetter(&OverlapHit::m_shapeHandle), nullptr)
            ->Property("subShapeId", BehaviorValueGetter(&OverlapHit::m_subShapeId), nullptr);

        behaviorContext->Class<ShapeOverlapHit>("ShapeOverlapHit")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Property("queryContactPosition", BehaviorValueGetter(&ShapeOverlapHit::m_queryContactPosition), nullptr)
            ->Property("targetContactPosition", BehaviorValueGetter(&ShapeOverlapHit::m_targetContactPosition), nullptr)
            ->Property("penetrationAxis", BehaviorValueGetter(&ShapeOverlapHit::m_penetrationAxis), nullptr)
            ->Property("bodyHandle", BehaviorValueGetter(&ShapeOverlapHit::m_bodyHandle), nullptr)
            ->Property("materialHandle", BehaviorValueGetter(&ShapeOverlapHit::m_materialHandle), nullptr)
            ->Property("shapeHandle", BehaviorValueGetter(&ShapeOverlapHit::m_shapeHandle), nullptr)
            ->Property("querySubShapeId", BehaviorValueGetter(&ShapeOverlapHit::m_querySubShapeId), nullptr)
            ->Property("targetSubShapeId", BehaviorValueGetter(&ShapeOverlapHit::m_targetSubShapeId), nullptr)
            ->Property("penetrationDepth", BehaviorValueGetter(&ShapeOverlapHit::m_penetrationDepth), nullptr)
            ->Property("queryFaceVertexCount", BehaviorValueGetter(&ShapeOverlapHit::m_queryFaceVertexCount), nullptr)
            ->Property(
                "targetFaceVertexCount",
                BehaviorValueGetter(&ShapeOverlapHit::m_targetFaceVertexCount),
                nullptr);

        behaviorContext->Class<ShapeCastHit>("ShapeCastHit")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Property("queryContactPosition", BehaviorValueGetter(&ShapeCastHit::m_queryContactPosition), nullptr)
            ->Property("targetContactPosition", BehaviorValueGetter(&ShapeCastHit::m_targetContactPosition), nullptr)
            ->Property("penetrationAxis", BehaviorValueGetter(&ShapeCastHit::m_penetrationAxis), nullptr)
            ->Property("bodyHandle", BehaviorValueGetter(&ShapeCastHit::m_bodyHandle), nullptr)
            ->Property("materialHandle", BehaviorValueGetter(&ShapeCastHit::m_materialHandle), nullptr)
            ->Property("shapeHandle", BehaviorValueGetter(&ShapeCastHit::m_shapeHandle), nullptr)
            ->Property("querySubShapeId", BehaviorValueGetter(&ShapeCastHit::m_querySubShapeId), nullptr)
            ->Property("targetSubShapeId", BehaviorValueGetter(&ShapeCastHit::m_targetSubShapeId), nullptr)
            ->Property("fraction", BehaviorValueGetter(&ShapeCastHit::m_fraction), nullptr)
            ->Property("penetrationDepth", BehaviorValueGetter(&ShapeCastHit::m_penetrationDepth), nullptr)
            ->Property("queryFaceVertexCount", BehaviorValueGetter(&ShapeCastHit::m_queryFaceVertexCount), nullptr)
            ->Property("targetFaceVertexCount", BehaviorValueGetter(&ShapeCastHit::m_targetFaceVertexCount), nullptr)
            ->Property("isBackFaceHit", BehaviorValueGetter(&ShapeCastHit::m_isBackFaceHit), nullptr);

        behaviorContext->Class<BroadPhaseHit>("BroadPhaseHit")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Property("bodyHandle", BehaviorValueGetter(&BroadPhaseHit::m_bodyHandle), nullptr);

        behaviorContext->Class<BroadPhaseCastHit>("BroadPhaseCastHit")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Property("bodyHandle", BehaviorValueGetter(&BroadPhaseCastHit::m_bodyHandle), nullptr)
            ->Property("fraction", BehaviorValueGetter(&BroadPhaseCastHit::m_fraction), nullptr);

        behaviorContext->Class<TransformedTriangle>("TransformedTriangle")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Property("firstVertex", BehaviorValueGetter(&TransformedTriangle::m_firstVertex), nullptr)
            ->Property("secondVertex", BehaviorValueGetter(&TransformedTriangle::m_secondVertex), nullptr)
            ->Property("thirdVertex", BehaviorValueGetter(&TransformedTriangle::m_thirdVertex), nullptr)
            ->Property("materialHandle", BehaviorValueGetter(&TransformedTriangle::m_materialHandle), nullptr);

        behaviorContext->Class<QueryResult>("QueryResult")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Property("hitCount", BehaviorValueGetter(&QueryResult::m_hitCount), nullptr)
            ->Property("requiredHitCount", BehaviorValueGetter(&QueryResult::m_requiredHitCount), nullptr)
            ->Method("IsComplete", &QueryResult::IsComplete);

        behaviorContext->Class<BufferResult>("BufferResult")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Property("count", BehaviorValueGetter(&BufferResult::m_count), nullptr)
            ->Property("requiredCount", BehaviorValueGetter(&BufferResult::m_requiredCount), nullptr)
            ->Method("IsComplete", &BufferResult::IsComplete)
            ->Method("HasOverflow", &BufferResult::HasOverflow);
    }
} // namespace Jolt
