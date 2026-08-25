/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <Jolt/Handle.h>
#include <Jolt/Collision.h>
#include <Jolt/TypeIds.h>
#include <Jolt/WorldTypes.h>

#include <AzCore/Math/Quaternion.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/base.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/containers/variant.h>
#include <AzCore/std/limits.h>

namespace AZ
{
    class ReflectContext;
} // namespace AZ

namespace Jolt
{
    JOLT_API void ReflectQueries(AZ::ReflectContext* context);

    inline constexpr AZ::u32 MaximumSupportingFaceVertexCount = 32;

    enum class BackFaceMode : AZ::u8
    {
        Ignore = 0,
        Collide,
    };

    enum class ActiveEdgeMode : AZ::u8
    {
        None = 0,
        CollideOnlyWithActive,
        CollideWithAll,
    };

    enum class FaceCollectionMode : AZ::u8
    {
        None = 0,
        Collect,
    };

    class SubShapeId final
    {
    public:
        using ValueType = AZ::u32;

        static const SubShapeId Root;

        constexpr SubShapeId() noexcept = default;

        explicit constexpr SubShapeId(
            const ValueType value) noexcept
            : m_value(value)
        {
        }

        AZ_TYPE_INFO(SubShapeId, SubShapeIdTypeId);

        [[nodiscard]]
        constexpr ValueType GetValue() const noexcept
        {
            return m_value;
        }

        friend constexpr bool operator==(SubShapeId, SubShapeId) noexcept = default;

    private:
        ValueType m_value = AZStd::numeric_limits<ValueType>::max();
    };

    inline constexpr SubShapeId SubShapeId::Root{};

    struct SubShapeTransform final
    {
        AZ_TYPE_INFO(SubShapeTransform, SubShapeTransformTypeId);

        AZ::Vector3 m_centerOfMassPosition = AZ::Vector3::CreateZero();
        AZ::Quaternion m_rotation = AZ::Quaternion::CreateIdentity();
        AZ::Vector3 m_scale = AZ::Vector3::CreateOne();
        SubShapeId m_remainder;
    };

    enum class ShapeKind : AZ::u8
    {
        None = 0,
        Box,
        Capsule,
        ConvexHull,
        Custom,
        CustomConvex,
        Cylinder,
        Empty,
        Heightfield,
        Mesh,
        MutableCompound,
        OffsetCenterOfMass,
        Plane,
        RotatedTranslated,
        Scaled,
        SoftBody,
        Sphere,
        StaticCompound,
        TaperedCapsule,
        TaperedCylinder,
        Triangle,
    };

    struct SimulationShape final
    {
        AZ_TYPE_INFO(SimulationShape, SimulationShapeTypeId);

        BodyHandle m_bodyHandle;
        ShapeHandle m_rootShapeHandle;
        SubShapeId m_subShapeId;
        ShapeKind m_kind = ShapeKind::None;
    };

    class IQueryFilter
    {
    public:
        virtual ~IQueryFilter() = default;

        //! Called synchronously while the target body is locked. Do not call runtime capabilities.

        [[nodiscard]]
        virtual bool ShouldIncludeBody([[maybe_unused]] BodyHandle bodyHandle) const
        {
            return true;
        }

        [[nodiscard]]
        virtual bool ShouldIncludeShape([[maybe_unused]] const SimulationShape& shape) const
        {
            return true;
        }

        [[nodiscard]]
        virtual bool ShouldIncludeShapePair(
            [[maybe_unused]] const SimulationShape& queryShape,
            const SimulationShape& targetShape) const
        {
            return ShouldIncludeShape(targetShape);
        }
    };

    struct QueryFilter final
    {
        AZ_TYPE_INFO(QueryFilter, QueryFilterTypeId);

        const IQueryFilter* m_callback = nullptr;
        ObjectLayer m_collisionLayer;
    };

    struct RaycastRequest final
    {
        AZ_TYPE_INFO(RaycastRequest, RaycastRequestTypeId);

        WorldPosition m_start;
        AZ::Vector3 m_displacement = AZ::Vector3::CreateZero();
        QueryFilter m_filter;
        BackFaceMode m_convexBackFaceMode = BackFaceMode::Ignore;
        BackFaceMode m_triangleBackFaceMode = BackFaceMode::Collide;
        bool m_treatConvexAsSolid = true;
    };

