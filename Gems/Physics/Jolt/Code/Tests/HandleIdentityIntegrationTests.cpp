/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/SystemInternal.h>

#include <AzCore/Interface/Interface.h>
#include <AzCore/Name/NameDictionary.h>
#include <AzCore/UnitTest/UnitTest.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/span.h>
#include <AzTest/AzTest.h>

namespace Jolt
{
    namespace
    {
        inline constexpr AZ::TypeId HandleIdentityStepListenerTypeId{"{782FC66F-B65C-4D64-BC57-E4C78DB98236}"};

        class HandleIdentityStepListener final
            : public IStepListener
        {
        public:
            [[nodiscard]]
            AZ::TypeId GetStateTypeId() const override
            {
                return HandleIdentityStepListenerTypeId;
            }

            [[nodiscard]]
            AZ::u64 GetStateHash() const override
            {
                return 0;
            }

            void OnStep(
                [[maybe_unused]] const StepInformation& information,
                [[maybe_unused]] IStepContext& context) override
            {
            }
        };

        class HandleIdentityNameDictionaryScope final
        {
        public:
            HandleIdentityNameDictionaryScope()
            {
                if (!AZ::Interface<AZ::NameDictionary>::Get())
                {
                    AZ::NameDictionary::Create();
                    m_created = true;
                }
            }

            ~HandleIdentityNameDictionaryScope()
            {
                if (m_created)
                {
                    AZ::NameDictionary::Destroy();
                }
            }

            AZ_DISABLE_COPY_MOVE(HandleIdentityNameDictionaryScope);

        private:
            bool m_created = false;
        };

        struct HandleIdentityResources final
        {
            BodyHandle m_body;
            CharacterHandle m_character;
            ConstraintHandle m_constraint;
            CookedShapeHandle m_cookedShape;
            ExtensionHandle m_extension;
            GroupFilterHandle m_groupFilter;
            HairDefinitionHandle m_hairDefinition;
            HairHandle m_hair;
            MaterialHandle m_material;
            PathHandle m_path;
            RagdollDefinitionHandle m_ragdollDefinition;
            RagdollHandle m_ragdoll;
            SceneDefinitionHandle m_sceneDefinition;
            SceneInstanceHandle m_sceneInstance;
            ShapeHandle m_shape;
            SkeletalAnimationHandle m_skeletalAnimation;
            SkeletonDefinitionHandle m_skeletonDefinition;
            SkeletonMapperHandle m_skeletonMapper;
            SkeletonPoseHandle m_skeletonPose;
            SoftBodyDefinitionHandle m_softBodyDefinition;
            StateSnapshotHandle m_stateSnapshot;
            VehicleHandle m_vehicle;
            VirtualCharacterHandle m_virtualCharacter;
            WorldHandle m_world;

            EventBatch m_events;
            RaycastHit m_raycastHit;
            TransformedShape m_transformedShape;
            MaterialHandle m_shapeMaterial;
            SkeletonDefinitionHandle m_targetSkeletonDefinition;
            BodyHandle m_secondBody;
        };

        struct HandleIdentityCounts final
        {
            AZ::u32 m_bodyCount = 0;
            AZ::u32 m_characterCount = 0;
            AZ::u32 m_constraintCount = 0;
            AZ::u32 m_hairCount = 0;
            AZ::u32 m_ragdollCount = 0;
            AZ::u32 m_sceneInstanceCount = 0;
            AZ::u32 m_shapeCount = 0;
            AZ::u32 m_stateSnapshotCount = 0;
            AZ::u32 m_vehicleCount = 0;
            AZ::u32 m_virtualCharacterCount = 0;

            friend bool operator==(const HandleIdentityCounts&, const HandleIdentityCounts&) = default;
        };

        [[nodiscard]]
        SystemConfiguration CreateHandleIdentitySystemConfiguration()
        {
            SystemConfiguration configuration;
            configuration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
            configuration.m_defaultWorld.m_workerCount = 1;
            return configuration;
        }

        [[nodiscard]]
        HairDefinitionConfiguration CreateHandleIdentityHairDefinition()
        {
            HairDefinitionConfiguration configuration;
            configuration.m_vertices = {
                {.m_position = AZ::Vector3(0.0f, 0.0f, 1.0f), .m_inverseMass = 0.0f},
                {.m_position = AZ::Vector3::CreateZero()},
            };
            configuration.m_strands = {
                {.m_beginVertex = 0, .m_endVertex = 2, .m_materialIndex = 0},
            };
            configuration.m_materials.resize(1);
            configuration.m_scalpVertices = {
                AZ::Vector3(-1.0f, -1.0f, 1.0f),
                AZ::Vector3(1.0f, -1.0f, 1.0f),
                AZ::Vector3(0.0f, 1.0f, 1.0f),
            };
            configuration.m_scalpTriangles = {
                {.m_firstVertex = 0, .m_secondVertex = 1, .m_thirdVertex = 2},
            };
            configuration.m_scalpInverseBindPoses = {AZ::Transform::CreateIdentity()};
            configuration.m_scalpSkinWeights = {
                {.m_jointIndex = 0, .m_weight = 1.0f},
                {.m_jointIndex = 0, .m_weight = 1.0f},
                {.m_jointIndex = 0, .m_weight = 1.0f},
            };
            configuration.m_scalpSkinWeightsPerVertex = 1;
            configuration.m_gridSizeX = 2;
            configuration.m_gridSizeY = 2;
            configuration.m_gridSizeZ = 2;
            return configuration;
        }

