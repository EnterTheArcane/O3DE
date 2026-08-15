/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/BodyConfiguration.h>
#include <Jolt/ConstraintComponentConfiguration.h>
#include <Jolt/Cooking.h>
#include <Jolt/Material.h>
#include <Jolt/Path.h>
#include <Jolt/SoftBody.h>
#include <Jolt/TypeIds.h>

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Component/EntityId.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Name/Name.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/base.h>
#include <AzCore/std/containers/variant.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/limits.h>

namespace AZ
{
    class ReflectContext;
} // namespace AZ

namespace Jolt
{
    inline constexpr AZ::u32 InvalidAssetIndex = AZStd::numeric_limits<AZ::u32>::max();

    struct SceneSourceShapeData final
    {
        AZ_TYPE_INFO(SceneSourceShapeData, SceneSourceShapeDataTypeId);

        ShapeGeometry m_geometry;
        AZStd::vector<AZ::u32> m_materialIndices;
        AZ::u64 m_userData = 0;
        float m_density = 1'000.0f;
    };

    struct SceneSourceCompoundChild final
    {
        AZ_TYPE_INFO(SceneSourceCompoundChild, SceneSourceCompoundChildTypeId);

        AZ::Vector3 m_position = AZ::Vector3::CreateZero();
        AZ::Quaternion m_rotation = AZ::Quaternion::CreateIdentity();
        AZ::u32 m_shapeIndex = InvalidAssetIndex;
        AZ::u32 m_userData = 0;
    };

    struct SceneSourceCompoundShape final
    {
        AZ_TYPE_INFO(SceneSourceCompoundShape, SceneSourceCompoundShapeTypeId);

        AZStd::vector<SceneSourceCompoundChild> m_children;
        AZ::u64 m_userData = 0;
    };

    struct SceneSourceOffsetCenterOfMassShape final
    {
        AZ_TYPE_INFO(SceneSourceOffsetCenterOfMassShape, SceneSourceOffsetCenterOfMassShapeTypeId);

        AZ::Vector3 m_offset = AZ::Vector3::CreateZero();
        AZ::u64 m_userData = 0;
        AZ::u32 m_shapeIndex = InvalidAssetIndex;
    };

    struct SceneSourceRotatedTranslatedShape final
    {
        AZ_TYPE_INFO(SceneSourceRotatedTranslatedShape, SceneSourceRotatedTranslatedShapeTypeId);

        AZ::Vector3 m_position = AZ::Vector3::CreateZero();
        AZ::Quaternion m_rotation = AZ::Quaternion::CreateIdentity();
        AZ::u64 m_userData = 0;
        AZ::u32 m_shapeIndex = InvalidAssetIndex;
    };

    struct SceneSourceScaledShape final
    {
        AZ_TYPE_INFO(SceneSourceScaledShape, SceneSourceScaledShapeTypeId);

        AZ::Vector3 m_scale = AZ::Vector3::CreateOne();
        AZ::u64 m_userData = 0;
        AZ::u32 m_shapeIndex = InvalidAssetIndex;
    };

    using SceneSourceShape = AZStd::variant<
        SceneSourceShapeData,
        SceneSourceCompoundShape,
        SceneSourceOffsetCenterOfMassShape,
        SceneSourceRotatedTranslatedShape,
        SceneSourceScaledShape>;

    struct SceneAssetShape final
    {
        AZ_TYPE_INFO(SceneAssetShape, SceneAssetShapeTypeId);

        CookedShapeArchive m_archive;
        AZStd::vector<AZ::u32> m_materialIndices;
        AZStd::vector<AZ::u32> m_childShapeIndices;
    };

    struct SceneSourceSoftBodyDefinition final
    {
        AZ_TYPE_INFO(SceneSourceSoftBodyDefinition, SceneSourceSoftBodyDefinitionTypeId);

        AZStd::vector<SoftBodyVertex> m_vertices;
        AZStd::vector<SoftBodyFace> m_faces;
        AZStd::vector<AZ::u32> m_materialIndices;
        AZStd::vector<SoftBodyVertexAttributes> m_vertexAttributes;

        AZStd::vector<SoftBodyEdgeConstraint> m_edgeConstraints;
        AZStd::vector<SoftBodyDihedralBendConstraint> m_dihedralBendConstraints;
        AZStd::vector<SoftBodyLongRangeConstraint> m_longRangeConstraints;
        AZStd::vector<SoftBodyRodStretchShearConstraint> m_rodStretchShearConstraints;
        AZStd::vector<SoftBodyRodBendTwistConstraint> m_rodBendTwistConstraints;
        AZStd::vector<SoftBodyVolumeConstraint> m_volumeConstraints;
        AZStd::vector<SoftBodyInverseBind> m_inverseBinds;
        AZStd::vector<SoftBodySkinConstraint> m_skinConstraints;

        float m_shearAngleTolerance = 0.13962634f;
        SoftBodyBendType m_bendType = SoftBodyBendType::Distance;
        bool m_createFaceConstraints = true;
        bool m_optimize = true;
    };