    struct PointOverlapRequest final
    {
        AZ_TYPE_INFO(PointOverlapRequest, PointOverlapRequestTypeId);

        WorldPosition m_position;
        QueryFilter m_filter;
    };

    //! Casts in the shape's authored local space. The start is adjusted for its native center of mass internally.
    struct ShapeRaycastRequest final
    {
        AZ_TYPE_INFO(ShapeRaycastRequest, ShapeRaycastRequestTypeId);

        ShapeHandle m_shapeHandle;
        AZ::Vector3 m_start = AZ::Vector3::CreateZero();
        AZ::Vector3 m_displacement = AZ::Vector3::CreateZero();
        const IQueryFilter* m_filter = nullptr;
        BackFaceMode m_convexBackFaceMode = BackFaceMode::Ignore;
        BackFaceMode m_triangleBackFaceMode = BackFaceMode::Collide;
        bool m_treatConvexAsSolid = true;
    };

    struct TransformedShapeRaycastRequest final
    {
        AZ_TYPE_INFO(TransformedShapeRaycastRequest, TransformedShapeRaycastRequestTypeId);

        WorldPosition m_start;
        AZ::Vector3 m_displacement = AZ::Vector3::CreateZero();
        const IQueryFilter* m_filter = nullptr;
        BackFaceMode m_convexBackFaceMode = BackFaceMode::Ignore;
        BackFaceMode m_triangleBackFaceMode = BackFaceMode::Collide;
        bool m_treatConvexAsSolid = true;
    };

    //! Places one retained world shape for an immediate exact pair query.
    struct ShapePlacement final
    {
        AZ_TYPE_INFO(ShapePlacement, ShapePlacementTypeId);

        ShapeHandle m_shapeHandle;
        WorldTransform m_transform;
        float m_uniformScale = 1.0f;
    };

    struct ShapeOverlapRequest final
    {
        AZ_TYPE_INFO(ShapeOverlapRequest, ShapeOverlapRequestTypeId);

        ShapeHandle m_shapeHandle;
        WorldTransform m_transform;
        AZ::Vector3 m_scale = AZ::Vector3::CreateOne();
        QueryFilter m_filter;
        AZ::Vector3 m_activeEdgeMovementDirection = AZ::Vector3::CreateZero();
        BackFaceMode m_backFaceMode = BackFaceMode::Ignore;
        float m_collisionTolerance = 1.0e-4f;
        float m_internalEdgeRemovalVertexTolerance = 1.0e-4f;
        float m_maximumSeparationDistance = 0.0f;
        float m_penetrationTolerance = 1.0e-4f;
        ActiveEdgeMode m_activeEdgeMode = ActiveEdgeMode::CollideOnlyWithActive;
        FaceCollectionMode m_faceCollectionMode = FaceCollectionMode::None;
        bool m_removeInternalEdges = false;
    };

    struct ShapeCastRequest final
    {
        AZ_TYPE_INFO(ShapeCastRequest, ShapeCastRequestTypeId);

        ShapeHandle m_shapeHandle;
        WorldTransform m_start;
        AZ::Vector3 m_scale = AZ::Vector3::CreateOne();
        AZ::Vector3 m_displacement = AZ::Vector3::CreateZero();
        QueryFilter m_filter;
        AZ::Vector3 m_activeEdgeMovementDirection = AZ::Vector3::CreateZero();
        BackFaceMode m_convexBackFaceMode = BackFaceMode::Ignore;
        BackFaceMode m_triangleBackFaceMode = BackFaceMode::Ignore;
        float m_collisionTolerance = 1.0e-4f;
        float m_extraConvexRadius = 0.0f;
        float m_penetrationTolerance = 1.0e-4f;
        ActiveEdgeMode m_activeEdgeMode = ActiveEdgeMode::CollideOnlyWithActive;
        FaceCollectionMode m_faceCollectionMode = FaceCollectionMode::None;
        bool m_returnDeepestPoint = false;
        bool m_useShrunkenShapeAndConvexRadius = false;
    };

    //! Controls an exact collision query between two retained transformed shapes.
    struct TransformedShapeCollisionRequest final
    {
        AZ_TYPE_INFO(TransformedShapeCollisionRequest, TransformedShapeCollisionRequestTypeId);

        AZ::Vector3 m_activeEdgeMovementDirection = AZ::Vector3::CreateZero();
        const IQueryFilter* m_filter = nullptr;

