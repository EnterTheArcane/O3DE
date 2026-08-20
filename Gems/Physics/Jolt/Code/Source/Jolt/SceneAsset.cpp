/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/SceneAsset.h>

#include <Jolt/Profiler.h>
#include <Jolt/RigidBodyConfiguration.h>
#include <Jolt/SoftBodyComponentConfiguration.h>
#include <Jolt/SystemInternal.h>

#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/typetraits/is_same.h>
#include <AzCore/std/typetraits/remove_cvref.h>
#include <AzCore/std/utility/move.h>

namespace Jolt
{
    namespace
    {
        [[nodiscard]]
        bool ResolveGroupFilter(
            const AZ::u32 filterIndex,
            const AZStd::span<const GroupFilterHandle> filters,
            CollisionGroupConfiguration& collisionGroup)
        {
            if (filterIndex == InvalidAssetIndex)
            {
                collisionGroup.m_filterHandle = GroupFilterHandle::Invalid;
                return true;
            }
            if (filterIndex >= filters.size())
            {
                return false;
            }

            collisionGroup.m_filterHandle = filters[filterIndex];
            return true;
        }

        [[nodiscard]]
        SceneRigidBodyConfiguration ResolveRigidBody(
            const SceneAssetRigidBody& source,
            const CookedShapeHandle shapeHandle,
            const GroupFilterHandle filterHandle)
        {
            BodyConfiguration result;
            result.m_transform = source.m_transform;
            result.m_linearVelocity = source.m_linearVelocity;
            result.m_angularVelocity = source.m_angularVelocity;
            result.m_entityId = source.m_entityId;
            result.m_name = source.m_name;
            result.m_collisionGroup = {
                .m_filterHandle = filterHandle,
                .m_groupId = source.m_collisionGroupId,
                .m_subGroupId = source.m_collisionSubGroupId,
            };
            result.m_objectLayer = source.m_objectLayer;
            result.m_allowedDofs = source.m_runtime.m_allowedDofs;
            result.m_massProperties = source.m_runtime.m_massProperties;
            result.m_motionQuality = source.m_runtime.m_motionQuality;
            result.m_motionType = source.m_motionType;
            result.m_friction = source.m_runtime.m_friction;
            result.m_restitution = source.m_runtime.m_restitution;
            result.m_linearDamping = source.m_runtime.m_linearDamping;
            result.m_angularDamping = source.m_runtime.m_angularDamping;
            result.m_maximumLinearVelocity = source.m_runtime.m_maximumLinearVelocity;
            result.m_maximumAngularVelocity = source.m_runtime.m_maximumAngularVelocity;
            result.m_gravityFactor = source.m_runtime.m_gravityFactor;
            result.m_velocityStepCount = source.m_runtime.m_velocityStepCount;
            result.m_positionStepCount = source.m_runtime.m_positionStepCount;
            result.m_activate = source.m_activate;
            result.m_allowDynamicOrKinematic = source.m_allowDynamicOrKinematic;
            result.m_allowSleeping = source.m_runtime.m_allowSleeping;
            result.m_applyGyroscopicForce = source.m_runtime.m_applyGyroscopicForce;
            result.m_collideKinematicVsNonDynamic = source.m_runtime.m_collideKinematicVsNonDynamic;
            result.m_enhancedInternalEdgeRemoval = source.m_runtime.m_enhancedInternalEdgeRemoval;
            result.m_isSensor = source.m_runtime.m_isSensor;
            result.m_useManifoldReduction = source.m_runtime.m_useManifoldReduction;

            return {
                .m_cookedShapeHandle = shapeHandle,
                .m_body = AZStd::move(result),
            };
        }

        [[nodiscard]]
        SoftBodyConfiguration ResolveSoftBody(
            const SceneAssetSoftBody& source,
            const SoftBodyDefinitionHandle definitionHandle,
            const GroupFilterHandle filterHandle)
        {
            return {
                .m_definitionHandle = definitionHandle,
                .m_transform = source.m_transform,
                .m_entityId = source.m_entityId,
                .m_name = source.m_name,
                .m_collisionGroup = {
                    .m_filterHandle = filterHandle,
                    .m_groupId = source.m_collisionGroupId,
                    .m_subGroupId = source.m_collisionSubGroupId,
                },
                .m_objectLayer = source.m_objectLayer,
                .m_friction = source.m_runtime.m_friction,
                .m_gravityFactor = source.m_runtime.m_gravityFactor,
                .m_linearDamping = source.m_runtime.m_linearDamping,
                .m_maximumLinearVelocity = source.m_runtime.m_maximumLinearVelocity,
                .m_pressure = source.m_runtime.m_pressure,
                .m_restitution = source.m_runtime.m_restitution,
                .m_skinnedMaximumDistanceMultiplier = source.m_runtime.m_skinnedMaximumDistanceMultiplier,
                .m_vertexRadius = source.m_runtime.m_vertexRadius,
                .m_iterationCount = source.m_runtime.m_iterationCount,
                .m_activate = source.m_activate,
                .m_allowSleeping = source.m_runtime.m_allowSleeping,
                .m_enableSkinConstraints = source.m_runtime.m_enableSkinConstraints,
                .m_facesDoubleSided = source.m_runtime.m_facesDoubleSided,
                .m_makeRotationIdentity = source.m_makeRotationIdentity,
                .m_manualUpdate = source.m_manualUpdate,
                .m_updatePosition = source.m_runtime.m_updatePosition,
            };
        }