        [[nodiscard]]
        SoftBodyDefinitionConfiguration CreateHandleIdentitySoftBodyDefinition()
        {
            SoftBodyDefinitionConfiguration configuration;
            configuration.m_vertices = {
                {.m_position = AZ::Vector3(-0.5f, 0.0f, 0.0f)},
                {.m_position = AZ::Vector3(0.5f, 0.0f, 0.0f)},
                {.m_position = AZ::Vector3(0.0f, 1.0f, 0.0f)},
            };
            configuration.m_faces = {
                {.m_firstVertex = 0, .m_secondVertex = 1, .m_thirdVertex = 2},
            };
            configuration.m_edgeConstraints = {
                {.m_firstVertex = 0, .m_secondVertex = 1},
                {.m_firstVertex = 1, .m_secondVertex = 2},
                {.m_firstVertex = 2, .m_secondVertex = 0},
            };
            return configuration;
        }

        [[nodiscard]]
        HandleIdentityResources CreateHandleIdentityResources(
            Runtime& runtime,
            HandleIdentityStepListener& stepListener,
            const AZ::u64 tag)
        {
            HandleIdentityResources resources;
            resources.m_world = runtime.GetDefaultWorldHandle();

            const ExtensionRegistrationResult extensionRegistration = runtime.RegisterExtension(&stepListener, {});
            resources.m_extension = extensionRegistration.m_handle;

            MaterialConfiguration materialConfiguration;
            materialConfiguration.m_debugName = "Identity";
            resources.m_material = runtime.CreateMaterial(materialConfiguration);
            materialConfiguration.m_debugName = "Shape";
            resources.m_shapeMaterial = runtime.CreateMaterial(materialConfiguration);

            ShapeConfiguration shapeConfiguration;
            shapeConfiguration.m_geometry = BoxShapeConfiguration{};
            shapeConfiguration.m_materials = {resources.m_shapeMaterial};
            shapeConfiguration.m_userData = tag;
            resources.m_cookedShape = runtime.CookShape(shapeConfiguration);

            GroupFilterTableConfiguration groupFilterConfiguration;
            groupFilterConfiguration.m_subGroupCount = 2;
            resources.m_groupFilter = runtime.CreateGroupFilterTable(groupFilterConfiguration);

            resources.m_path = runtime.CreatePath(HermitePathConfiguration::CreateDefault());

            SkeletonDefinitionConfiguration skeletonConfiguration;
            skeletonConfiguration.m_joints = {{.m_name = AZ::Name("root"), .m_parentIndex = -1}};
            resources.m_skeletonDefinition = runtime.CreateSkeletonDefinition(skeletonConfiguration);

            SkeletonDefinitionConfiguration targetSkeletonConfiguration;
            targetSkeletonConfiguration.m_joints = {{.m_name = AZ::Name("target_root"), .m_parentIndex = -1}};
            resources.m_targetSkeletonDefinition = runtime.CreateSkeletonDefinition(targetSkeletonConfiguration);

            SkeletalAnimationConfiguration animationConfiguration;
            animationConfiguration.m_joints = {
                {
                    .m_name = AZ::Name("root"),
                    .m_keyframes = {{}},
                },
            };
            resources.m_skeletalAnimation = runtime.CreateSkeletalAnimation(animationConfiguration);
            resources.m_skeletonPose = runtime.CreateSkeletonPose(resources.m_skeletonDefinition);

            SkeletonMapperConfiguration mapperConfiguration;
            mapperConfiguration.m_sourceSkeletonHandle = resources.m_skeletonDefinition;
            mapperConfiguration.m_targetSkeletonHandle = resources.m_targetSkeletonDefinition;
            mapperConfiguration.m_sourceNeutralModelTransforms = {AZ::Transform::CreateIdentity()};
            mapperConfiguration.m_targetNeutralModelTransforms = {AZ::Transform::CreateIdentity()};
            mapperConfiguration.m_jointMappings = {{.m_sourceJoint = 0, .m_targetJoint = 0}};
            resources.m_skeletonMapper = runtime.CreateSkeletonMapper(mapperConfiguration);

            resources.m_softBodyDefinition = runtime.CreateSoftBodyDefinition(CreateHandleIdentitySoftBodyDefinition());
            resources.m_hairDefinition = runtime.CreateHairDefinition(CreateHandleIdentityHairDefinition());

            SceneRigidBodyConfiguration sceneBodyConfiguration;
            sceneBodyConfiguration.m_cookedShapeHandle = resources.m_cookedShape;
            sceneBodyConfiguration.m_body.m_motionType = MotionType::Static;
            sceneBodyConfiguration.m_body.m_objectLayer = DefaultLayers::NonMoving;
            SceneConfiguration sceneConfiguration;
            sceneConfiguration.m_bodies = {sceneBodyConfiguration};
            resources.m_sceneDefinition = runtime.CreateSceneDefinition(sceneConfiguration);

            resources.m_shape = runtime.CreateShape(resources.m_world, resources.m_cookedShape);

            BodyConfiguration bodyConfiguration;
            bodyConfiguration.m_shapeHandle = resources.m_shape;
            bodyConfiguration.m_transform.m_position.m_x = -2.0;
            bodyConfiguration.m_userData = tag;
            resources.m_body = runtime.CreateBody(resources.m_world, bodyConfiguration);

            bodyConfiguration.m_transform.m_position.m_x = 2.0;
            bodyConfiguration.m_userData = tag + 1;
            resources.m_secondBody = runtime.CreateBody(resources.m_world, bodyConfiguration);

            ShapeCollectionRequest shapeCollectionRequest;
            shapeCollectionRequest.m_bounds = {
                .m_center = WorldPosition(-2.0, 0.0, 0.0),
                .m_halfExtents = AZ::Vector3(1.0f),
            };
            AZStd::array<TransformedShape, 1> transformedShapes;
            const QueryResult shapeCollectionResult =
                runtime.CollectShapesInBounds(resources.m_world, shapeCollectionRequest, transformedShapes);
            EXPECT_TRUE(shapeCollectionResult.IsComplete());
            EXPECT_EQ(shapeCollectionResult.m_hitCount, 1);
            resources.m_transformedShape = AZStd::move(transformedShapes.front());

            RaycastRequest raycastRequest;
            raycastRequest.m_start = WorldPosition(-2.0, 0.0, 5.0);
            raycastRequest.m_displacement = AZ::Vector3(0.0f, 0.0f, -10.0f);
            EXPECT_TRUE(runtime.RaycastClosest(resources.m_world, raycastRequest, resources.m_raycastHit));

            ConstraintConfiguration constraintConfiguration;
            constraintConfiguration.m_firstBodyHandle = resources.m_body;
            constraintConfiguration.m_secondBodyHandle = resources.m_secondBody;
            constraintConfiguration.m_geometry = PointConstraintConfiguration{};
            constraintConfiguration.m_userData = tag;
            resources.m_constraint = runtime.CreateConstraint(resources.m_world, constraintConfiguration);

            EXPECT_TRUE(runtime.SetBodyMoveEventsEnabled(resources.m_world, resources.m_body, true));
            EXPECT_TRUE(runtime.SetBodyLinearVelocity(resources.m_world, resources.m_body, AZ::Vector3::CreateAxisY()));
            EXPECT_TRUE(runtime.StepWorld(resources.m_world, 1.0f / 60.0f));
            resources.m_events = runtime.GetEvents(resources.m_world);

            CharacterConfiguration characterConfiguration;
            characterConfiguration.m_shapeHandle = resources.m_shape;
            characterConfiguration.m_transform.m_position = WorldPosition(0.0, 3.0, 0.0);
            characterConfiguration.m_userData = tag;
            resources.m_character = runtime.CreateCharacter(resources.m_world, characterConfiguration);

            VirtualCharacterConfiguration virtualCharacterConfiguration;
            virtualCharacterConfiguration.m_shapeHandle = resources.m_shape;
            virtualCharacterConfiguration.m_transform.m_position = WorldPosition(0.0, -3.0, 0.0);
            virtualCharacterConfiguration.m_userData = tag;
            resources.m_virtualCharacter = runtime.CreateVirtualCharacter(resources.m_world, virtualCharacterConfiguration);

            WheeledVehicleConfiguration vehicleConfiguration;
            vehicleConfiguration.m_bodyHandle = resources.m_secondBody;
            vehicleConfiguration.m_wheels = {
                {.m_position = AZ::Vector3(-0.8f, 1.4f, -0.25f), .m_maximumHandBrakeTorque = 0.0f},
                {.m_position = AZ::Vector3(0.8f, 1.4f, -0.25f), .m_maximumHandBrakeTorque = 0.0f},
                {.m_position = AZ::Vector3(-0.8f, -1.4f, -0.25f), .m_maximumSteerAngle = 0.0f},
                {.m_position = AZ::Vector3(0.8f, -1.4f, -0.25f), .m_maximumSteerAngle = 0.0f},
            };
            vehicleConfiguration.m_differentials = {{.m_leftWheel = 0, .m_rightWheel = 1}};
            resources.m_vehicle = runtime.CreateWheeledVehicle(resources.m_world, vehicleConfiguration);

            RagdollDefinitionConfiguration ragdollDefinitionConfiguration;
            ragdollDefinitionConfiguration.m_skeletonHandle = resources.m_skeletonDefinition;
            ragdollDefinitionConfiguration.m_parts.resize(1);
            ragdollDefinitionConfiguration.m_parts.front().m_body.m_shapeHandle = resources.m_shape;
            resources.m_ragdollDefinition = runtime.CreateRagdollDefinition(resources.m_world, ragdollDefinitionConfiguration);

            RagdollConfiguration ragdollConfiguration;
            ragdollConfiguration.m_definitionHandle = resources.m_ragdollDefinition;
            ragdollConfiguration.m_rootPosition = WorldPosition(0.0, 6.0, 0.0);
            resources.m_ragdoll = runtime.CreateRagdoll(resources.m_world, ragdollConfiguration);

            HairConfiguration hairConfiguration;
            hairConfiguration.m_definitionHandle = resources.m_hairDefinition;
            hairConfiguration.m_objectLayer = DefaultLayers::Moving;
            hairConfiguration.m_worldTransform.m_position = WorldPosition(0.0, -6.0, 0.0);
            resources.m_hair = runtime.CreateHair(resources.m_world, hairConfiguration);

            resources.m_sceneInstance = runtime.InstantiateScene(resources.m_world, resources.m_sceneDefinition);
            resources.m_stateSnapshot = runtime.CaptureWorldState(resources.m_world);
            return resources;
        }

