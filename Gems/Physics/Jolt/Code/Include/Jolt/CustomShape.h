/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Shape.h>
#include <Jolt/TypeIds.h>

#include <AzCore/Math/Aabb.h>
#include <AzCore/Math/Uuid.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/base.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace Jolt
{
    enum class CustomShapeGeometryKind : AZ::u8
    {
        None = 0,
        Convex,
        Mesh,
    };

    enum class ProviderRegistrationResult : AZ::u8
    {
        None = 0,
        AlreadyRegistered,
        InUse,
        Invalid,
        NotRegistered,
        Success,
    };

    enum class CustomShapeDispatchResult : AZ::u8
    {
        None = 0,
        Handled,
        Invalid,
        Unsupported,
    };

    inline constexpr AZ::u32 MaximumCustomShapeRuntimeDataSize = 16 * 1024 * 1024;

    struct CustomShapeTriangle final
    {
        AZ_TYPE_INFO(CustomShapeTriangle, CustomShapeTriangleTypeId);

        AZ::u32 m_firstVertex = 0;
        AZ::u32 m_secondVertex = 0;
        AZ::u32 m_thirdVertex = 0;
        AZ::u32 m_materialIndex = 0;
        AZ::u32 m_userData = 0;
    };

    struct CustomShapeDependency final
    {
        AZ_TYPE_INFO(CustomShapeDependency, CustomShapeDependencyTypeId);

        AZStd::string m_path;
        AZ::u64 m_contentHash = 0;
    };

    //! Authoring data routed to a registered general custom-shape provider during cooking.
    struct CustomShapeConfiguration final
    {
        AZ_TYPE_INFO(CustomShapeConfiguration, CustomShapeConfigurationTypeId);

        AZStd::vector<AZ::u8> m_data;
        AZ::Aabb m_editorBounds = AZ::Aabb::CreateNull();
        AZ::TypeId m_providerId = AZ::TypeId::CreateNull();
    };

    //! Immutable, provider-independent geometry produced at the cooking boundary.
    struct CustomShapeData final
    {
        AZStd::vector<AZ::Vector3> m_vertices;
        AZStd::vector<CustomShapeTriangle> m_triangles;

        AZStd::vector<CustomShapeDependency> m_dependencies;
        AZStd::vector<AZ::u8> m_runtimeData;

        float m_activeEdgeCosineThreshold = 0.996195f;
        float m_hullTolerance = 1.0e-3f;
        float m_maximumConvexRadius = 0.05f;
        float m_maximumConvexRadiusError = 0.05f;
        AZ::u32 m_maximumTrianglesPerLeaf = 8;

        CustomShapeGeometryKind m_geometryKind = CustomShapeGeometryKind::None;
        bool m_perTriangleUserData = false;
    };

    struct CustomShapeRaycastRequest final
    {
        AZ::Vector3 m_start = AZ::Vector3::CreateZero();
        AZ::Vector3 m_displacement = AZ::Vector3::CreateZero();
        BackFaceMode m_convexBackFaceMode = BackFaceMode::Ignore;
        BackFaceMode m_triangleBackFaceMode = BackFaceMode::Collide;
        bool m_treatConvexAsSolid = true;
    };

    struct CustomShapeRaycastHit final
    {
        AZ::Vector3 m_position = AZ::Vector3::CreateZero();
        AZ::Vector3 m_normal = AZ::Vector3::CreateZero();
        MaterialHandle m_materialHandle;
        SubShapeId m_subShapeId;
        AZ::u64 m_userData = 0;
        float m_fraction = 0.0f;
        bool m_isBackFaceHit = false;
    };

    struct CustomShapeTriangleVertices final
    {
        AZ::Vector3 m_first = AZ::Vector3::CreateZero();
        AZ::Vector3 m_second = AZ::Vector3::CreateZero();
        AZ::Vector3 m_third = AZ::Vector3::CreateZero();
        MaterialHandle m_materialHandle;
        AZ::u64 m_userData = 0;
    };

    struct CustomShapeCollisionSettings final
    {
        AZ::Vector3 m_activeEdgeMovementDirection = AZ::Vector3::CreateZero();
        BackFaceMode m_backFaceMode = BackFaceMode::Ignore;
        float m_collisionTolerance = 1.0e-4f;
        float m_internalEdgeRemovalVertexTolerance = 1.0e-4f;
        float m_maximumSeparationDistance = 0.0f;
        float m_penetrationTolerance = 1.0e-4f;
        ActiveEdgeMode m_activeEdgeMode = ActiveEdgeMode::CollideOnlyWithActive;
        FaceCollectionMode m_faceCollectionMode = FaceCollectionMode::None;
    };

    struct CustomShapeCastSettings final
    {
        AZ::Vector3 m_displacement = AZ::Vector3::CreateZero();
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

    struct CustomShapeCollisionHit final
    {
        AZStd::span<const AZ::Vector3> m_firstFace;
        AZStd::span<const AZ::Vector3> m_secondFace;
        AZ::Vector3 m_firstContactPosition = AZ::Vector3::CreateZero();
        AZ::Vector3 m_secondContactPosition = AZ::Vector3::CreateZero();
        AZ::Vector3 m_penetrationAxis = AZ::Vector3::CreateZero();
        SubShapeId m_firstSubShapeId;
        SubShapeId m_secondSubShapeId;
        float m_penetrationDepth = 0.0f;
    };

    struct CustomShapeCastHit final
    {
        CustomShapeCollisionHit m_collision;
        float m_fraction = 0.0f;
        bool m_isBackFaceHit = false;
    };

    //! Immutable collision view valid only for the duration of a provider callback.
    //! Operations use center-of-mass-relative coordinates and do not allocate.
    class ICustomShapeView
    {
    public:
        AZ_RTTI(ICustomShapeView, ICustomShapeViewTypeId);

        virtual ~ICustomShapeView() = default;

        [[nodiscard]]
        virtual AZ::TypeId GetProviderId() const = 0;

        [[nodiscard]]
        virtual AZ::u64 GetProviderVersion() const = 0;

        [[nodiscard]]
        virtual AZStd::span<const AZ::u8> GetRuntimeData() const = 0;

        [[nodiscard]]
        virtual AZ::Transform GetCenterOfMassTransform() const = 0;

        [[nodiscard]]
        virtual AZ::Vector3 GetScale() const = 0;

        [[nodiscard]]
        virtual ShapeProperties GetProperties() const = 0;

        [[nodiscard]]
        virtual ShapeStats GetStats() const = 0;

        [[nodiscard]]
        virtual MaterialHandle GetMaterial(SubShapeId subShapeId) const = 0;

        [[nodiscard]]
        virtual AZ::u64 GetUserData(SubShapeId subShapeId) const = 0;

        [[nodiscard]]
        virtual bool GetSurfaceNormal(
            SubShapeId subShapeId,
            const AZ::Vector3& localSurfacePosition,
            AZ::Vector3& normal) const = 0;

        [[nodiscard]]
        virtual QueryResult GetSupportingFace(
            SubShapeId subShapeId,
            const AZ::Vector3& direction,
            AZStd::span<AZ::Vector3> vertices) const = 0;

        [[nodiscard]]
        virtual bool Raycast(
            const CustomShapeRaycastRequest& request,
            CustomShapeRaycastHit& hit) const = 0;

        [[nodiscard]]
        virtual bool CollidePoint(const AZ::Vector3& point) const = 0;

        [[nodiscard]]
        virtual QueryResult CollectTriangles(
            const AZ::Aabb& bounds,
            AZStd::span<CustomShapeTriangleVertices> triangles) const = 0;
    };

    class ICustomShapeCollisionCollector
    {
    public:
        AZ_RTTI(ICustomShapeCollisionCollector, ICustomShapeCollisionCollectorTypeId);

        virtual ~ICustomShapeCollisionCollector() = default;

        [[nodiscard]]
        virtual float GetEarlyOutFraction() const = 0;

        [[nodiscard]]
        virtual bool AddHit(const CustomShapeCollisionHit& hit) = 0;
    };

    class ICustomShapeCastCollector
    {
    public:
        AZ_RTTI(ICustomShapeCastCollector, ICustomShapeCastCollectorTypeId);

        virtual ~ICustomShapeCastCollector() = default;

        [[nodiscard]]
        virtual float GetEarlyOutFraction() const = 0;

        [[nodiscard]]
        virtual bool AddHit(const CustomShapeCastHit& hit) = 0;
    };

    //! General custom-shape cooking provider. The identifier and nonzero version must remain stable while registered.
    //! Cook may run concurrently, must be deterministic, and must not reenter ICooking or ISystem.
    class ICustomShapeProvider
    {
    public:
        AZ_RTTI(ICustomShapeProvider, ICustomShapeProviderTypeId);

        virtual ~ICustomShapeProvider() = default;

        [[nodiscard]]
        virtual AZ::TypeId GetId() const = 0;

        [[nodiscard]]
        virtual AZ::u64 GetVersion() const = 0;

        [[nodiscard]]
        virtual bool Cook(
            AZStd::span<const AZ::u8> input,
            CustomShapeData& output) const = 0;

        //! Called only when this provider participates in a collision pair. Returning Unsupported uses the cooked fallback.
        //! The callback must be thread-safe, deterministic, allocation-free, and must not reenter ISystem.
        [[nodiscard]]
        virtual CustomShapeDispatchResult Collide(
            const ICustomShapeView&,
            const ICustomShapeView&,
            const CustomShapeCollisionSettings&,
            ICustomShapeCollisionCollector&) const
        {
            return CustomShapeDispatchResult::Unsupported;
        }

        //! Called only when this provider participates in a shape cast. The first shape is the moving shape.
        [[nodiscard]]
        virtual CustomShapeDispatchResult Cast(
            const ICustomShapeView&,
            const ICustomShapeView&,
            const CustomShapeCastSettings&,
            ICustomShapeCastCollector&) const
        {
            return CustomShapeDispatchResult::Unsupported;
        }
    };

    struct CustomShapeInfo final
    {
        AZ_TYPE_INFO(CustomShapeInfo, CustomShapeInfoTypeId);

        AZ::TypeId m_providerId = AZ::TypeId::CreateNull();
        AZ::u64 m_providerVersion = 0;
        AZ::u64 m_sourceHash = 0;
    };

    //! Authoring data routed to a registered custom convex-shape provider during cooking.
    struct CustomConvexShapeConfiguration final
    {
        AZ_TYPE_INFO(CustomConvexShapeConfiguration, CustomConvexShapeConfigurationTypeId);

        AZStd::vector<AZ::u8> m_data;
        AZ::Aabb m_editorBounds = AZ::Aabb::CreateNull();
        AZ::TypeId m_providerId = AZ::TypeId::CreateNull();
    };

    //! Immutable geometry produced once at the cooking boundary.
    struct CustomConvexShapeData final
    {
        AZStd::vector<AZ::Vector3> m_points;
        float m_hullTolerance = 1.0e-3f;
        float m_maximumConvexRadius = 0.05f;
        float m_maximumConvexRadiusError = 0.05f;
    };

    //! Converts provider-specific data into immutable convex geometry.
    class ICustomConvexShapeProvider
    {
    public:
        AZ_RTTI(ICustomConvexShapeProvider, ICustomConvexShapeProviderTypeId);

        virtual ~ICustomConvexShapeProvider() = default;

        //! Identifies the provider in serialized authoring data.
        [[nodiscard]]
        virtual AZ::TypeId GetId() const = 0;

        //! Changes whenever identical input data may produce different cooked geometry.
        [[nodiscard]]
        virtual AZ::u64 GetVersion() const = 0;

        //! May be called concurrently. The implementation must be deterministic and must not call ICooking.
        [[nodiscard]]
        virtual bool Cook(
            AZStd::span<const AZ::u8> input,
            CustomConvexShapeData& output) const = 0;
    };

    struct CustomConvexShapeInfo final
    {
        AZ_TYPE_INFO(CustomConvexShapeInfo, CustomConvexShapeInfoTypeId);

        AZ::TypeId m_providerId = AZ::TypeId::CreateNull();
        AZ::u64 m_providerVersion = 0;
        AZ::u64 m_sourceHash = 0;
    };
} // namespace Jolt

AZ_TYPE_INFO_SPECIALIZE(Jolt::CustomShapeGeometryKind, "{BE713BA7-53C1-4B14-8BC5-8246EE3F0D3F}");
AZ_TYPE_INFO_SPECIALIZE(Jolt::ProviderRegistrationResult, "{29601816-36B2-453A-B901-3AA81AB5EC0C}");
AZ_TYPE_INFO_SPECIALIZE(Jolt::CustomShapeDispatchResult, "{40D4B16F-A952-4AFD-86CB-8E702EFCB565}");