        [[nodiscard]]
        bool ResolveConstraintGeometry(
            const SceneAssetConstraint& source,
            const AZStd::span<const PathHandle> paths,
            ConstraintGeometry& geometry)
        {
            bool resolved = true;
            AZStd::visit(
                [&](const auto& sourceGeometry)
                {
                    using Geometry = AZStd::remove_cvref_t<decltype(sourceGeometry)>;
                    if constexpr (AZStd::is_same_v<Geometry, GearConstraintComponentConfiguration>)
                    {
                        if (source.m_pathIndex != InvalidAssetIndex
                            || sourceGeometry.m_firstHingeEntityId.IsValid()
                            || sourceGeometry.m_secondHingeEntityId.IsValid())
                        {
                            resolved = false;
                            return;
                        }
                        geometry = GearConstraintConfiguration{
                            .m_firstHingeAxis = sourceGeometry.m_firstHingeAxis,
                            .m_secondHingeAxis = sourceGeometry.m_secondHingeAxis,
                            .m_ratio = sourceGeometry.m_ratio,
                            .m_space = sourceGeometry.m_space,
                        };
                    }
                    else if constexpr (AZStd::is_same_v<Geometry, PathConstraintComponentConfiguration>)
                    {
                        if (source.m_pathIndex >= paths.size() || sourceGeometry.m_pathEntityId.IsValid())
                        {
                            resolved = false;
                            return;
                        }
                        geometry = PathConstraintConfiguration{
                            .m_pathHandle = paths[source.m_pathIndex],
                            .m_pathPosition = sourceGeometry.m_pathPosition,
                            .m_pathRotation = sourceGeometry.m_pathRotation,
                            .m_positionMotor = sourceGeometry.m_positionMotor,
                            .m_maximumFrictionForce = sourceGeometry.m_maximumFrictionForce,
                            .m_pathFraction = sourceGeometry.m_pathFraction,
                            .m_targetPathFraction = sourceGeometry.m_targetPathFraction,
                            .m_targetVelocity = sourceGeometry.m_targetVelocity,
                            .m_rotationConstraint = sourceGeometry.m_rotationConstraint,
                        };
                    }
                    else if constexpr (AZStd::is_same_v<Geometry, RackAndPinionConstraintComponentConfiguration>)
                    {
                        if (source.m_pathIndex != InvalidAssetIndex
                            || sourceGeometry.m_pinionConstraintEntityId.IsValid()
                            || sourceGeometry.m_rackConstraintEntityId.IsValid())
                        {
                            resolved = false;
                            return;
                        }
                        geometry = RackAndPinionConstraintConfiguration{
                            .m_hingeAxis = sourceGeometry.m_hingeAxis,
                            .m_sliderAxis = sourceGeometry.m_sliderAxis,
                            .m_ratio = sourceGeometry.m_ratio,
                            .m_space = sourceGeometry.m_space,
                        };
                    }
                    else
                    {
                        if (source.m_pathIndex != InvalidAssetIndex)
                        {
                            resolved = false;
                            return;
                        }
                        geometry = sourceGeometry;
                    }
                },
                source.m_geometry);
            return resolved;
        }
    } // namespace

    void SceneAssetData::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            if (!ShouldReflect<SceneAssetData>(*serializeContext))
            {
                return;
            }

            CollisionGroupId::Reflect(context);
            CollisionSubGroupId::Reflect(context);
            ConstraintComponentConfiguration::Reflect(context);
            GroupFilterTableConfiguration::Reflect(context);
            HermitePathConfiguration::Reflect(context);
            MaterialConfiguration::Reflect(context);
            RigidBodyConfiguration::Reflect(context);
            ShapeConfiguration::Reflect(context);
            SoftBodyComponentConfiguration::Reflect(context);
            WorldTransform::Reflect(context);

            serializeContext
                ->Class<SceneSourceShapeData>()
                ->Field("Geometry", &SceneSourceShapeData::m_geometry)
                ->Field("MaterialIndices", &SceneSourceShapeData::m_materialIndices)
                ->Field("UserData", &SceneSourceShapeData::m_userData)
                ->Field("Density", &SceneSourceShapeData::m_density);