        void AssertHandleIdentityResourcesValid(
            Runtime& runtime,
            const HandleIdentityResources& resources)
        {
            ASSERT_TRUE(resources.m_body);
            ASSERT_TRUE(resources.m_character);
            ASSERT_TRUE(resources.m_constraint);
            ASSERT_TRUE(resources.m_cookedShape);
            ASSERT_TRUE(resources.m_extension);
            ASSERT_TRUE(resources.m_groupFilter);
            ASSERT_TRUE(resources.m_hairDefinition);
            ASSERT_TRUE(resources.m_hair);
            ASSERT_TRUE(resources.m_material);
            ASSERT_TRUE(resources.m_path);
            ASSERT_TRUE(resources.m_ragdollDefinition);
            ASSERT_TRUE(resources.m_ragdoll);
            ASSERT_TRUE(resources.m_sceneDefinition);
            ASSERT_TRUE(resources.m_sceneInstance);
            ASSERT_TRUE(resources.m_shape);
            ASSERT_TRUE(resources.m_skeletalAnimation);
            ASSERT_TRUE(resources.m_skeletonDefinition);
            ASSERT_TRUE(resources.m_skeletonMapper);
            ASSERT_TRUE(resources.m_skeletonPose);
            ASSERT_TRUE(resources.m_softBodyDefinition);
            ASSERT_TRUE(resources.m_stateSnapshot);
            ASSERT_TRUE(resources.m_vehicle);
            ASSERT_TRUE(resources.m_virtualCharacter);
            ASSERT_TRUE(resources.m_world);
            ASSERT_TRUE(resources.m_secondBody);
            ASSERT_TRUE(resources.m_shapeMaterial);
            ASSERT_TRUE(resources.m_events);
            ASSERT_TRUE(resources.m_transformedShape);
            ASSERT_TRUE(resources.m_raycastHit.m_bodyHandle);
            ASSERT_TRUE(resources.m_raycastHit.m_materialHandle);
            ASSERT_TRUE(resources.m_raycastHit.m_shapeHandle);
            EXPECT_EQ(resources.m_raycastHit.m_bodyHandle, resources.m_body);
            EXPECT_EQ(resources.m_raycastHit.m_materialHandle, resources.m_shapeMaterial);
            EXPECT_EQ(resources.m_raycastHit.m_shapeHandle, resources.m_shape);
            EXPECT_EQ(resources.m_transformedShape.GetBodyHandle(), resources.m_body);
            EXPECT_EQ(resources.m_transformedShape.GetMaterialHandle(), resources.m_shapeMaterial);
            EXPECT_EQ(resources.m_transformedShape.GetShapeHandle(), resources.m_shape);

            const AZStd::span<const BodyMoveEvent> bodyMoves = resources.m_events.GetBodyMoves();
            ASSERT_FALSE(bodyMoves.empty());
            EXPECT_EQ(bodyMoves.front().m_bodyHandle, resources.m_body);

            EXPECT_TRUE(runtime.IsValid(resources.m_world));
            EXPECT_TRUE(runtime.IsValid(resources.m_material));
            EXPECT_TRUE(runtime.IsValid(resources.m_shapeMaterial));
            EXPECT_TRUE(runtime.IsValid(resources.m_cookedShape));
            EXPECT_TRUE(runtime.IsValid(resources.m_groupFilter));
            EXPECT_TRUE(runtime.IsValid(resources.m_path));
            EXPECT_TRUE(runtime.IsValid(resources.m_skeletonDefinition));
            EXPECT_TRUE(runtime.IsValid(resources.m_skeletalAnimation));
            EXPECT_TRUE(runtime.IsValid(resources.m_skeletonPose));
            EXPECT_TRUE(runtime.IsValid(resources.m_skeletonMapper));
            EXPECT_TRUE(runtime.IsValid(resources.m_softBodyDefinition));
            EXPECT_TRUE(runtime.IsValid(resources.m_hairDefinition));
            EXPECT_TRUE(runtime.IsValid(resources.m_sceneDefinition));
            EXPECT_TRUE(runtime.IsValid(resources.m_world, resources.m_shape));
            EXPECT_TRUE(runtime.IsValid(resources.m_world, resources.m_body));
            EXPECT_TRUE(runtime.IsValid(resources.m_world, resources.m_constraint));
            EXPECT_TRUE(runtime.IsValid(resources.m_world, resources.m_character));
            EXPECT_TRUE(runtime.IsValid(resources.m_world, resources.m_virtualCharacter));
            EXPECT_TRUE(runtime.IsValid(resources.m_world, resources.m_vehicle));
            EXPECT_TRUE(runtime.IsValid(resources.m_world, resources.m_ragdollDefinition));
            EXPECT_TRUE(runtime.IsValid(resources.m_world, resources.m_ragdoll));
            EXPECT_TRUE(runtime.IsValid(resources.m_world, resources.m_hair));
            EXPECT_TRUE(runtime.IsValid(resources.m_world, resources.m_sceneInstance));
            EXPECT_TRUE(runtime.IsValid(resources.m_world, resources.m_stateSnapshot));
        }