        float m_collisionTolerance = 1.0e-4f;
        float m_internalEdgeRemovalVertexTolerance = 1.0e-4f;
        float m_maximumSeparationDistance = 0.0f;
        float m_penetrationTolerance = 1.0e-4f;

        BackFaceMode m_backFaceMode = BackFaceMode::Ignore;
        ActiveEdgeMode m_activeEdgeMode = ActiveEdgeMode::CollideOnlyWithActive;
        FaceCollectionMode m_faceCollectionMode = FaceCollectionMode::None;
        bool m_removeInternalEdges = false;
    };

    //! Controls an exact cast of the first retained shape against the second.
    struct TransformedShapeCastRequest final
    {
        AZ_TYPE_INFO(TransformedShapeCastRequest, TransformedShapeCastRequestTypeId);

        AZ::Vector3 m_displacement = AZ::Vector3::CreateZero();
        AZ::Vector3 m_activeEdgeMovementDirection = AZ::Vector3::CreateZero();
        const IQueryFilter* m_filter = nullptr;

        float m_collisionTolerance = 1.0e-4f;
        float m_extraConvexRadius = 0.0f;
        float m_maximumFraction = 1.0f;
        float m_penetrationTolerance = 1.0e-4f;

        BackFaceMode m_convexBackFaceMode = BackFaceMode::Ignore;
        BackFaceMode m_triangleBackFaceMode = BackFaceMode::Ignore;
        ActiveEdgeMode m_activeEdgeMode = ActiveEdgeMode::CollideOnlyWithActive;
        FaceCollectionMode m_faceCollectionMode = FaceCollectionMode::None;
        bool m_returnDeepestPoint = false;
        bool m_useShrunkenShapeAndConvexRadius = false;
    };

    struct BroadPhaseAabb final
    {
        AZ_TYPE_INFO(BroadPhaseAabb, BroadPhaseAabbTypeId);

        WorldPosition m_center;
        AZ::Vector3 m_halfExtents = AZ::Vector3::CreateZero();
    };

    struct BroadPhaseOrientedBox final
    {
        AZ_TYPE_INFO(BroadPhaseOrientedBox, BroadPhaseOrientedBoxTypeId);

        WorldTransform m_transform;
        AZ::Vector3 m_halfExtents = AZ::Vector3::CreateZero();
    };

    struct ShapeCollectionRequest final
    {
        AZ_TYPE_INFO(ShapeCollectionRequest, ShapeCollectionRequestTypeId);

        BroadPhaseAabb m_bounds;
        QueryFilter m_filter;
    };

    struct SupportingFaceRequest final
    {
        AZ_TYPE_INFO(SupportingFaceRequest, SupportingFaceRequestTypeId);

        BodyHandle m_bodyHandle;
        SubShapeId m_subShapeId;
        AZ::Vector3 m_incidentDirection = AZ::Vector3::CreateAxisZ();
    };

    struct TriangleCollectionRequest final
    {
        AZ_TYPE_INFO(TriangleCollectionRequest, TriangleCollectionRequestTypeId);

        BodyHandle m_bodyHandle;
        BroadPhaseAabb m_bounds;
    };

    struct ShapeTriangleCollectionRequest final
    {
        AZ_TYPE_INFO(ShapeTriangleCollectionRequest, ShapeTriangleCollectionRequestTypeId);

        ShapeHandle m_shapeHandle;
        AZ::Transform m_transform = AZ::Transform::CreateIdentity();
        AZ::Vector3 m_boundsCenter = AZ::Vector3::CreateZero();
        AZ::Vector3 m_boundsHalfExtents = AZ::Vector3::CreateZero();
    };

    struct BroadPhasePoint final
    {
        AZ_TYPE_INFO(BroadPhasePoint, BroadPhasePointTypeId);

        WorldPosition m_position;
    };

    struct BroadPhaseSphere final
    {
        AZ_TYPE_INFO(BroadPhaseSphere, BroadPhaseSphereTypeId);

        WorldPosition m_center;
        float m_radius = 0.0f;
    };

    using BroadPhaseOverlapGeometry = AZStd::variant<
        BroadPhaseAabb,
        BroadPhaseOrientedBox,
        BroadPhasePoint,
        BroadPhaseSphere>;

    struct BroadPhaseOverlapRequest final
    {
        AZ_TYPE_INFO(BroadPhaseOverlapRequest, BroadPhaseOverlapRequestTypeId);