            serializeContext
                ->Class<SceneSourceCompoundChild>()
                ->Field("Position", &SceneSourceCompoundChild::m_position)
                ->Field("Rotation", &SceneSourceCompoundChild::m_rotation)
                ->Field("ShapeIndex", &SceneSourceCompoundChild::m_shapeIndex)
                ->Field("UserData", &SceneSourceCompoundChild::m_userData);

            serializeContext
                ->Class<SceneSourceCompoundShape>()
                ->Field("Children", &SceneSourceCompoundShape::m_children)
                ->Field("UserData", &SceneSourceCompoundShape::m_userData);

            serializeContext
                ->Class<SceneSourceOffsetCenterOfMassShape>()
                ->Field("Offset", &SceneSourceOffsetCenterOfMassShape::m_offset)
                ->Field("UserData", &SceneSourceOffsetCenterOfMassShape::m_userData)
                ->Field("ShapeIndex", &SceneSourceOffsetCenterOfMassShape::m_shapeIndex);

            serializeContext
                ->Class<SceneSourceRotatedTranslatedShape>()
                ->Field("Position", &SceneSourceRotatedTranslatedShape::m_position)
                ->Field("Rotation", &SceneSourceRotatedTranslatedShape::m_rotation)
                ->Field("UserData", &SceneSourceRotatedTranslatedShape::m_userData)
                ->Field("ShapeIndex", &SceneSourceRotatedTranslatedShape::m_shapeIndex);

            serializeContext
                ->Class<SceneSourceScaledShape>()
                ->Field("Scale", &SceneSourceScaledShape::m_scale)
                ->Field("UserData", &SceneSourceScaledShape::m_userData)
                ->Field("ShapeIndex", &SceneSourceScaledShape::m_shapeIndex);

            serializeContext
                ->Class<SceneAssetShape>()
                ->Field("Archive", &SceneAssetShape::m_archive)
                ->Field("MaterialIndices", &SceneAssetShape::m_materialIndices)
                ->Field("ChildShapeIndices", &SceneAssetShape::m_childShapeIndices);

            serializeContext
                ->Class<SceneSourceSoftBodyDefinition>()
                ->Field("Vertices", &SceneSourceSoftBodyDefinition::m_vertices)
                ->Field("Faces", &SceneSourceSoftBodyDefinition::m_faces)
                ->Field("MaterialIndices", &SceneSourceSoftBodyDefinition::m_materialIndices)
                ->Field("VertexAttributes", &SceneSourceSoftBodyDefinition::m_vertexAttributes)
                ->Field("EdgeConstraints", &SceneSourceSoftBodyDefinition::m_edgeConstraints)
                ->Field("DihedralBendConstraints", &SceneSourceSoftBodyDefinition::m_dihedralBendConstraints)
                ->Field("LongRangeConstraints", &SceneSourceSoftBodyDefinition::m_longRangeConstraints)
                ->Field("RodStretchShearConstraints", &SceneSourceSoftBodyDefinition::m_rodStretchShearConstraints)
                ->Field("RodBendTwistConstraints", &SceneSourceSoftBodyDefinition::m_rodBendTwistConstraints)
                ->Field("VolumeConstraints", &SceneSourceSoftBodyDefinition::m_volumeConstraints)
                ->Field("InverseBinds", &SceneSourceSoftBodyDefinition::m_inverseBinds)
                ->Field("SkinConstraints", &SceneSourceSoftBodyDefinition::m_skinConstraints)
                ->Field("ShearAngleTolerance", &SceneSourceSoftBodyDefinition::m_shearAngleTolerance)
                ->Field("BendType", &SceneSourceSoftBodyDefinition::m_bendType)
                ->Field("CreateFaceConstraints", &SceneSourceSoftBodyDefinition::m_createFaceConstraints)
                ->Field("Optimize", &SceneSourceSoftBodyDefinition::m_optimize);

            serializeContext
                ->Class<SceneAssetSoftBodyDefinition>()
                ->Field("Archive", &SceneAssetSoftBodyDefinition::m_archive)
                ->Field("MaterialIndices", &SceneAssetSoftBodyDefinition::m_materialIndices);

            serializeContext
                ->Class<SceneAssetRigidBody>()
                ->Field("Runtime", &SceneAssetRigidBody::m_runtime)
                ->Field("Transform", &SceneAssetRigidBody::m_transform)
                ->Field("LinearVelocity", &SceneAssetRigidBody::m_linearVelocity)
                ->Field("AngularVelocity", &SceneAssetRigidBody::m_angularVelocity)
                ->Field("EntityId", &SceneAssetRigidBody::m_entityId)
                ->Field("Name", &SceneAssetRigidBody::m_name)
                ->Field("CollisionGroupId", &SceneAssetRigidBody::m_collisionGroupId)
                ->Field("CollisionSubGroupId", &SceneAssetRigidBody::m_collisionSubGroupId)
                ->Field("ObjectLayer", &SceneAssetRigidBody::m_objectLayer)
                ->Field("MotionType", &SceneAssetRigidBody::m_motionType)
                ->Field("ShapeIndex", &SceneAssetRigidBody::m_shapeIndex)
                ->Field("GroupFilterIndex", &SceneAssetRigidBody::m_groupFilterIndex)
                ->Field("Activate", &SceneAssetRigidBody::m_activate)
                ->Field("AllowDynamicOrKinematic", &SceneAssetRigidBody::m_allowDynamicOrKinematic);