        [[nodiscard]]
        HandleIdentityCounts GetHandleIdentityCounts(
            Runtime& runtime,
            const WorldHandle worldHandle)
        {
            WorldStatistics statistics;
            if (!runtime.GetWorldStatistics(worldHandle, statistics))
            {
                ADD_FAILURE() << "The target world must remain valid while checking handle isolation.";
                return {};
            }

            return {
                .m_bodyCount = statistics.m_bodyCount,
                .m_characterCount = statistics.m_characterCount,
                .m_constraintCount = statistics.m_constraintCount,
                .m_hairCount = statistics.m_hairCount,
                .m_ragdollCount = statistics.m_ragdollCount,
                .m_sceneInstanceCount = statistics.m_sceneInstanceCount,
                .m_shapeCount = statistics.m_shapeCount,
                .m_stateSnapshotCount = statistics.m_stateSnapshotCount,
                .m_vehicleCount = statistics.m_vehicleCount,
                .m_virtualCharacterCount = statistics.m_virtualCharacterCount,
            };
        }

        void ExpectDistinctHandleIdentity(
            const HandleIdentityResources& first,
            const HandleIdentityResources& second)
        {
            EXPECT_NE(first.m_body, second.m_body);
            EXPECT_NE(first.m_character, second.m_character);
            EXPECT_NE(first.m_constraint, second.m_constraint);
            EXPECT_NE(first.m_cookedShape, second.m_cookedShape);
            EXPECT_NE(first.m_extension, second.m_extension);
            EXPECT_NE(first.m_groupFilter, second.m_groupFilter);
            EXPECT_NE(first.m_hairDefinition, second.m_hairDefinition);
            EXPECT_NE(first.m_hair, second.m_hair);
            EXPECT_NE(first.m_material, second.m_material);
            EXPECT_NE(first.m_path, second.m_path);
            EXPECT_NE(first.m_ragdollDefinition, second.m_ragdollDefinition);
            EXPECT_NE(first.m_ragdoll, second.m_ragdoll);
            EXPECT_NE(first.m_sceneDefinition, second.m_sceneDefinition);
            EXPECT_NE(first.m_sceneInstance, second.m_sceneInstance);
            EXPECT_NE(first.m_shape, second.m_shape);
            EXPECT_NE(first.m_skeletalAnimation, second.m_skeletalAnimation);
            EXPECT_NE(first.m_skeletonDefinition, second.m_skeletonDefinition);
            EXPECT_NE(first.m_skeletonMapper, second.m_skeletonMapper);
            EXPECT_NE(first.m_skeletonPose, second.m_skeletonPose);
            EXPECT_NE(first.m_softBodyDefinition, second.m_softBodyDefinition);
            EXPECT_NE(first.m_stateSnapshot, second.m_stateSnapshot);
            EXPECT_NE(first.m_vehicle, second.m_vehicle);
            EXPECT_NE(first.m_virtualCharacter, second.m_virtualCharacter);
            EXPECT_NE(first.m_world, second.m_world);
            EXPECT_NE(first.m_shapeMaterial, second.m_shapeMaterial);
        }

