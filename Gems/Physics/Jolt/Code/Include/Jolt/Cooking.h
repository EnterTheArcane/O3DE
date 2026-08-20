/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>

#include <Jolt/CustomShape.h>
#include <Jolt/Handle.h>
#include <Jolt/Operation.h>
#include <Jolt/Query.h>
#include <Jolt/Shape.h>
#include <Jolt/ShapeConfiguration.h>
#include <Jolt/TypeIds.h>

#include <AzCore/Math/Quaternion.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/base.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/parallel/atomic.h>
#include <AzCore/std/containers/variant.h>
#include <AzCore/std/containers/vector.h>

namespace AZ
{
    class ReflectContext;
} // namespace AZ

namespace Jolt
{
    class Runtime;
    struct CookedCompoundChildConfiguration final
    {
        AZ_TYPE_INFO(CookedCompoundChildConfiguration, CookedCompoundChildConfigurationTypeId);

        AZ::Vector3 m_position = AZ::Vector3::CreateZero();
        AZ::Quaternion m_rotation = AZ::Quaternion::CreateIdentity();
        CookedShapeHandle m_shapeHandle;
        AZ::u32 m_userData = 0;
    };

    struct CookedCompoundShapeConfiguration final
    {
        AZ_TYPE_INFO(CookedCompoundShapeConfiguration, CookedCompoundShapeConfigurationTypeId);

        AZStd::vector<CookedCompoundChildConfiguration> m_children;
        AZ::u64 m_userData = 0;
    };

    struct CookedOffsetCenterOfMassShapeConfiguration final
    {
        AZ_TYPE_INFO(
            CookedOffsetCenterOfMassShapeConfiguration,
            CookedOffsetCenterOfMassShapeConfigurationTypeId);

        CookedShapeHandle m_shapeHandle;
        AZ::Vector3 m_offset = AZ::Vector3::CreateZero();
    };

    struct CookedRotatedTranslatedShapeConfiguration final
    {
        AZ_TYPE_INFO(
            CookedRotatedTranslatedShapeConfiguration,
            CookedRotatedTranslatedShapeConfigurationTypeId);

        CookedShapeHandle m_shapeHandle;
        AZ::Vector3 m_position = AZ::Vector3::CreateZero();
        AZ::Quaternion m_rotation = AZ::Quaternion::CreateIdentity();
    };

    struct CookedScaledShapeConfiguration final
    {
        AZ_TYPE_INFO(CookedScaledShapeConfiguration, CookedScaledShapeConfigurationTypeId);

        CookedShapeHandle m_shapeHandle;
        AZ::Vector3 m_scale = AZ::Vector3::CreateOne();
    };

    using CookedDecoratedShapeGeometry = AZStd::variant<
        CookedOffsetCenterOfMassShapeConfiguration,
        CookedRotatedTranslatedShapeConfiguration,
        CookedScaledShapeConfiguration>;

    struct CookedDecoratedShapeConfiguration final
    {
        AZ_TYPE_INFO(CookedDecoratedShapeConfiguration, CookedDecoratedShapeConfigurationTypeId);

        CookedDecoratedShapeGeometry m_geometry;
        AZ::u64 m_userData = 0;
    };

    struct CookedShapeArchive final
    {
        AZ_TYPE_INFO(CookedShapeArchive, CookedShapeArchiveTypeId);

        JOLT_API static void Reflect(AZ::ReflectContext* context);

        AZStd::vector<AZ::u8> m_binaryState;
        AZStd::vector<CustomShapeDependency> m_dependencies;
        AZ::u64 m_buildFingerprint = 0;
        AZ::u64 m_contentHash = 0;
        AZ::u32 m_formatVersion = 0;
        AZ::u32 m_materialCount = 0;
        AZ::u32 m_childShapeCount = 0;
    };

    struct CookedRaycastHit final
    {
        AZ_TYPE_INFO(CookedRaycastHit, CookedRaycastHitTypeId);

        AZ::Vector3 m_position = AZ::Vector3::CreateZero();
        AZ::Vector3 m_normal = AZ::Vector3::CreateZero();
        MaterialHandle m_materialHandle;
        SubShapeId m_subShapeId;
        float m_distance = 0.0f;
        float m_fraction = 0.0f;
    };

    //! Owns immutable native shapes that can be shared by bodies in multiple worlds.
    class JOLT_API Cooking
    {
    public:
        [[nodiscard]]
        static Cooking* Get();

        //! Creates the native shape once and retains all referenced materials until destruction.
        [[nodiscard]]
        CookedShapeHandle CookShape(const ShapeConfiguration& configuration);

        [[nodiscard]]
        Operation<CookedShapeHandle> CookShapeAsync(const ShapeConfiguration& configuration);

        [[nodiscard]]
        CookedShapeHandle CookShape(const CookedCompoundShapeConfiguration& configuration);