            serializeContext
                ->Class<SceneAssetSoftBody>()
                ->Field("Runtime", &SceneAssetSoftBody::m_runtime)
                ->Field("Transform", &SceneAssetSoftBody::m_transform)
                ->Field("EntityId", &SceneAssetSoftBody::m_entityId)
                ->Field("Name", &SceneAssetSoftBody::m_name)
                ->Field("CollisionGroupId", &SceneAssetSoftBody::m_collisionGroupId)
                ->Field("CollisionSubGroupId", &SceneAssetSoftBody::m_collisionSubGroupId)
                ->Field("ObjectLayer", &SceneAssetSoftBody::m_objectLayer)
                ->Field("DefinitionIndex", &SceneAssetSoftBody::m_definitionIndex)
                ->Field("GroupFilterIndex", &SceneAssetSoftBody::m_groupFilterIndex)
                ->Field("Activate", &SceneAssetSoftBody::m_activate)
                ->Field("MakeRotationIdentity", &SceneAssetSoftBody::m_makeRotationIdentity)
                ->Field("ManualUpdate", &SceneAssetSoftBody::m_manualUpdate);

            serializeContext
                ->Class<SceneAssetConstraint>()
                ->Field("Geometry", &SceneAssetConstraint::m_geometry)
                ->Field("EntityId", &SceneAssetConstraint::m_entityId)
                ->Field("Name", &SceneAssetConstraint::m_name)
                ->Field("FirstBodyIndex", &SceneAssetConstraint::m_firstBodyIndex)
                ->Field("SecondBodyIndex", &SceneAssetConstraint::m_secondBodyIndex)
                ->Field("FirstDependencyIndex", &SceneAssetConstraint::m_firstDependencyIndex)
                ->Field("SecondDependencyIndex", &SceneAssetConstraint::m_secondDependencyIndex)
                ->Field("PathIndex", &SceneAssetConstraint::m_pathIndex)
                ->Field("Priority", &SceneAssetConstraint::m_priority)
                ->Field("PositionStepCount", &SceneAssetConstraint::m_positionStepCount)
                ->Field("VelocityStepCount", &SceneAssetConstraint::m_velocityStepCount)
                ->Field("Enabled", &SceneAssetConstraint::m_enabled);

            serializeContext
                ->Class<SceneAssetData>()
                ->Field("Materials", &SceneAssetData::m_materials)
                ->Field("Shapes", &SceneAssetData::m_shapes)
                ->Field("GroupFilters", &SceneAssetData::m_groupFilters)
                ->Field("Paths", &SceneAssetData::m_paths)
                ->Field("SoftBodyDefinitions", &SceneAssetData::m_softBodyDefinitions)
                ->Field("Bodies", &SceneAssetData::m_bodies)
                ->Field("Constraints", &SceneAssetData::m_constraints)
                ->Field("Name", &SceneAssetData::m_name);