        void ExpectForeignHandlesRejectedWithoutMutation(
            Runtime& target,
            const HandleIdentityResources& foreign,
            const HandleIdentityResources& current,
            const AZ::u64 currentTag)
        {
            const HandleIdentityCounts countsBefore = GetHandleIdentityCounts(target, current.m_world);

            ExtensionInformation extensionInformation;
            EXPECT_FALSE(target.GetExtensionInformation(foreign.m_extension, extensionInformation));
            EXPECT_NE(target.UnregisterExtension(foreign.m_extension), ExtensionRegistrationStatus::Success);
            EXPECT_TRUE(target.GetExtensionInformation(current.m_extension, extensionInformation));

            EXPECT_FALSE(target.DestroyMaterial(foreign.m_material));
            EXPECT_TRUE(target.IsValid(current.m_material));

            ShapeStats shapeStats;
            EXPECT_FALSE(target.GetStats(foreign.m_cookedShape, shapeStats));
            EXPECT_TRUE(target.GetStats(current.m_cookedShape, shapeStats));

            bool collisionEnabled = false;
            EXPECT_FALSE(target.SetSubGroupCollisionEnabled(
                foreign.m_groupFilter,
                CollisionSubGroupId(0),
                CollisionSubGroupId(1),
                false));
            EXPECT_TRUE(target.GetSubGroupCollisionEnabled(
                current.m_groupFilter,
                CollisionSubGroupId(0),
                CollisionSubGroupId(1),
                collisionEnabled));
            EXPECT_TRUE(collisionEnabled);

            PathState pathState;
            EXPECT_FALSE(target.GetPathState(foreign.m_path, pathState));
            EXPECT_TRUE(target.GetPathState(current.m_path, pathState));

            AZStd::array<SkeletonJoint, 1> joints;
            EXPECT_EQ(target.GetSkeletonJoints(foreign.m_skeletonDefinition, joints).m_requiredHitCount, 0);
            EXPECT_TRUE(target.GetSkeletonJoints(current.m_skeletonDefinition, joints).IsComplete());

            SkeletalAnimationState animationState;
            EXPECT_FALSE(target.SetSkeletalAnimationLooping(foreign.m_skeletalAnimation, false));
            ASSERT_TRUE(target.GetSkeletalAnimationState(current.m_skeletalAnimation, animationState));
            EXPECT_TRUE(animationState.m_isLooping);

            SkeletonPoseState poseState;
            EXPECT_FALSE(target.SetSkeletonPoseRootOffset(foreign.m_skeletonPose, WorldPosition(100.0, 200.0, 300.0)));
            ASSERT_TRUE(target.GetSkeletonPoseState(current.m_skeletonPose, poseState));
            EXPECT_EQ(poseState.m_rootOffset.m_x, 0.0);
            EXPECT_EQ(poseState.m_rootOffset.m_y, 0.0);
            EXPECT_EQ(poseState.m_rootOffset.m_z, 0.0);

            SkeletonMapperState mapperState;
            EXPECT_FALSE(target.GetSkeletonMapperState(foreign.m_skeletonMapper, mapperState));
            EXPECT_TRUE(target.GetSkeletonMapperState(current.m_skeletonMapper, mapperState));

            SoftBodyDefinitionState softBodyState;
            EXPECT_FALSE(target.GetSoftBodyDefinitionState(foreign.m_softBodyDefinition, softBodyState));
            EXPECT_TRUE(target.GetSoftBodyDefinitionState(current.m_softBodyDefinition, softBodyState));

            HairDefinitionState hairDefinitionState;
            EXPECT_FALSE(target.GetHairDefinitionState(foreign.m_hairDefinition, hairDefinitionState));
            EXPECT_TRUE(target.GetHairDefinitionState(current.m_hairDefinition, hairDefinitionState));

            SceneDefinitionState sceneDefinitionState;
            EXPECT_FALSE(target.GetSceneDefinitionState(foreign.m_sceneDefinition, sceneDefinitionState));
            EXPECT_TRUE(target.GetSceneDefinitionState(current.m_sceneDefinition, sceneDefinitionState));

            AZ::Vector3 gravity;
            ASSERT_TRUE(target.GetWorldGravity(current.m_world, gravity));
            EXPECT_FALSE(target.SetWorldGravity(foreign.m_world, AZ::Vector3::CreateAxisX(100.0f)));
            AZ::Vector3 gravityAfter;
            ASSERT_TRUE(target.GetWorldGravity(current.m_world, gravityAfter));
            EXPECT_EQ(gravityAfter, gravity);

            AZ::u64 userData = 0;
            EXPECT_FALSE(target.GetShapeUserData(current.m_world, foreign.m_shape, userData));
            ASSERT_TRUE(target.GetShapeUserData(current.m_world, current.m_shape, userData));
            EXPECT_EQ(userData, currentTag);

            EXPECT_FALSE(target.SetBodyUserData(current.m_world, foreign.m_body, currentTag + 100));
            ASSERT_TRUE(target.GetBodyUserData(current.m_world, current.m_body, userData));
            EXPECT_EQ(userData, currentTag);

            EXPECT_FALSE(target.SetConstraintUserData(current.m_world, foreign.m_constraint, currentTag + 100));
            ASSERT_TRUE(target.GetConstraintUserData(current.m_world, current.m_constraint, userData));
            EXPECT_EQ(userData, currentTag);

            EXPECT_FALSE(target.SetCharacterUserData(current.m_world, foreign.m_character, currentTag + 100));
            ASSERT_TRUE(target.GetCharacterUserData(current.m_world, current.m_character, userData));
            EXPECT_EQ(userData, currentTag);

            EXPECT_FALSE(target.SetVirtualCharacterUserData(current.m_world, foreign.m_virtualCharacter, currentTag + 100));
            ASSERT_TRUE(target.GetVirtualCharacterUserData(current.m_world, current.m_virtualCharacter, userData));
            EXPECT_EQ(userData, currentTag);

            EXPECT_FALSE(target.SetWheeledVehicleInput(current.m_world, foreign.m_vehicle, {.m_forward = 1.0f}));
            WheeledVehicleState vehicleState;
            AZStd::array<WheelState, 4> wheels;
            EXPECT_TRUE(target.GetWheeledVehicleState(current.m_world, current.m_vehicle, vehicleState, wheels).IsComplete());

            RagdollState ragdollState;
            ASSERT_TRUE(target.GetRagdollState(current.m_world, current.m_ragdoll, ragdollState));
            const AZ::u32 currentRagdollCollisionGroupId = ragdollState.m_collisionGroupId;
            EXPECT_FALSE(target.SetRagdollCollisionGroupId(current.m_world, foreign.m_ragdoll, 999));
            ASSERT_TRUE(target.GetRagdollState(current.m_world, current.m_ragdoll, ragdollState));
            EXPECT_EQ(ragdollState.m_collisionGroupId, currentRagdollCollisionGroupId);

            AZStd::array<AZ::s32, 1> constraintIndices;
            EXPECT_EQ(
                target.GetRagdollBodyConstraintIndices(
                    current.m_world,
                    foreign.m_ragdollDefinition,
                    constraintIndices).m_requiredHitCount,
                0);
            EXPECT_TRUE(
                target.GetRagdollBodyConstraintIndices(current.m_world, current.m_ragdollDefinition, constraintIndices).IsComplete());

            WorldTransform movedHairTransform;
            movedHairTransform.m_position = WorldPosition(50.0, 0.0, 0.0);
            EXPECT_FALSE(target.SetHairTransform(current.m_world, foreign.m_hair, movedHairTransform, false));
            HairState hairState;
            ASSERT_TRUE(target.GetHairState(current.m_world, current.m_hair, hairState));
            EXPECT_NE(hairState.m_worldTransform.m_position, movedHairTransform.m_position);

            EXPECT_FALSE(target.DestroySceneInstance(current.m_world, foreign.m_sceneInstance));
            EXPECT_TRUE(target.IsValid(current.m_world, current.m_sceneInstance));

            ASSERT_TRUE(target.SetBodyLinearVelocity(current.m_world, current.m_body, AZ::Vector3::CreateAxisZ(7.0f)));
            EXPECT_FALSE(target.RestoreWorldState(current.m_world, foreign.m_stateSnapshot));
            AZ::Vector3 linearVelocity;
            ASSERT_TRUE(target.GetBodyLinearVelocity(current.m_world, current.m_body, linearVelocity));
            EXPECT_EQ(linearVelocity, AZ::Vector3::CreateAxisZ(7.0f));

            BodyState bodyState;
            EXPECT_FALSE(target.GetBodyState(current.m_world, foreign.m_raycastHit.m_bodyHandle, bodyState));
            EXPECT_FALSE(target.GetShapeStats(current.m_world, foreign.m_raycastHit.m_shapeHandle, shapeStats));
            EXPECT_FALSE(target.IsValid(foreign.m_raycastHit.m_materialHandle));

            TransformedShapeRaycastRequest transformedShapeRequest;
            transformedShapeRequest.m_start = WorldPosition(-10.0, 0.0, 0.0);
            transformedShapeRequest.m_displacement = AZ::Vector3::CreateAxisX(20.0f);
            RaycastHit transformedShapeHit;
            EXPECT_FALSE(target.RaycastTransformedShapeClosest(
                current.m_world,
                foreign.m_transformedShape,
                transformedShapeRequest,
                transformedShapeHit));

            const AZStd::span<const BodyMoveEvent> foreignMoves = foreign.m_events.GetBodyMoves();
            ASSERT_FALSE(foreignMoves.empty());
            EXPECT_FALSE(target.GetBodyState(current.m_world, foreignMoves.front().m_bodyHandle, bodyState));

            EXPECT_EQ(GetHandleIdentityCounts(target, current.m_world), countsBefore);
            AssertHandleIdentityResourcesValid(target, current);
        }