        BroadPhaseOverlapGeometry m_geometry;
        QueryFilter m_filter;
    };

    struct BroadPhaseAabbCast final
    {
        AZ_TYPE_INFO(BroadPhaseAabbCast, BroadPhaseAabbCastTypeId);

        BroadPhaseAabb m_start;
        AZ::Vector3 m_displacement = AZ::Vector3::CreateZero();
    };

    struct BroadPhaseRay final
    {
        AZ_TYPE_INFO(BroadPhaseRay, BroadPhaseRayTypeId);

        WorldPosition m_start;
        AZ::Vector3 m_displacement = AZ::Vector3::CreateZero();
    };

    using BroadPhaseCastGeometry = AZStd::variant<
        BroadPhaseAabbCast,
        BroadPhaseRay>;

    struct BroadPhaseCastRequest final
    {
        AZ_TYPE_INFO(BroadPhaseCastRequest, BroadPhaseCastRequestTypeId);

        BroadPhaseCastGeometry m_geometry;
        QueryFilter m_filter;
    };

    struct RaycastHit final
    {
        AZ_TYPE_INFO(RaycastHit, RaycastHitTypeId);

        WorldPosition m_position;
        AZ::Vector3 m_normal = AZ::Vector3::CreateZero();
        BodyHandle m_bodyHandle;
        MaterialHandle m_materialHandle;
        ShapeHandle m_shapeHandle;
        SubShapeId m_subShapeId;
        float m_fraction = 0.0f;
    };

    struct ShapeRaycastHit final
    {
        AZ_TYPE_INFO(ShapeRaycastHit, ShapeRaycastHitTypeId);

        AZ::Vector3 m_position = AZ::Vector3::CreateZero();
        AZ::Vector3 m_normal = AZ::Vector3::CreateZero();
        MaterialHandle m_materialHandle;
        SubShapeId m_subShapeId;
        float m_fraction = 0.0f;
    };

    struct ShapePointHit final
    {
        AZ_TYPE_INFO(ShapePointHit, ShapePointHitTypeId);

        MaterialHandle m_materialHandle;
        SubShapeId m_subShapeId;
    };

    struct ClosestRaycastResult final
    {
        AZ_TYPE_INFO(ClosestRaycastResult, ClosestRaycastResultTypeId);

        RaycastHit m_hit;
        bool m_found = false;
    };

    struct OverlapHit final
    {
        AZ_TYPE_INFO(OverlapHit, OverlapHitTypeId);

        BodyHandle m_bodyHandle;
        MaterialHandle m_materialHandle;
        ShapeHandle m_shapeHandle;
        SubShapeId m_subShapeId;
    };

    struct ShapeOverlapHit final
    {
        AZ_TYPE_INFO(ShapeOverlapHit, ShapeOverlapHitTypeId);

        WorldPosition m_queryContactPosition;
        WorldPosition m_targetContactPosition;
        AZ::Vector3 m_penetrationAxis = AZ::Vector3::CreateZero();
        BodyHandle m_bodyHandle;
        MaterialHandle m_materialHandle;
        ShapeHandle m_shapeHandle;
        SubShapeId m_querySubShapeId;
        SubShapeId m_targetSubShapeId;
        float m_penetrationDepth = 0.0f;
        AZ::u8 m_queryFaceVertexCount = 0;
        AZ::u8 m_targetFaceVertexCount = 0;
    };

    struct ShapeCastHit final
    {
        AZ_TYPE_INFO(ShapeCastHit, ShapeCastHitTypeId);

        WorldPosition m_queryContactPosition;
        WorldPosition m_targetContactPosition;
        AZ::Vector3 m_penetrationAxis = AZ::Vector3::CreateZero();
        BodyHandle m_bodyHandle;
        MaterialHandle m_materialHandle;
        ShapeHandle m_shapeHandle;
        SubShapeId m_querySubShapeId;
        SubShapeId m_targetSubShapeId;
        float m_fraction = 0.0f;
        float m_penetrationDepth = 0.0f;
        AZ::u8 m_queryFaceVertexCount = 0;
        AZ::u8 m_targetFaceVertexCount = 0;
        bool m_isBackFaceHit = false;
    };