            serializeContext
                ->Class<SceneSourceData>()
                ->Field("Materials", &SceneSourceData::m_materials)
                ->Field("Shapes", &SceneSourceData::m_shapes)
                ->Field("GroupFilters", &SceneSourceData::m_groupFilters)
                ->Field("Paths", &SceneSourceData::m_paths)
                ->Field("SoftBodyDefinitions", &SceneSourceData::m_softBodyDefinitions)
                ->Field("Bodies", &SceneSourceData::m_bodies)
                ->Field("Constraints", &SceneSourceData::m_constraints)
                ->Field("Name", &SceneSourceData::m_name);
        }
    }

    SceneAsset::SceneAsset(
        const AZ::Data::AssetId& assetId,
        const AZ::Data::AssetData::AssetStatus status)
        : AZ::Data::AssetData(assetId, status)
    {
    }

    void SceneAsset::Reflect(
        AZ::ReflectContext* context)
    {
        SceneAssetData::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            if (!ShouldReflect<SceneAsset>(*serializeContext))
            {
                return;
            }

            serializeContext
                ->Class<SceneAsset, AZ::Data::AssetData>()
                ->Field("Data", &SceneAsset::m_data);
        }
    }

    bool RuntimeImplementation::BuildSceneAsset(
        const SceneSourceData& sourceData,
        SceneAssetData& assetData)
    {
        JOLT_PROFILE_SCOPE(Physics, "Jolt::RuntimeImplementation::BuildSceneAsset");
        AZStd::vector<MaterialHandle> materials;
        AZStd::vector<CookedShapeHandle> shapes;
        AZStd::unordered_map<MaterialHandle, AZ::u32> materialIndices;
        AZStd::unordered_map<CookedShapeHandle, AZ::u32> shapeIndices;
        materials.reserve(sourceData.m_materials.size());
        shapes.reserve(sourceData.m_shapes.size());
        materialIndices.reserve(sourceData.m_materials.size());
        shapeIndices.reserve(sourceData.m_shapes.size());

        const auto releaseSourceResources = [&]()
        {
            for (auto shape = shapes.rbegin(); shape != shapes.rend(); ++shape)
            {
                [[maybe_unused]] const bool destroyed = DestroyCookedShape(*shape);
                AZ_Assert(destroyed, "Scene source shape destruction failed.");
            }
            for (auto material = materials.rbegin(); material != materials.rend(); ++material)
            {
                [[maybe_unused]] const bool destroyed = DestroyMaterial(*material);
                AZ_Assert(destroyed, "Scene source material destruction failed.");
            }
        };

        for (AZ::u32 materialIndex = 0; materialIndex < sourceData.m_materials.size(); ++materialIndex)
        {
            const MaterialHandle materialHandle = CreateMaterial(sourceData.m_materials[materialIndex]);
            if (!materialHandle)
            {
                releaseSourceResources();
                return false;
            }
            materials.push_back(materialHandle);
            materialIndices.emplace(materialHandle, materialIndex);
        }

        SceneAssetData compiled;
        compiled.m_materials = sourceData.m_materials;
        compiled.m_groupFilters = sourceData.m_groupFilters;
        compiled.m_paths = sourceData.m_paths;
        compiled.m_bodies = sourceData.m_bodies;
        compiled.m_constraints = sourceData.m_constraints;
        compiled.m_name = sourceData.m_name;
        compiled.m_shapes.reserve(sourceData.m_shapes.size());
        compiled.m_softBodyDefinitions.reserve(sourceData.m_softBodyDefinitions.size());

        for (AZ::u32 shapeIndex = 0; shapeIndex < sourceData.m_shapes.size(); ++shapeIndex)
        {
            CookedShapeHandle shapeHandle;
            AZStd::visit(
                [&](const auto& source)
                {
                    using Shape = AZStd::remove_cvref_t<decltype(source)>;
                    if constexpr (AZStd::is_same_v<Shape, SceneSourceShapeData>)
                    {
                        ShapeConfiguration configuration;
                        configuration.m_geometry = source.m_geometry;
                        configuration.m_userData = source.m_userData;
                        configuration.m_density = source.m_density;
                        configuration.m_materials.reserve(source.m_materialIndices.size());
                        for (const AZ::u32 materialIndex : source.m_materialIndices)
                        {
                            if (materialIndex >= materials.size())
                            {
                                return;
                            }
                            configuration.m_materials.push_back(materials[materialIndex]);
                        }
                        shapeHandle = CookShape(configuration);
                    }
                    else if constexpr (AZStd::is_same_v<Shape, SceneSourceCompoundShape>)
                    {
                        CookedCompoundShapeConfiguration configuration;
                        configuration.m_children.reserve(source.m_children.size());
                        for (const SceneSourceCompoundChild& child : source.m_children)
                        {
                            if (child.m_shapeIndex >= shapeIndex)
                            {
                                return;
                            }
                            configuration.m_children.push_back({
                                .m_position = child.m_position,
                                .m_rotation = child.m_rotation,
                                .m_shapeHandle = shapes[child.m_shapeIndex],
                                .m_userData = child.m_userData,
                            });
                        }
                        configuration.m_userData = source.m_userData;
                        shapeHandle = CookShape(configuration);
                    }
                    else if constexpr (AZStd::is_same_v<Shape, SceneSourceOffsetCenterOfMassShape>)
                    {
                        if (source.m_shapeIndex >= shapeIndex)
                        {
                            return;
                        }
                        shapeHandle = CookShape(CookedDecoratedShapeConfiguration{
                            .m_geometry = CookedOffsetCenterOfMassShapeConfiguration{
                                .m_shapeHandle = shapes[source.m_shapeIndex],
                                .m_offset = source.m_offset,
                            },
                            .m_userData = source.m_userData,
                        });
                    }
                    else if constexpr (AZStd::is_same_v<Shape, SceneSourceRotatedTranslatedShape>)
                    {
                        if (source.m_shapeIndex >= shapeIndex)
                        {
                            return;
                        }
                        shapeHandle = CookShape(CookedDecoratedShapeConfiguration{
                            .m_geometry = CookedRotatedTranslatedShapeConfiguration{
                                .m_shapeHandle = shapes[source.m_shapeIndex],
                                .m_position = source.m_position,
                                .m_rotation = source.m_rotation,
                            },
                            .m_userData = source.m_userData,
                        });
                    }
                    else if constexpr (AZStd::is_same_v<Shape, SceneSourceScaledShape>)
                    {
                        if (source.m_shapeIndex >= shapeIndex)
                        {
                            return;
                        }
                        shapeHandle = CookShape(CookedDecoratedShapeConfiguration{
                            .m_geometry = CookedScaledShapeConfiguration{
                                .m_shapeHandle = shapes[source.m_shapeIndex],
                                .m_scale = source.m_scale,
                            },
                            .m_userData = source.m_userData,
                        });
                    }
                },
                sourceData.m_shapes[shapeIndex]);
            if (!shapeHandle)
            {
                releaseSourceResources();
                return false;
            }

            shapes.push_back(shapeHandle);
            shapeIndices.emplace(shapeHandle, shapeIndex);
            SceneAssetShape compiledShape;
            AZStd::vector<MaterialHandle> shapeMaterials;
            AZStd::vector<CookedShapeHandle> childShapes;
            if (!ExportShape(shapeHandle, compiledShape.m_archive, shapeMaterials, childShapes))
            {
                releaseSourceResources();
                return false;
            }

            compiledShape.m_materialIndices.reserve(shapeMaterials.size());
            for (const MaterialHandle materialHandle : shapeMaterials)
            {
                if (!materialHandle)
                {
                    compiledShape.m_materialIndices.push_back(InvalidAssetIndex);
                    continue;
                }
                const auto material = materialIndices.find(materialHandle);
                if (material == materialIndices.end())
                {
                    releaseSourceResources();
                    return false;
                }
                compiledShape.m_materialIndices.push_back(material->second);
            }

            compiledShape.m_childShapeIndices.reserve(childShapes.size());
            for (const CookedShapeHandle childShape : childShapes)
            {
                const auto child = shapeIndices.find(childShape);
                if (child == shapeIndices.end() || child->second >= shapeIndex)
                {
                    releaseSourceResources();
                    return false;
                }
                compiledShape.m_childShapeIndices.push_back(child->second);
            }
            compiled.m_shapes.push_back(AZStd::move(compiledShape));
        }

        for (const SceneSourceSoftBodyDefinition& source : sourceData.m_softBodyDefinitions)
        {
            SoftBodyDefinitionConfiguration configuration;
            configuration.m_vertices = source.m_vertices;
            configuration.m_faces = source.m_faces;
            configuration.m_vertexAttributes = source.m_vertexAttributes;
            configuration.m_edgeConstraints = source.m_edgeConstraints;
            configuration.m_dihedralBendConstraints = source.m_dihedralBendConstraints;
            configuration.m_longRangeConstraints = source.m_longRangeConstraints;
            configuration.m_rodStretchShearConstraints = source.m_rodStretchShearConstraints;
            configuration.m_rodBendTwistConstraints = source.m_rodBendTwistConstraints;
            configuration.m_volumeConstraints = source.m_volumeConstraints;
            configuration.m_inverseBinds = source.m_inverseBinds;
            configuration.m_skinConstraints = source.m_skinConstraints;
            configuration.m_shearAngleTolerance = source.m_shearAngleTolerance;
            configuration.m_bendType = source.m_bendType;
            configuration.m_createFaceConstraints = source.m_createFaceConstraints;
            configuration.m_optimize = source.m_optimize;
            configuration.m_materials.reserve(source.m_materialIndices.size());
            for (const AZ::u32 materialIndex : source.m_materialIndices)
            {
                if (materialIndex >= materials.size())
                {
                    releaseSourceResources();
                    return false;
                }
                configuration.m_materials.push_back(materials[materialIndex]);
            }

            const SoftBodyDefinitionHandle definitionHandle = CreateSoftBodyDefinition(configuration);
            if (!definitionHandle)
            {
                releaseSourceResources();
                return false;
            }

            SceneAssetSoftBodyDefinition compiledDefinition;
            AZStd::vector<MaterialHandle> definitionMaterials;
            const bool exported = ExportSoftBodyDefinition(
                definitionHandle,
                compiledDefinition.m_archive,
                definitionMaterials);
            [[maybe_unused]] const bool destroyed = DestroySoftBodyDefinition(definitionHandle);
            AZ_Assert(destroyed, "Compiled soft body definition destruction failed.");
            if (!exported || !destroyed)
            {
                releaseSourceResources();
                return false;
            }

            compiledDefinition.m_materialIndices.reserve(definitionMaterials.size());
            for (const MaterialHandle materialHandle : definitionMaterials)
            {
                const auto material = materialIndices.find(materialHandle);
                if (material == materialIndices.end())
                {
                    releaseSourceResources();
                    return false;
                }
                compiledDefinition.m_materialIndices.push_back(material->second);
            }
            compiled.m_softBodyDefinitions.push_back(AZStd::move(compiledDefinition));
        }

        releaseSourceResources();
        const SceneDefinitionHandle validationHandle = CreateSceneDefinition(compiled);
        if (!validationHandle)
        {
            return false;
        }
        [[maybe_unused]] const bool destroyed = DestroySceneDefinition(validationHandle);
        AZ_Assert(destroyed, "Compiled scene asset validation cleanup failed.");
        if (!destroyed)
        {
            return false;
        }

        assetData = AZStd::move(compiled);
        return true;
    }

    SceneDefinitionHandle RuntimeImplementation::CreateSceneDefinition(
        const SceneAssetData& assetData)
    {
        JOLT_PROFILE_SCOPE(Physics, "Jolt::RuntimeImplementation::CreateSceneDefinitionFromAsset");
        AZStd::vector<MaterialHandle> materials;
        AZStd::vector<CookedShapeHandle> shapes;
        AZStd::vector<GroupFilterHandle> groupFilters;
        AZStd::vector<PathHandle> paths;
        AZStd::vector<SoftBodyDefinitionHandle> softBodyDefinitions;
        materials.reserve(assetData.m_materials.size());
        shapes.reserve(assetData.m_shapes.size());
        groupFilters.reserve(assetData.m_groupFilters.size());
        paths.reserve(assetData.m_paths.size());
        softBodyDefinitions.reserve(assetData.m_softBodyDefinitions.size());

        const auto rollback = [&]()
        {
            for (auto definition = softBodyDefinitions.rbegin(); definition != softBodyDefinitions.rend(); ++definition)
            {
                [[maybe_unused]] const bool destroyed = DestroySoftBodyDefinition(*definition);
                AZ_Assert(destroyed, "Scene asset soft body definition rollback failed.");
            }
            for (auto shape = shapes.rbegin(); shape != shapes.rend(); ++shape)
            {
                [[maybe_unused]] const bool destroyed = DestroyCookedShape(*shape);
                AZ_Assert(destroyed, "Scene asset shape rollback failed.");
            }
            for (auto path = paths.rbegin(); path != paths.rend(); ++path)
            {
                [[maybe_unused]] const bool destroyed = DestroyPath(*path);
                AZ_Assert(destroyed, "Scene asset path rollback failed.");
            }
            for (auto filter = groupFilters.rbegin(); filter != groupFilters.rend(); ++filter)
            {
                [[maybe_unused]] const bool destroyed = DestroyGroupFilter(*filter);
                AZ_Assert(destroyed, "Scene asset group filter rollback failed.");
            }
            for (auto material = materials.rbegin(); material != materials.rend(); ++material)
            {
                [[maybe_unused]] const bool destroyed = DestroyMaterial(*material);
                AZ_Assert(destroyed, "Scene asset material rollback failed.");
            }
        };

        for (const MaterialConfiguration& configuration : assetData.m_materials)
        {
            const MaterialHandle materialHandle = CreateMaterial(configuration);
            if (!materialHandle)
            {
                rollback();
                return {};
            }
            materials.push_back(materialHandle);
        }

        for (const GroupFilterTableConfiguration& configuration : assetData.m_groupFilters)
        {
            const GroupFilterHandle filterHandle = CreateGroupFilterTable(configuration);
            if (!filterHandle)
            {
                rollback();
                return {};
            }
            groupFilters.push_back(filterHandle);
        }

        for (const HermitePathConfiguration& configuration : assetData.m_paths)
        {
            const PathHandle pathHandle = CreatePath(configuration);
            if (!pathHandle)
            {
                rollback();
                return {};
            }
            paths.push_back(pathHandle);
        }

        for (AZ::u32 shapeIndex = 0; shapeIndex < assetData.m_shapes.size(); ++shapeIndex)
        {
            const SceneAssetShape& source = assetData.m_shapes[shapeIndex];
            AZStd::vector<MaterialHandle> shapeMaterials;
            shapeMaterials.reserve(source.m_materialIndices.size());
            for (const AZ::u32 materialIndex : source.m_materialIndices)
            {
                if (materialIndex == InvalidAssetIndex)
                {
                    shapeMaterials.push_back(MaterialHandle::Invalid);
                }
                else if (materialIndex < materials.size())
                {
                    shapeMaterials.push_back(materials[materialIndex]);
                }
                else
                {
                    rollback();
                    return {};
                }
            }

            AZStd::vector<CookedShapeHandle> childShapes;
            childShapes.reserve(source.m_childShapeIndices.size());
            for (const AZ::u32 childShapeIndex : source.m_childShapeIndices)
            {
                if (childShapeIndex >= shapeIndex)
                {
                    rollback();
                    return {};
                }
                childShapes.push_back(shapes[childShapeIndex]);
            }

            const CookedShapeHandle shapeHandle = ImportShape(
                source.m_archive,
                shapeMaterials,
                childShapes);
            if (!shapeHandle)
            {
                rollback();
                return {};
            }
            shapes.push_back(shapeHandle);
        }

        for (const SceneAssetSoftBodyDefinition& source : assetData.m_softBodyDefinitions)
        {
            AZStd::vector<MaterialHandle> definitionMaterials;
            definitionMaterials.reserve(source.m_materialIndices.size());
            for (const AZ::u32 materialIndex : source.m_materialIndices)
            {
                if (materialIndex >= materials.size())
                {
                    rollback();
                    return {};
                }
                definitionMaterials.push_back(materials[materialIndex]);
            }

            const SoftBodyDefinitionHandle definitionHandle = ImportSoftBodyDefinition(
                source.m_archive,
                definitionMaterials);
            if (!definitionHandle)
            {
                rollback();
                return {};
            }
            softBodyDefinitions.push_back(definitionHandle);
        }

        SceneConfiguration configuration;
        configuration.m_name = assetData.m_name;
        configuration.m_bodies.reserve(assetData.m_bodies.size());
        for (const SceneAssetBody& sourceBody : assetData.m_bodies)
        {
            bool resolved = true;
            AZStd::visit(
                [&](const auto& source)
                {
                    using Body = AZStd::remove_cvref_t<decltype(source)>;
                    if constexpr (AZStd::is_same_v<Body, SceneAssetRigidBody>)
                    {
                        if (source.m_shapeIndex >= shapes.size())
                        {
                            resolved = false;
                            return;
                        }
                        GroupFilterHandle filterHandle;
                        CollisionGroupConfiguration collisionGroup;
                        if (!ResolveGroupFilter(source.m_groupFilterIndex, groupFilters, collisionGroup))
                        {
                            resolved = false;
                            return;
                        }
                        filterHandle = collisionGroup.m_filterHandle;
                        configuration.m_bodies.emplace_back(
                            ResolveRigidBody(source, shapes[source.m_shapeIndex], filterHandle));
                    }
                    else
                    {
                        if (source.m_definitionIndex >= softBodyDefinitions.size())
                        {
                            resolved = false;
                            return;
                        }
                        GroupFilterHandle filterHandle;
                        CollisionGroupConfiguration collisionGroup;
                        if (!ResolveGroupFilter(source.m_groupFilterIndex, groupFilters, collisionGroup))
                        {
                            resolved = false;
                            return;
                        }
                        filterHandle = collisionGroup.m_filterHandle;
                        configuration.m_bodies.emplace_back(
                            ResolveSoftBody(source, softBodyDefinitions[source.m_definitionIndex], filterHandle));
                    }
                },
                sourceBody);
            if (!resolved)
            {
                rollback();
                return {};
            }
        }

        configuration.m_constraints.reserve(assetData.m_constraints.size());
        for (const SceneAssetConstraint& source : assetData.m_constraints)
        {
            ConstraintConfiguration constraint;
            if (!ResolveConstraintGeometry(source, paths, constraint.m_geometry))
            {
                rollback();
                return {};
            }
            constraint.m_entityId = source.m_entityId;
            constraint.m_name = source.m_name;
            constraint.m_priority = source.m_priority;
            constraint.m_positionStepCount = source.m_positionStepCount;
            constraint.m_velocityStepCount = source.m_velocityStepCount;
            constraint.m_enabled = source.m_enabled;
            configuration.m_constraints.push_back({
                .m_constraint = AZStd::move(constraint),
                .m_firstBodyIndex = source.m_firstBodyIndex,
                .m_secondBodyIndex = source.m_secondBodyIndex,
                .m_firstDependencyIndex = source.m_firstDependencyIndex,
                .m_secondDependencyIndex = source.m_secondDependencyIndex,
            });
        }

        const SceneDefinitionHandle definitionHandle = CreateSceneDefinition(configuration);
        if (!definitionHandle)
        {
            rollback();
            return {};
        }

        {
            AZStd::lock_guard lock(m_sceneDefinitionMutex);
            SceneDefinitionSlot* slot = FindSceneDefinitionUnlocked(definitionHandle);
            AZ_Assert(slot, "A newly created scene definition could not be resolved.");
            slot->m_ownedMaterialHandles = AZStd::move(materials);
            slot->m_ownedCookedShapeHandles = AZStd::move(shapes);
            slot->m_ownedGroupFilterHandles = AZStd::move(groupFilters);
            slot->m_ownedPathHandles = AZStd::move(paths);
            slot->m_ownedSoftBodyDefinitionHandles = AZStd::move(softBodyDefinitions);
        }
        return definitionHandle;
    }
} // namespace Jolt