        TEST(HandleIdentityIntegrationTests, SimultaneousIsolatedRuntimesRejectEveryForeignHandleFamily)
        {
            HandleIdentityNameDictionaryScope nameDictionaryScope;
            HandleIdentityStepListener firstStepListener;
            HandleIdentityStepListener secondStepListener;
            Runtime first(CreateHandleIdentitySystemConfiguration(), nullptr, SystemRegistration::Isolated);
            Runtime second(CreateHandleIdentitySystemConfiguration(), nullptr, SystemRegistration::Isolated);
            ASSERT_TRUE(first);
            ASSERT_TRUE(second);

            const HandleIdentityResources firstResources = CreateHandleIdentityResources(first, firstStepListener, 101);
            const HandleIdentityResources secondResources = CreateHandleIdentityResources(second, secondStepListener, 202);
            AssertHandleIdentityResourcesValid(first, firstResources);
            AssertHandleIdentityResourcesValid(second, secondResources);
            ExpectDistinctHandleIdentity(firstResources, secondResources);

            ExpectForeignHandlesRejectedWithoutMutation(second, firstResources, secondResources, 202);
            ExpectForeignHandlesRejectedWithoutMutation(first, secondResources, firstResources, 101);
        }

        TEST(HandleIdentityIntegrationTests, SequentialRuntimeReplacementRejectsRetainedHandlesAndResults)
        {
            HandleIdentityNameDictionaryScope nameDictionaryScope;
            HandleIdentityStepListener retiredStepListener;
            HandleIdentityResources retiredResources;
            {
                Runtime retired(CreateHandleIdentitySystemConfiguration(), nullptr, SystemRegistration::Isolated);
                ASSERT_TRUE(retired);
                retiredResources = CreateHandleIdentityResources(retired, retiredStepListener, 303);
                AssertHandleIdentityResourcesValid(retired, retiredResources);
            }

            HandleIdentityStepListener replacementStepListener;
            Runtime replacement(CreateHandleIdentitySystemConfiguration(), nullptr, SystemRegistration::Isolated);
            ASSERT_TRUE(replacement);
            const HandleIdentityResources replacementResources =
                CreateHandleIdentityResources(replacement, replacementStepListener, 404);
            AssertHandleIdentityResourcesValid(replacement, replacementResources);
            ExpectDistinctHandleIdentity(retiredResources, replacementResources);
            ExpectForeignHandlesRejectedWithoutMutation(replacement, retiredResources, replacementResources, 404);
        }