        [[nodiscard]]
        Operation<CookedShapeHandle> CookShapeAsync(const CookedCompoundShapeConfiguration& configuration);

        [[nodiscard]]
        CookedShapeHandle CookShape(const CookedDecoratedShapeConfiguration& configuration);

        [[nodiscard]]
        Operation<CookedShapeHandle> CookShapeAsync(const CookedDecoratedShapeConfiguration& configuration);

        [[nodiscard]]
        bool ExportShape(
            CookedShapeHandle cookedShapeHandle,
            CookedShapeArchive& archive,
            AZStd::vector<MaterialHandle>& materialHandles,
            AZStd::vector<CookedShapeHandle>& childShapeHandles) const;

        [[nodiscard]]
        CookedShapeHandle ImportShape(
            const CookedShapeArchive& archive,
            AZStd::span<const MaterialHandle> materialHandles,
            AZStd::span<const CookedShapeHandle> childShapeHandles);

        bool DestroyCookedShape(CookedShapeHandle cookedShapeHandle);

        [[nodiscard]]
        bool IsValid(CookedShapeHandle cookedShapeHandle) const;

        //! Returns storage owned directly by the root shape.
        [[nodiscard]]
        bool GetStats(
            CookedShapeHandle cookedShapeHandle,
            ShapeStats& stats) const;

        //! Includes each unique child shape once and may allocate traversal bookkeeping.
        [[nodiscard]]
        bool GetStatsRecursive(
            CookedShapeHandle cookedShapeHandle,
            ShapeStats& stats) const;

        [[nodiscard]]
        bool GetProperties(
            CookedShapeHandle cookedShapeHandle,
            ShapeProperties& properties) const;

        [[nodiscard]]
        bool GetUserData(
            CookedShapeHandle cookedShapeHandle,
            AZ::u64& userData) const;

        [[nodiscard]]
        bool GetCustomConvexShapeInfo(
            CookedShapeHandle cookedShapeHandle,
            CustomConvexShapeInfo& info) const;

        [[nodiscard]]
        bool GetCustomShapeInfo(
            CookedShapeHandle cookedShapeHandle,
            CustomShapeInfo& info) const;

        [[nodiscard]]
        BufferResult GetCustomShapeDependencies(
            CookedShapeHandle cookedShapeHandle,
            AZStd::span<CustomShapeDependency> dependencies) const;

        //! The sub-shape ID must originate from a query against this shape.
        [[nodiscard]]
        bool GetSubShapeUserData(
            CookedShapeHandle cookedShapeHandle,
            SubShapeId subShapeId,
            AZ::u64& userData) const;

        //! Resolves one level of a query sub-shape path in the root shape's center-of-mass space.
        [[nodiscard]]
        bool GetDirectChildShape(
            CookedShapeHandle cookedShapeHandle,
            SubShapeId subShapeId,
            CookedShapeHandle& childShapeHandle,
            SubShapeTransform& transform) const;

        //! Returns the ordered material list for a mesh or decorated mesh root.
        [[nodiscard]]
        BufferResult GetMeshMaterials(
            CookedShapeHandle cookedShapeHandle,
            AZStd::span<MaterialHandle> materialHandles) const;

        //! The sub-shape ID must identify a mesh triangle in this shape.
        [[nodiscard]]
        bool GetMeshTriangleMaterialIndex(
            CookedShapeHandle cookedShapeHandle,
            SubShapeId subShapeId,
            AZ::u32& materialIndex) const;

        //! The sub-shape ID must identify a mesh triangle in this shape.
        [[nodiscard]]
        bool GetMeshTriangleUserData(
            CookedShapeHandle cookedShapeHandle,
            SubShapeId subShapeId,
            AZ::u32& userData) const;

        [[nodiscard]]
        bool GetCompoundChildCount(
            CookedShapeHandle cookedShapeHandle,
            AZ::u32& childCount) const;

        [[nodiscard]]
        bool GetCompoundChild(
            CookedShapeHandle cookedShapeHandle,
            AZ::u32 childIndex,
            CookedCompoundChildConfiguration& child) const;

        //! The sub-shape ID must originate from a query against this compound shape.
        [[nodiscard]]
        bool GetCompoundChildIndex(
            CookedShapeHandle cookedShapeHandle,
            SubShapeId subShapeId,
            AZ::u32& childIndex) const;

        [[nodiscard]]
        bool Raycast(
            CookedShapeHandle cookedShapeHandle,
            const AZ::Vector3& start,
            const AZ::Vector3& direction,
            float distance,
            CookedRaycastHit& hit) const;

    private:
        friend class Runtime;

        Cooking() = default;
        ~Cooking() = default;

        static AZStd::atomic<Cooking*> s_instance;
    };
} // namespace Jolt