    struct SceneAssetSoftBodyDefinition final
    {
        AZ_TYPE_INFO(SceneAssetSoftBodyDefinition, SceneAssetSoftBodyDefinitionTypeId);

        SoftBodyDefinitionArchive m_archive;
        AZStd::vector<AZ::u32> m_materialIndices;
    };

    struct SceneAssetRigidBody final
    {
        AZ_TYPE_INFO(SceneAssetRigidBody, SceneAssetRigidBodyTypeId);

        BodyRuntimeConfiguration m_runtime;
        WorldTransform m_transform;
        AZ::Vector3 m_linearVelocity = AZ::Vector3::CreateZero();
        AZ::Vector3 m_angularVelocity = AZ::Vector3::CreateZero();

        AZ::EntityId m_entityId = AZ::EntityId();
        AZ::Name m_name;
        CollisionGroupId m_collisionGroupId;
        CollisionSubGroupId m_collisionSubGroupId;
        ObjectLayer m_objectLayer = DefaultLayers::Moving;
        MotionType m_motionType = MotionType::Dynamic;

        AZ::u32 m_shapeIndex = InvalidAssetIndex;
        AZ::u32 m_groupFilterIndex = InvalidAssetIndex;

        bool m_activate = true;
        bool m_allowDynamicOrKinematic = false;
    };

    struct SceneAssetSoftBody final
    {
        AZ_TYPE_INFO(SceneAssetSoftBody, SceneAssetSoftBodyTypeId);

        SoftBodyRuntimeConfiguration m_runtime;
        WorldTransform m_transform;

        AZ::EntityId m_entityId = AZ::EntityId();
        AZ::Name m_name;
        CollisionGroupId m_collisionGroupId;
        CollisionSubGroupId m_collisionSubGroupId;
        ObjectLayer m_objectLayer = DefaultLayers::Moving;

        AZ::u32 m_definitionIndex = InvalidAssetIndex;
        AZ::u32 m_groupFilterIndex = InvalidAssetIndex;

        bool m_activate = true;
        bool m_makeRotationIdentity = true;
        bool m_manualUpdate = false;
    };

    using SceneAssetBody = AZStd::variant<SceneAssetRigidBody, SceneAssetSoftBody>;

    struct SceneAssetConstraint final
    {
        AZ_TYPE_INFO(SceneAssetConstraint, SceneAssetConstraintTypeId);

        ConstraintComponentGeometry m_geometry;
        AZ::EntityId m_entityId = AZ::EntityId();
        AZ::Name m_name;

        AZ::u32 m_firstBodyIndex = InvalidAssetIndex;
        AZ::u32 m_secondBodyIndex = InvalidAssetIndex;
        AZ::u32 m_firstDependencyIndex = InvalidAssetIndex;
        AZ::u32 m_secondDependencyIndex = InvalidAssetIndex;
        AZ::u32 m_pathIndex = InvalidAssetIndex;
        AZ::u32 m_priority = 0;
        AZ::u8 m_positionStepCount = 0;
        AZ::u8 m_velocityStepCount = 0;
        bool m_enabled = true;
    };

    struct SceneAssetData final
    {
        AZ_TYPE_INFO(SceneAssetData, SceneAssetDataTypeId);

        static void Reflect(AZ::ReflectContext* context);

        AZStd::vector<MaterialConfiguration> m_materials;
        AZStd::vector<SceneAssetShape> m_shapes;
        AZStd::vector<GroupFilterTableConfiguration> m_groupFilters;
        AZStd::vector<HermitePathConfiguration> m_paths;
        AZStd::vector<SceneAssetSoftBodyDefinition> m_softBodyDefinitions;
        AZStd::vector<SceneAssetBody> m_bodies;
        AZStd::vector<SceneAssetConstraint> m_constraints;
        AZ::Name m_name;
    };

    struct SceneSourceData final
    {
        AZ_TYPE_INFO(SceneSourceData, SceneSourceDataTypeId);

        AZStd::vector<MaterialConfiguration> m_materials;
        AZStd::vector<SceneSourceShape> m_shapes;
        AZStd::vector<GroupFilterTableConfiguration> m_groupFilters;
        AZStd::vector<HermitePathConfiguration> m_paths;
        AZStd::vector<SceneSourceSoftBodyDefinition> m_softBodyDefinitions;
        AZStd::vector<SceneAssetBody> m_bodies;
        AZStd::vector<SceneAssetConstraint> m_constraints;
        AZ::Name m_name;
    };

    class SceneAsset final
        : public AZ::Data::AssetData
    {
    public:
        AZ_CLASS_ALLOCATOR(SceneAsset, AZ::SystemAllocator);
        AZ_RTTI(SceneAsset, SceneAssetTypeId, AZ::Data::AssetData);

        explicit SceneAsset(
            const AZ::Data::AssetId& assetId = {},
            AZ::Data::AssetData::AssetStatus status = AZ::Data::AssetData::AssetStatus::NotLoaded);

        static void Reflect(AZ::ReflectContext* context);

        SceneAssetData m_data;
    };
} // namespace Jolt