        TEST(HandleIdentityIntegrationTests, SameRuntimeSlotReuseRejectsStaleHandlesWithoutMutatingReplacement)
        {
            HandleIdentityNameDictionaryScope nameDictionaryScope;
            Runtime runtime(CreateHandleIdentitySystemConfiguration(), nullptr, SystemRegistration::Isolated);
            ASSERT_TRUE(runtime);

            const MaterialHandle firstMaterial = runtime.CreateMaterial({.m_debugName = "First"});
            ASSERT_TRUE(firstMaterial);
            ASSERT_TRUE(runtime.DestroyMaterial(firstMaterial));
            const MaterialHandle secondMaterial = runtime.CreateMaterial({.m_debugName = "Second"});
            ASSERT_TRUE(secondMaterial);
            EXPECT_NE(firstMaterial, secondMaterial);
            EXPECT_FALSE(runtime.DestroyMaterial(firstMaterial));
            EXPECT_TRUE(runtime.IsValid(secondMaterial));

            ShapeConfiguration shapeConfiguration;
            shapeConfiguration.m_geometry = SphereShapeConfiguration{};
            const CookedShapeHandle firstCookedShape = runtime.CookShape(shapeConfiguration);
            ASSERT_TRUE(firstCookedShape);
            ASSERT_TRUE(runtime.DestroyCookedShape(firstCookedShape));
            const CookedShapeHandle secondCookedShape = runtime.CookShape(shapeConfiguration);
            ASSERT_TRUE(secondCookedShape);
            EXPECT_NE(firstCookedShape, secondCookedShape);
            EXPECT_FALSE(runtime.DestroyCookedShape(firstCookedShape));
            EXPECT_TRUE(runtime.IsValid(secondCookedShape));

            const WorldHandle firstWorld = runtime.CreateWorld({});
            ASSERT_TRUE(firstWorld);
            ASSERT_TRUE(runtime.DestroyWorld(firstWorld));
            const WorldHandle secondWorld = runtime.CreateWorld({});
            ASSERT_TRUE(secondWorld);
            EXPECT_NE(firstWorld, secondWorld);
            EXPECT_FALSE(runtime.SetWorldGravity(firstWorld, AZ::Vector3::CreateAxisX()));
            EXPECT_TRUE(runtime.IsValid(secondWorld));

            const ShapeHandle firstShape = runtime.CreateShape(secondWorld, shapeConfiguration);
            ASSERT_TRUE(firstShape);
            ASSERT_TRUE(runtime.DestroyShape(secondWorld, firstShape));
            const ShapeHandle secondShape = runtime.CreateShape(secondWorld, shapeConfiguration);
            ASSERT_TRUE(secondShape);
            EXPECT_NE(firstShape, secondShape);
            EXPECT_FALSE(runtime.DestroyShape(secondWorld, firstShape));
            EXPECT_TRUE(runtime.IsValid(secondWorld, secondShape));

            BodyConfiguration bodyConfiguration;
            bodyConfiguration.m_shapeHandle = secondShape;
            const BodyHandle firstBody = runtime.CreateBody(secondWorld, bodyConfiguration);
            ASSERT_TRUE(firstBody);
            ASSERT_TRUE(runtime.DestroyBody(secondWorld, firstBody));
            const BodyHandle secondBody = runtime.CreateBody(secondWorld, bodyConfiguration);
            ASSERT_TRUE(secondBody);
            EXPECT_NE(firstBody, secondBody);
            EXPECT_FALSE(runtime.SetBodyUserData(secondWorld, firstBody, 99));
            EXPECT_TRUE(runtime.IsValid(secondWorld, secondBody));

            const BodyHandle anchorBody = runtime.CreateBody(secondWorld, bodyConfiguration);
            ASSERT_TRUE(anchorBody);
            ConstraintConfiguration constraintConfiguration;
            constraintConfiguration.m_firstBodyHandle = secondBody;
            constraintConfiguration.m_secondBodyHandle = anchorBody;
            constraintConfiguration.m_geometry = PointConstraintConfiguration{};
            const ConstraintHandle firstConstraint = runtime.CreateConstraint(secondWorld, constraintConfiguration);
            ASSERT_TRUE(firstConstraint);
            ASSERT_TRUE(runtime.DestroyConstraint(secondWorld, firstConstraint));
            const ConstraintHandle secondConstraint = runtime.CreateConstraint(secondWorld, constraintConfiguration);
            ASSERT_TRUE(secondConstraint);
            EXPECT_NE(firstConstraint, secondConstraint);
            EXPECT_FALSE(runtime.SetConstraintEnabled(secondWorld, firstConstraint, false));
            ConstraintState constraintState;
            ASSERT_TRUE(runtime.GetConstraintState(secondWorld, secondConstraint, constraintState));
            EXPECT_TRUE(constraintState.m_enabled);

            const StateSnapshotHandle firstSnapshot = runtime.CaptureWorldState(secondWorld);
            ASSERT_TRUE(firstSnapshot);
            ASSERT_TRUE(runtime.DestroyStateSnapshot(secondWorld, firstSnapshot));
            const StateSnapshotHandle secondSnapshot = runtime.CaptureWorldState(secondWorld);
            ASSERT_TRUE(secondSnapshot);
            EXPECT_NE(firstSnapshot, secondSnapshot);
            EXPECT_FALSE(runtime.RestoreWorldState(secondWorld, firstSnapshot));
            EXPECT_TRUE(runtime.IsValid(secondWorld, secondSnapshot));
        }