    //! Exact collision result whose identities remain meaningful after either source body is destroyed.
    struct TransformedShapeCollisionHit final
    {
        AZ_TYPE_INFO(TransformedShapeCollisionHit, TransformedShapeCollisionHitTypeId);

        WorldPosition m_firstContactPosition;
        WorldPosition m_secondContactPosition;
        AZ::Vector3 m_penetrationAxis = AZ::Vector3::CreateZero();

        WorldHandle m_firstWorldHandle;
        WorldHandle m_secondWorldHandle;
        BodyHandle m_firstBodyHandle;
        BodyHandle m_secondBodyHandle;
        MaterialHandle m_firstMaterialHandle;
        MaterialHandle m_secondMaterialHandle;
        ShapeHandle m_firstShapeHandle;
        ShapeHandle m_secondShapeHandle;
        SubShapeId m_firstSubShapeId;
        SubShapeId m_secondSubShapeId;

        AZ::u64 m_firstUserData = 0;
        AZ::u64 m_secondUserData = 0;
        float m_penetrationDepth = 0.0f;
        AZ::u8 m_firstFaceVertexCount = 0;
        AZ::u8 m_secondFaceVertexCount = 0;
    };

    struct TransformedShapeCastHit final
    {
        AZ_TYPE_INFO(TransformedShapeCastHit, TransformedShapeCastHitTypeId);

        TransformedShapeCollisionHit m_collision;
        float m_fraction = 0.0f;
        bool m_isBackFaceHit = false;
    };

    //! Synchronous collision sink. Face spans are valid only during AddHit.
    //! Callbacks follow deterministic native traversal order, must not reenter runtime capabilities, and may be concurrent across caller threads.
    //! GetEarlyOutFraction may only stay unchanged or decrease after a hit. Use the span overload for canonical result ordering.
    class ITransformedShapeCollisionCollector
    {
    public:
        AZ_RTTI(ITransformedShapeCollisionCollector, ITransformedShapeCollisionCollectorTypeId);

        virtual ~ITransformedShapeCollisionCollector() = default;

        [[nodiscard]]
        virtual float GetEarlyOutFraction() const = 0;

        //! Return false to stop traversal immediately.
        [[nodiscard]]
        virtual bool AddHit(
            const TransformedShapeCollisionHit& hit,
            AZStd::span<const WorldPosition> firstFace,
            AZStd::span<const WorldPosition> secondFace) = 0;
    };

    //! Synchronous cast sink. Face spans are valid only during AddHit.
    //! Callbacks follow deterministic native traversal order, must not reenter runtime capabilities, and may be concurrent across caller threads.
    //! GetEarlyOutFraction may only stay unchanged or decrease after a hit. Use the span overload for canonical result ordering.
    class ITransformedShapeCastCollector
    {
    public:
        AZ_RTTI(ITransformedShapeCastCollector, ITransformedShapeCastCollectorTypeId);

        virtual ~ITransformedShapeCastCollector() = default;

        [[nodiscard]]
        virtual float GetEarlyOutFraction() const = 0;

        //! Return false to stop traversal immediately.
        [[nodiscard]]
        virtual bool AddHit(
            const TransformedShapeCastHit& hit,
            AZStd::span<const WorldPosition> firstFace,
            AZStd::span<const WorldPosition> secondFace) = 0;
    };

    //! Each hit owns a fixed 32-vertex partition in both spans. Faces are written only when requested.
    struct ShapeQueryFaceBuffers final
    {
        AZStd::span<WorldPosition> m_queryVertices;
        AZStd::span<WorldPosition> m_targetVertices;

        [[nodiscard]]
        JOLT_API AZStd::span<WorldPosition> GetQueryFace(AZ::u32 hitIndex) const;

        [[nodiscard]]
        JOLT_API AZStd::span<WorldPosition> GetTargetFace(AZ::u32 hitIndex) const;
    };

    struct BroadPhaseHit final
    {
        AZ_TYPE_INFO(BroadPhaseHit, BroadPhaseHitTypeId);

        BodyHandle m_bodyHandle;
    };

    struct BroadPhaseCastHit final
    {
        AZ_TYPE_INFO(BroadPhaseCastHit, BroadPhaseCastHitTypeId);

        BodyHandle m_bodyHandle;
        float m_fraction = 0.0f;
    };

