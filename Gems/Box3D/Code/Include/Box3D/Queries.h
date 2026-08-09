/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Box3D/Collision.h>
#include <Box3D/Handle.h>
#include <Box3D/ShapeConfiguration.h>
#include <Box3D/TypeIds.h>

#include <AzCore/Math/Aabb.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/base.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/containers/variant.h>

#include <cstddef>
#include <type_traits>

namespace AZ
{
    class ReflectContext;
}

namespace Box3D
{
    void ReflectQueries(AZ::ReflectContext* context);

    enum class QueryBodyTypes : AZ::u8
    {
        None = 0,
        Static = 1 << 0,
        Kinematic = 1 << 1,
        Dynamic = 1 << 2,
        All = 0x07,
    };
    AZ_DEFINE_ENUM_BITWISE_OPERATORS(QueryBodyTypes)

    struct QueryHit final
    {
        AZ_TYPE_INFO(QueryHit, "{6E9608CC-57E6-4599-8733-46A78D779D0D}");

        BodyHandle m_bodyHandle;
        ShapeHandle m_shapeHandle;
        MaterialHandle m_materialHandle;
        AZ::Vector3 m_position = AZ::Vector3::CreateZero();
        AZ::Vector3 m_normal = AZ::Vector3::CreateZero();
        float m_distance = 0.0f;
        float m_fraction = 0.0f;
        AZ::s32 m_faceIndex = -1;
        AZ::s32 m_childIndex = -1;
    };

    //! Identity returned by overlap queries, which do not compute contact geometry.
    struct OverlapHit final
    {
        AZ_TYPE_INFO(OverlapHit, "{A2E0A749-834A-44CD-9949-15F6C797B186}");

        BodyHandle m_bodyHandle;
        ShapeHandle m_shapeHandle;
    };

    static_assert(sizeof(OverlapHit) == 2 * sizeof(AZ::u64));
    static_assert(std::is_trivially_copyable_v<OverlapHit>);

    struct ClosestQueryResult final
    {
        AZ_TYPE_INFO(ClosestQueryResult, "{609A90DA-A6A8-43A0-95AC-5585C5D76010}");

        QueryHit m_hit;
        bool m_found = false;
    };

    //! Hit returned by a body-independent query against reusable cooked geometry.
    struct GeometryHit final
    {
        AZ_TYPE_INFO(GeometryHit, GeometryHitTypeId);

        AZ::Vector3 m_position = AZ::Vector3::CreateZero();
        AZ::Vector3 m_normal = AZ::Vector3::CreateZero();
        float m_distance = 0.0f;
        float m_fraction = 0.0f;
        AZ::s32 m_materialIndex = -1;
        AZ::s32 m_faceIndex = -1;
        AZ::s32 m_childIndex = -1;
    };

    using QueryFilterCallback = bool (*)(const QueryHit& hit, void* userData);

    struct QueryFilter final
    {
        AZ_TYPE_INFO(QueryFilter, "{FD320F4D-F889-4B25-BF31-6BD2B6FCFC7D}");

        CollisionFilter m_collisionFilter;
        QueryFilterCallback m_callback = nullptr;
        void* m_userData = nullptr;
        QueryBodyTypes m_bodyTypes = QueryBodyTypes::All;
        bool m_includeSensors = false;
    };

    struct RaycastRequest final
    {
        AZ_TYPE_INFO(RaycastRequest, "{85230D4B-5574-4215-97BE-01387C5C2EDB}");

        AZ::Vector3 m_start = AZ::Vector3::CreateZero();
        AZ::Vector3 m_direction = AZ::Vector3::CreateAxisX();
        QueryFilter m_filter;
        float m_distance = 1.0f;
    };

    //! Convex geometry accepted by overlap and sweep queries.
    using QueryGeometry = AZStd::variant<
        SphereShapeConfiguration,
        CapsuleShapeConfiguration,
        BoxShapeConfiguration,
        CylinderShapeConfiguration,
        ConvexHullShapeConfiguration>;

    struct ShapeCastRequest final
    {
        QueryGeometry m_geometry;
        AZ::Transform m_start = AZ::Transform::CreateIdentity();
        AZ::Vector3 m_translation = AZ::Vector3::CreateZero();
        QueryFilter m_filter;
    };

    struct OverlapRequest final
    {
        QueryGeometry m_geometry;
        AZ::Transform m_transform = AZ::Transform::CreateIdentity();
        QueryFilter m_filter;
    };

    struct AabbOverlapRequest final
    {
        AZ_TYPE_INFO(AabbOverlapRequest, "{58D51505-4869-4B80-9D81-50EEFC4E3CA2}");

        AZ::Aabb m_aabb = AZ::Aabb::CreateNull();
        QueryFilter m_filter;
    };

    //! Closest raycast against the shapes attached to one body.
    struct BodyRaycastRequest final
    {
        AZ::Vector3 m_start = AZ::Vector3::CreateZero();
        AZ::Vector3 m_direction = AZ::Vector3::CreateAxisX();
        CollisionFilter m_filter;
        float m_distance = 1.0f;
    };

    //! Closest convex sweep against the shapes attached to one body.
    struct BodyShapeCastRequest final
    {
        QueryGeometry m_geometry;
        AZ::Transform m_start = AZ::Transform::CreateIdentity();
        AZ::Vector3 m_translation = AZ::Vector3::CreateZero();
        CollisionFilter m_filter;
    };

    //! Convex overlap test against the shapes attached to one body.
    struct BodyOverlapRequest final
    {
        QueryGeometry m_geometry;
        AZ::Transform m_transform = AZ::Transform::CreateIdentity();
        CollisionFilter m_filter;
    };

    //! Number of accepted hits and whether the caller-provided output was too small.
    struct QueryResult final
    {
        size_t m_hitCount = 0;
        size_t m_requiredHitCount = 0;

        [[nodiscard]] constexpr bool HasOverflow() const noexcept
        {
            return m_requiredHitCount > m_hitCount;
        }
    };

    //! Borrowed hot-path query view. The pointer remains valid until its world is destroyed.
    class IWorldQueries
    {
    public:
        AZ_RTTI(IWorldQueries, IWorldQueriesTypeId);

        virtual ~IWorldQueries() = default;

        [[nodiscard]] virtual bool RaycastClosest(const RaycastRequest& request, QueryHit& hit) const = 0;
        [[nodiscard]] virtual BufferResult RaycastClosestBatch(
            AZStd::span<const RaycastRequest> requests, AZStd::span<ClosestQueryResult> results) const = 0;
        [[nodiscard]] virtual QueryResult Raycast(const RaycastRequest& request, AZStd::span<QueryHit> hits) const = 0;
        [[nodiscard]] virtual QueryResult ShapeCast(const ShapeCastRequest& request, AZStd::span<QueryHit> hits) const = 0;
        [[nodiscard]] virtual QueryResult Overlap(const OverlapRequest& request, AZStd::span<OverlapHit> hits) const = 0;
        [[nodiscard]] virtual QueryResult Overlap(const OverlapRequest& request, AZStd::span<QueryHit> hits) const = 0;
        [[nodiscard]] virtual QueryResult OverlapAabb(const AabbOverlapRequest& request, AZStd::span<OverlapHit> hits) const = 0;
        [[nodiscard]] virtual QueryResult OverlapAabb(const AabbOverlapRequest& request, AZStd::span<QueryHit> hits) const = 0;
    };
} // namespace Box3D