        TEST(HandleIdentityIntegrationTests, CompletedAsyncHandleResultCannotAliasAReplacementRuntime)
        {
            HandleIdentityNameDictionaryScope nameDictionaryScope;
            Operation<CookedShapeHandle> retiredOperation;
            CookedShapeHandle retiredHandle;
            {
                Runtime retired(CreateHandleIdentitySystemConfiguration(), nullptr, SystemRegistration::Isolated);
                ASSERT_TRUE(retired);
                retiredOperation = retired.CookShapeAsync(ShapeConfiguration{
                    .m_geometry = SphereShapeConfiguration{.m_radius = 0.5f},
                });
                ASSERT_TRUE(retiredOperation);
                ASSERT_EQ(retiredOperation.Wait(), OperationStatus::Succeeded);
                ASSERT_NE(retiredOperation.GetResult(), nullptr);
                retiredHandle = *retiredOperation.GetResult();
                ASSERT_TRUE(retiredHandle);
            }

            Runtime replacement(CreateHandleIdentitySystemConfiguration(), nullptr, SystemRegistration::Isolated);
            ASSERT_TRUE(replacement);
            const CookedShapeHandle replacementHandle = replacement.CookShape(ShapeConfiguration{
                .m_geometry = SphereShapeConfiguration{.m_radius = 0.5f},
            });
            ASSERT_TRUE(replacementHandle);

            EXPECT_NE(retiredHandle, replacementHandle);
            EXPECT_FALSE(replacement.IsValid(retiredHandle));
            EXPECT_FALSE(replacement.DestroyCookedShape(retiredHandle));
            EXPECT_TRUE(replacement.IsValid(replacementHandle));
            EXPECT_TRUE(replacement.DestroyCookedShape(replacementHandle));

            ASSERT_NE(retiredOperation.GetResult(), nullptr);
            EXPECT_EQ(*retiredOperation.GetResult(), retiredHandle);
            retiredOperation.Reset();
        }
    } // namespace
} // namespace Jolt