    //! Immutable shape lease. Explicit shape and world destruction fail while a lease exists.
    //! Forced system shutdown invalidates world queries, but outstanding lease values remain safe to destroy.
    class JOLT_API TransformedShape final
    {
    public:
        AZ_TYPE_INFO(TransformedShape, TransformedShapeTypeId);

        TransformedShape();
        TransformedShape(const TransformedShape& other);
        TransformedShape(TransformedShape&& other) noexcept;
        ~TransformedShape();

        TransformedShape& operator=(const TransformedShape& other);

        TransformedShape& operator=(TransformedShape&& other) noexcept;

        [[nodiscard]]
        explicit operator bool() const;

        [[nodiscard]]
        BodyHandle GetBodyHandle() const;

        [[nodiscard]]
        BroadPhaseAabb GetBounds() const;

        [[nodiscard]]
        MaterialHandle GetMaterialHandle() const;

        [[nodiscard]]
        AZ::Vector3 GetScale() const;

        [[nodiscard]]
        ShapeHandle GetShapeHandle() const;

        [[nodiscard]]
        ShapeKind GetShapeKind() const;

        [[nodiscard]]
        SubShapeId GetSubShapeId() const;

        [[nodiscard]]
        WorldTransform GetTransform() const;

        [[nodiscard]]
        AZ::u64 GetUserData() const;

        [[nodiscard]]
        WorldHandle GetWorldHandle() const;

    private:
        friend class World;

        static constexpr size_t NativeStorageSize = 96;

#if JOLT_DOUBLE_PRECISION
        alignas(32) AZ::u8 m_nativeStorage[NativeStorageSize]{};
#else
        alignas(16) AZ::u8 m_nativeStorage[NativeStorageSize]{};
#endif
        WorldPosition m_worldOrigin;

        WorldHandle m_worldHandle;
        BodyHandle m_bodyHandle;
        MaterialHandle m_materialHandle;
        ShapeHandle m_shapeHandle;

        AZ::u64 m_userData = 0;
        ShapeKind m_shapeKind = ShapeKind::None;
        void* m_leaseOwner = nullptr;
    };

    struct TransformedTriangle final
    {
        AZ_TYPE_INFO(TransformedTriangle, TransformedTriangleTypeId);

        WorldPosition m_firstVertex;
        WorldPosition m_secondVertex;
        WorldPosition m_thirdVertex;
        MaterialHandle m_materialHandle;
    };

    struct ShapeTriangle final
    {
        AZ_TYPE_INFO(ShapeTriangle, ShapeTriangleTypeId);

        AZ::Vector3 m_firstVertex = AZ::Vector3::CreateZero();
        AZ::Vector3 m_secondVertex = AZ::Vector3::CreateZero();
        AZ::Vector3 m_thirdVertex = AZ::Vector3::CreateZero();
        MaterialHandle m_materialHandle;
    };

    struct QueryResult final
    {
        AZ_TYPE_INFO(QueryResult, QueryResultTypeId);

        AZ::u32 m_hitCount = 0;
        AZ::u32 m_requiredHitCount = 0;

        [[nodiscard]]
        constexpr bool IsComplete() const
        {
            return m_hitCount == m_requiredHitCount;
        }
    };

    struct BufferResult final
    {
        AZ_TYPE_INFO(BufferResult, BufferResultTypeId);

        AZ::u32 m_count = 0;
        AZ::u32 m_requiredCount = 0;

        [[nodiscard]]
        constexpr bool IsComplete() const
        {
            return m_count == m_requiredCount;
        }

        [[nodiscard]]
        constexpr bool HasOverflow() const
        {
            return m_count < m_requiredCount;
        }
    };

    //! Borrowed hot-path query view. The pointer remains valid until its world is destroyed.
    //! Independent geometric queries may run concurrently with each other and with simulation.
    //! Topology changes wait for in-flight queries; callbacks must not attempt world mutation.
    //! Cast hits are ordered by distance or penetration, then by stable body and subshape identity.
    //! Closest casts use the same ordering to resolve exact ties.
    class IWorldQueries
    {
    public:
        AZ_RTTI(IWorldQueries, IWorldQueriesTypeId);

        virtual ~IWorldQueries() = default;

        [[nodiscard]]
        virtual bool RaycastShapeClosest(
            const ShapeRaycastRequest& request,
            ShapeRaycastHit& hit) const = 0;

        [[nodiscard]]
        virtual QueryResult RaycastShapeAll(
            const ShapeRaycastRequest& request,
            AZStd::span<ShapeRaycastHit> hits) const = 0;

        [[nodiscard]]
        virtual QueryResult CollideShapePoint(
            ShapeHandle shapeHandle,
            const AZ::Vector3& localPosition,
            const IQueryFilter* filter,
            AZStd::span<ShapePointHit> hits) const = 0;

        [[nodiscard]]
        virtual bool CollideShapePointAny(
            ShapeHandle shapeHandle,
            const AZ::Vector3& localPosition,
            const IQueryFilter* filter = nullptr) const = 0;

        [[nodiscard]]
        virtual QueryResult CollectShapeTriangles(
            const ShapeTriangleCollectionRequest& request,
            AZStd::span<ShapeTriangle> triangles) const = 0;

        [[nodiscard]]
        virtual bool RaycastTransformedShapeClosest(
            const TransformedShape& shape,
            const TransformedShapeRaycastRequest& request,
            RaycastHit& hit) const = 0;

        [[nodiscard]]
        virtual QueryResult RaycastTransformedShapeAll(
            const TransformedShape& shape,
            const TransformedShapeRaycastRequest& request,
            AZStd::span<RaycastHit> hits) const = 0;

        [[nodiscard]]
        virtual QueryResult CollideTransformedShapePoint(
            const TransformedShape& shape,
            const WorldPosition& position,
            const IQueryFilter* filter,
            AZStd::span<OverlapHit> hits) const = 0;

        [[nodiscard]]
        virtual bool CollideTransformedShapePointAny(
            const TransformedShape& shape,
            const WorldPosition& position,
            const IQueryFilter* filter = nullptr) const = 0;

        [[nodiscard]]
        virtual QueryResult CollectTransformedShapeChildren(
            const TransformedShape& shape,
            const BroadPhaseAabb& bounds,
            const IQueryFilter* filter,
            AZStd::span<TransformedShape> children) const = 0;

        [[nodiscard]]
        virtual QueryResult CollectTransformedShapeTriangles(
            const TransformedShape& shape,
            const BroadPhaseAabb& bounds,
            AZStd::span<TransformedTriangle> triangles) const = 0;

        [[nodiscard]]
        virtual bool GetTransformedShapeSurfaceNormal(
            const TransformedShape& shape,
            SubShapeId subShapeId,
            const WorldPosition& position,
            AZ::Vector3& normal) const = 0;

        [[nodiscard]]
        virtual QueryResult GetTransformedShapeSupportingFace(
            const TransformedShape& shape,
            SubShapeId subShapeId,
            const AZ::Vector3& direction,
            AZStd::span<WorldPosition> vertices) const = 0;

        //! Retains an immutable shape and its provider/material dependencies independently of a body.
        [[nodiscard]]
        virtual bool RetainShape(
            ShapeHandle shapeHandle,
            const WorldTransform& transform,
            float uniformScale,
            TransformedShape& shape) const = 0;

        [[nodiscard]]
        virtual QueryResult CollideTransformedShapes(
            const TransformedShape& firstShape,
            const TransformedShape& secondShape,
            const TransformedShapeCollisionRequest& request,
            AZStd::span<TransformedShapeCollisionHit> hits,
            const ShapeQueryFaceBuffers& faceBuffers = {}) const = 0;

        [[nodiscard]]
        virtual bool CollideTransformedShapes(
            const TransformedShape& firstShape,
            const TransformedShape& secondShape,
            const TransformedShapeCollisionRequest& request,
            ITransformedShapeCollisionCollector& collector) const = 0;

        [[nodiscard]]
        virtual QueryResult CastTransformedShape(
            const TransformedShape& firstShape,
            const TransformedShape& secondShape,
            const TransformedShapeCastRequest& request,
            AZStd::span<TransformedShapeCastHit> hits,
            const ShapeQueryFaceBuffers& faceBuffers = {}) const = 0;

        [[nodiscard]]
        virtual bool CastTransformedShape(
            const TransformedShape& firstShape,
            const TransformedShape& secondShape,
            const TransformedShapeCastRequest& request,
            ITransformedShapeCastCollector& collector) const = 0;

        [[nodiscard]]
        virtual bool RaycastClosest(
            const RaycastRequest& request,
            RaycastHit& hit) const = 0;

        [[nodiscard]]
        virtual BufferResult RaycastClosestBatch(
            AZStd::span<const RaycastRequest> requests,
            AZStd::span<ClosestRaycastResult> results) const = 0;

        [[nodiscard]]
        virtual QueryResult RaycastClosestPerBody(
            const RaycastRequest& request,
            AZStd::span<RaycastHit> hits) const = 0;

        [[nodiscard]]
        virtual bool RaycastAny(const RaycastRequest& request) const = 0;

        [[nodiscard]]
        virtual QueryResult RaycastAll(
            const RaycastRequest& request,
            AZStd::span<RaycastHit> hits) const = 0;

        [[nodiscard]]
        virtual QueryResult OverlapPoint(
            const PointOverlapRequest& request,
            AZStd::span<OverlapHit> hits) const = 0;

        [[nodiscard]]
        virtual bool OverlapPointAny(const PointOverlapRequest& request) const = 0;

        [[nodiscard]]
        virtual QueryResult CollideShape(
            const ShapeOverlapRequest& request,
            AZStd::span<ShapeOverlapHit> hits,
            const ShapeQueryFaceBuffers& faceBuffers = {}) const = 0;

        [[nodiscard]]
        virtual QueryResult OverlapShape(
            const ShapeOverlapRequest& request,
            AZStd::span<OverlapHit> hits) const = 0;

        [[nodiscard]]
        virtual bool OverlapShapeAny(const ShapeOverlapRequest& request) const = 0;

        [[nodiscard]]
        virtual bool CastShapeClosest(
            const ShapeCastRequest& request,
            ShapeCastHit& hit,
            const ShapeQueryFaceBuffers& faceBuffers = {}) const = 0;

        [[nodiscard]]
        virtual QueryResult CastShapeClosestPerBody(
            const ShapeCastRequest& request,
            AZStd::span<ShapeCastHit> hits,
            const ShapeQueryFaceBuffers& faceBuffers = {}) const = 0;

        [[nodiscard]]
        virtual QueryResult CastShapeAll(
            const ShapeCastRequest& request,
            AZStd::span<ShapeCastHit> hits,
            const ShapeQueryFaceBuffers& faceBuffers = {}) const = 0;

        [[nodiscard]]
        virtual QueryResult OverlapBroadPhase(
            const BroadPhaseOverlapRequest& request,
            AZStd::span<BroadPhaseHit> hits) const = 0;

        [[nodiscard]]
        virtual bool OverlapBroadPhaseAny(const BroadPhaseOverlapRequest& request) const = 0;

        [[nodiscard]]
        virtual bool CastBroadPhaseClosest(
            const BroadPhaseCastRequest& request,
            BroadPhaseCastHit& hit) const = 0;

        [[nodiscard]]
        virtual QueryResult CastBroadPhaseAll(
            const BroadPhaseCastRequest& request,
            AZStd::span<BroadPhaseCastHit> hits) const = 0;

        [[nodiscard]]
        virtual QueryResult CollectShapesInBounds(
            const ShapeCollectionRequest& request,
            AZStd::span<TransformedShape> shapes) const = 0;

        [[nodiscard]]
        virtual QueryResult GetSupportingFace(
            const SupportingFaceRequest& request,
            AZStd::span<WorldPosition> vertices) const = 0;

        [[nodiscard]]
        virtual QueryResult CollectTriangles(
            const TriangleCollectionRequest& request,
            AZStd::span<TransformedTriangle> triangles) const = 0;

        [[nodiscard]]
        virtual bool GetBroadPhaseBounds(BroadPhaseAabb& bounds) const = 0;

        [[nodiscard]]
        virtual bool WereBodiesInContact(
            BodyHandle firstBodyHandle,
            BodyHandle secondBodyHandle) const = 0;
    };
} // namespace Jolt

AZ_TYPE_INFO_SPECIALIZE(Jolt::BackFaceMode, "{5529630C-B250-44B1-9FC9-5C8CDF446904}");
AZ_TYPE_INFO_SPECIALIZE(Jolt::ActiveEdgeMode, "{D2F0404C-F5D4-44D6-AE62-53E7E8F75D22}");
AZ_TYPE_INFO_SPECIALIZE(Jolt::FaceCollectionMode, "{AD14EC6A-E8DD-4805-A3A8-6A94A4B6B2E3}");
AZ_TYPE_INFO_SPECIALIZE(Jolt::ShapeKind, "{FE5E55C0-C825-448D-AAB4-089989695F99}");
