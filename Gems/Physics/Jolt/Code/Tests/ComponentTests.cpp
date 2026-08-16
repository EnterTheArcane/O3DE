/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/CharacterControllerComponent.h>
#include <Jolt/ColliderComponent.h>
#include <Jolt/ConstraintComponent.h>
#include <Jolt/HairComponent.h>
#include <Jolt/PathComponent.h>
#include <Jolt/RagdollComponent.h>
#include <Jolt/RigidBodyComponent.h>
#include <Jolt/SceneAsset.h>
#include <Jolt/SceneAssetHandler.h>
#include <Jolt/SceneBus.h>
#include <Jolt/SceneComponent.h>
#include <Jolt/SkeletonAsset.h>
#include <Jolt/SkeletonAssetHandler.h>
#include <Jolt/SkeletonComponent.h>
#include <Jolt/SoftBodyComponent.h>
#include <Jolt/StaticRigidBodyComponent.h>
#include <Jolt/SystemComponent.h>
#include <Jolt/SystemInternal.h>
#include <Jolt/VehicleComponents.h>
#include <Jolt/VirtualCharacterControllerComponent.h>
#include <Jolt/WorldQueryBus.h>

#include <AzTest/AzTest.h>

#include <AzCore/Asset/AssetManager.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/Math/Aabb.h>
#include <AzCore/Math/MathReflection.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Name/NameDictionary.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Script/ScriptContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/UnitTest/UnitTest.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/limits.h>
#include <AzFramework/Components/TransformComponent.h>

namespace Jolt
{
    namespace
    {
        inline constexpr AZ::TypeId ComponentVehicleStateTypeId{"{A2000001-0201-4201-8201-000000000001}"};
        inline constexpr AZ::TypeId ComponentVehicleFilterStateTypeId{"{A2000002-0202-4202-8202-000000000002}"};

        class ComponentNameDictionaryScope final
        {
        public:
            ComponentNameDictionaryScope()
            {
                if (!AZ::Interface<AZ::NameDictionary>::Get())
                {
                    AZ::NameDictionary::Create();
                    m_created = true;
                }
            }

            ~ComponentNameDictionaryScope()
            {
                if (m_created)
                {
                    AZ::NameDictionary::Destroy();
                }
            }

            AZ_DISABLE_COPY_MOVE(ComponentNameDictionaryScope);

        private:
            bool m_created = false;
        };

        class ComponentAssetManagerScope final
        {
        public:
            ComponentAssetManagerScope()
            {
                if (!AZ::Data::AssetManager::IsReady())
                {
                    AZ::Data::AssetManager::Create({});
                    m_created = true;
                }
            }

            ~ComponentAssetManagerScope()
            {
                if (m_created)
                {
                    AZ::Data::AssetManager::Destroy();
                }
            }

            AZ_DISABLE_COPY_MOVE(ComponentAssetManagerScope);

        private:
            bool m_created = false;
        };

        class ComponentVehicleCallbacks final
            : public IVehicleCallbacks
        {
        public:
            [[nodiscard]]
            AZ::TypeId GetStateTypeId() const override
            {
                return ComponentVehicleStateTypeId;
            }

            [[nodiscard]]
            AZ::u64 GetStateHash() const override
            {
                return 0;
            }

            void OnPreStep(
                [[maybe_unused]] const VehicleHandle vehicleHandle,
                [[maybe_unused]] const StepInformation& information,
                [[maybe_unused]] IVehicleStepContext& context) override
            {
                ++m_preStepCount;
            }

            AZ::u32 m_preStepCount = 0;
        };

        class ComponentVehicleCollisionFilter final
            : public IVehicleCollisionFilter
        {
        public:
            [[nodiscard]]
            AZ::TypeId GetStateTypeId() const override
            {
                return ComponentVehicleFilterStateTypeId;
            }

            [[nodiscard]]
            AZ::u64 GetStateHash() const override
            {
                return 0;
            }
        };

        class ComponentWorldNotifications final
            : public WorldNotificationBus::Handler
        {
        public:
            ComponentWorldNotifications() = default;

            void OnBodyActivation(
                const ActivationEvent& event) override
            {
                ++m_activationCount;
                m_lastActivation = event;
            }

            void OnBodyMoved(
                const BodyMoveEvent& event) override
            {
                ++m_bodyMoveCount;
                m_lastBodyMove = event;
            }

            void OnContact(
                const ContactEvent& event,
                const ContactPointView& points) override
            {
                ++m_contactCount;
                m_lastContact = event;
                m_lastContactPointCount = points.GetPointCount();
                if (m_lastContactPointCount > 0)
                {
                    m_lastContactPoint = points.GetPoint(0);
                }
            }

            AZ_DISABLE_COPY_MOVE(ComponentWorldNotifications);

            ActivationEvent m_lastActivation;
            BodyMoveEvent m_lastBodyMove;
            ContactEvent m_lastContact;
            ContactPoint m_lastContactPoint;
            AZ::u32 m_activationCount = 0;
            AZ::u32 m_bodyMoveCount = 0;
            AZ::u32 m_contactCount = 0;
            AZ::u32 m_lastContactPointCount = 0;
        };

        class ComponentSkeletonNotifications final
            : public SkeletonComponentNotificationBus::Handler
        {
        public:
            ComponentSkeletonNotifications() = default;

            void OnSkeletonReady(
                const SkeletonDefinitionHandle skeletonHandle) override
            {
                ++m_readyCount;
                m_lastReadyHandle = skeletonHandle;
            }

            void OnSkeletonReloading(
                const SkeletonDefinitionHandle skeletonHandle) override
            {
                ++m_reloadingCount;
                m_lastReloadingHandle = skeletonHandle;
            }

            void OnSkeletonReleased() override
            {
                ++m_releasedCount;
            }

            AZ_DISABLE_COPY_MOVE(ComponentSkeletonNotifications);

            SkeletonDefinitionHandle m_lastReadyHandle;
            SkeletonDefinitionHandle m_lastReloadingHandle;
            AZ::u32 m_readyCount = 0;
            AZ::u32 m_reloadingCount = 0;
            AZ::u32 m_releasedCount = 0;
        };

        class ComponentSceneNotifications final
            : public SceneNotificationBus::Handler
        {
        public:
            ComponentSceneNotifications() = default;

            void OnSceneReady(
                const SceneInstanceHandle instanceHandle) override
            {
                ++m_readyCount;
                m_lastReadyHandle = instanceHandle;
            }

            void OnSceneReloading(
                const SceneInstanceHandle instanceHandle) override
            {
                ++m_reloadingCount;
                m_lastReloadingHandle = instanceHandle;
            }

            void OnSceneReleased() override
            {
                ++m_releasedCount;
            }

            AZ_DISABLE_COPY_MOVE(ComponentSceneNotifications);

            SceneInstanceHandle m_lastReadyHandle;
            SceneInstanceHandle m_lastReloadingHandle;
            AZ::u32 m_readyCount = 0;
            AZ::u32 m_reloadingCount = 0;
            AZ::u32 m_releasedCount = 0;
        };

        SystemConfiguration CreateComponentSystemConfiguration()
        {
            SystemConfiguration configuration;
            configuration.m_defaultWorld.m_workerCount = 1;
            configuration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
            configuration.m_defaultWorld.m_autoSimulate = false;
            return configuration;
        }
    } // namespace

    TEST(ComponentTests, HandleReflectionRegistersEveryScriptVisibleHandleForSerialization)
    {
        {
            AZ::SerializeContext serializeContext;
            SystemComponent::Reflect(&serializeContext);

            const AZ::TypeId reflectedHandleTypeIds[] = {
                azrtti_typeid<BodyId>(),
                azrtti_typeid<BodyHandle>(),
                azrtti_typeid<BodySnapshotHandle>(),
                azrtti_typeid<CharacterHandle>(),
                azrtti_typeid<ConstraintHandle>(),
                azrtti_typeid<CookedShapeHandle>(),
                azrtti_typeid<GroupFilterHandle>(),
                azrtti_typeid<HairDefinitionHandle>(),
                azrtti_typeid<HairHandle>(),
                azrtti_typeid<MaterialHandle>(),
                azrtti_typeid<PathHandle>(),
                azrtti_typeid<RagdollDefinitionHandle>(),
                azrtti_typeid<RagdollHandle>(),
                azrtti_typeid<SceneDefinitionHandle>(),
                azrtti_typeid<SceneInstanceHandle>(),
                azrtti_typeid<ShapeHandle>(),
                azrtti_typeid<SkeletalAnimationHandle>(),
                azrtti_typeid<SkeletonDefinitionHandle>(),
                azrtti_typeid<SkeletonMapperHandle>(),
                azrtti_typeid<SkeletonPoseHandle>(),
                azrtti_typeid<SoftBodyDefinitionHandle>(),
                azrtti_typeid<StateSnapshotHandle>(),
                azrtti_typeid<VehicleHandle>(),
                azrtti_typeid<VirtualCharacterHandle>(),
                azrtti_typeid<WorldHandle>(),
            };

            for (const AZ::TypeId& reflectedHandleTypeId : reflectedHandleTypeIds)
            {
                EXPECT_TRUE(serializeContext.FindClassData(reflectedHandleTypeId));
            }

            serializeContext.EnableRemoveReflection();
            SystemComponent::Reflect(&serializeContext);
            serializeContext.DisableRemoveReflection();
        }

        AZ::GetGlobalSerializeContextModule().Cleanup();
    }

    TEST(ComponentTests, CharacterRuntimeAndCollisionTypesReflectForSerialization)
    {
        {
            AZ::SerializeContext serializeContext;
            SystemComponent::Reflect(&serializeContext);
            CharacterControllerComponent::Reflect(&serializeContext);
            VirtualCharacterControllerComponent::Reflect(&serializeContext);

            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<CharacterRuntimeConfiguration>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<CharacterCollisionHit>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<CharacterCollisionRequest>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<VirtualCharacterRuntimeConfiguration>()));

            serializeContext.EnableRemoveReflection();
            VirtualCharacterControllerComponent::Reflect(&serializeContext);
            CharacterControllerComponent::Reflect(&serializeContext);
            SystemComponent::Reflect(&serializeContext);
            serializeContext.DisableRemoveReflection();
        }

        AZ::GetGlobalSerializeContextModule().Cleanup();
    }

    TEST(ComponentTests, RigidBodyRuntimeTypesReflectForSerialization)
    {
        {
            AZ::SerializeContext serializeContext;
            RigidBodyComponent::Reflect(&serializeContext);

            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<BodyRuntimeConfiguration>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<BuoyancyConfiguration>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<MassPropertiesConfiguration>()));

            serializeContext.EnableRemoveReflection();
            RigidBodyComponent::Reflect(&serializeContext);
            serializeContext.DisableRemoveReflection();
        }

        AZ::GetGlobalSerializeContextModule().Cleanup();
    }

    TEST(ComponentTests, PathExtensionTypesReflectForSerialization)
    {
        {
            AZ::SerializeContext serializeContext;
            PathComponent::Reflect(&serializeContext);

            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<CustomPathConfiguration>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<CustomPathInfo>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<CustomPathPoint>()));

            serializeContext.EnableRemoveReflection();
            PathComponent::Reflect(&serializeContext);
            serializeContext.DisableRemoveReflection();
        }

        AZ::GetGlobalSerializeContextModule().Cleanup();
    }

    TEST(ComponentTests, RuntimeEventDiagnosticAndSnapshotTypesReflectForSerialization)
    {
        {
            AZ::SerializeContext serializeContext;
            SystemComponent::Reflect(&serializeContext);

            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<ActivationEvent>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<BodyMoveEvent>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<BodySimulationStatistics>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<BroadPhaseStatistics>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<ContactEvent>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<ContactPoint>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<DebugCaptureConfiguration>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<DebugCaptureStatistics>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<DebugDrawSettings>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<DebugHairDrawSettings>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<SimulationResult>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<DiagnosticStatisticsResult>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<NarrowPhaseStatistics>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<StateSnapshotArchive>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<StateSnapshotConfiguration>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<StateValidationResult>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<WorldStateDigest>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<WorldStatistics>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<VirtualCharacterMoveEvent>()));

            serializeContext.EnableRemoveReflection();
            SystemComponent::Reflect(&serializeContext);
            serializeContext.DisableRemoveReflection();
        }

        AZ::GetGlobalSerializeContextModule().Cleanup();
    }

    TEST(ComponentTests, SkeletonRuntimeTypesReflectForSerialization)
    {
        {
            AZ::SerializeContext serializeContext;
            SystemComponent::Reflect(&serializeContext);
            SkeletonComponent::Reflect(&serializeContext);

            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<SkeletonAsset>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<SkeletonAssetData>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<SkeletonComponent>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<SkeletonComponentConfiguration>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<SkeletonMapperConfiguration>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<SkeletonMapperState>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<SkeletonMapperMappingState>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<SkeletonMapperChainState>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<SkeletonMapperUnmappedJoint>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<SkeletonMapperLockedTranslation>()));

            serializeContext.EnableRemoveReflection();
            SkeletonComponent::Reflect(&serializeContext);
            SystemComponent::Reflect(&serializeContext);
            serializeContext.DisableRemoveReflection();
        }

        AZ::GetGlobalSerializeContextModule().Cleanup();
    }

    TEST(ComponentTests, SceneRuntimeTypesReflectForSerialization)
    {
        {
            AZ::SerializeContext serializeContext;
            SceneComponent::Reflect(&serializeContext);

            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<SceneAsset>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<SceneAssetData>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<SceneComponent>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<SceneComponentConfiguration>()));

            serializeContext.EnableRemoveReflection();
            SceneComponent::Reflect(&serializeContext);
            serializeContext.DisableRemoveReflection();
        }

        AZ::GetGlobalSerializeContextModule().Cleanup();
    }

    TEST(ComponentTests, VehicleRuntimeTypesReflectForSerialization)
    {
        {
            AZ::SerializeContext serializeContext;
            WheeledVehicleComponent::Reflect(&serializeContext);

            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<VehicleCollisionConfiguration>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<VehiclePowertrainControl>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<VehicleRuntimeConfiguration>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<WheelMotion>()));

            serializeContext.EnableRemoveReflection();
            WheeledVehicleComponent::Reflect(&serializeContext);
            serializeContext.DisableRemoveReflection();
        }

        AZ::GetGlobalSerializeContextModule().Cleanup();
    }

    TEST(ComponentTests, BehaviorReflectionExposesEveryRuntimeComponentBus)
    {
        ComponentNameDictionaryScope nameDictionary;
        AZ::BehaviorContext behaviorContext;
        AZ::Entity::Reflect(&behaviorContext);
        AZ::MathReflect(&behaviorContext);
        AZ::Name::Reflect(&behaviorContext);

        const auto reflectComponents = [&behaviorContext]()
        {
            SystemComponent::Reflect(&behaviorContext);
            ColliderComponent::Reflect(&behaviorContext);
            RigidBodyComponent::Reflect(&behaviorContext);
            StaticRigidBodyComponent::Reflect(&behaviorContext);
            CharacterControllerComponent::Reflect(&behaviorContext);
            ConstraintComponent::Reflect(&behaviorContext);
            HairComponent::Reflect(&behaviorContext);
            PathComponent::Reflect(&behaviorContext);
            RagdollComponent::Reflect(&behaviorContext);
            SceneComponent::Reflect(&behaviorContext);
            SkeletonComponent::Reflect(&behaviorContext);
            WheeledVehicleComponent::Reflect(&behaviorContext);
            MotorcycleComponent::Reflect(&behaviorContext);
            TrackedVehicleComponent::Reflect(&behaviorContext);
            VirtualCharacterControllerComponent::Reflect(&behaviorContext);
            SoftBodyComponent::Reflect(&behaviorContext);
        };
        reflectComponents();

        AZ::ScriptContext scriptContext;
        scriptContext.BindTo(&behaviorContext);

        EXPECT_TRUE(behaviorContext.m_ebuses.contains("JoltBodyRequestBus"));
        EXPECT_TRUE(behaviorContext.m_ebuses.contains("JoltBodyNotificationBus"));
        EXPECT_TRUE(behaviorContext.m_ebuses.contains("JoltCharacterRequestBus"));
        EXPECT_TRUE(behaviorContext.m_ebuses.contains("JoltColliderRequestBus"));
        EXPECT_TRUE(behaviorContext.m_ebuses.contains("JoltConstraintRequestBus"));
        EXPECT_TRUE(behaviorContext.m_ebuses.contains("JoltConstraintNotificationBus"));
        EXPECT_TRUE(behaviorContext.m_ebuses.contains("JoltHairRequestBus"));
        EXPECT_TRUE(behaviorContext.m_ebuses.contains("JoltHairNotificationBus"));
        EXPECT_TRUE(behaviorContext.m_ebuses.contains("JoltRigidBodyRequestBus"));
        EXPECT_TRUE(behaviorContext.m_ebuses.contains("JoltStaticRigidBodyRequestBus"));
        EXPECT_TRUE(behaviorContext.m_ebuses.contains("JoltPathRequestBus"));
        EXPECT_TRUE(behaviorContext.m_ebuses.contains("JoltPathNotificationBus"));
        EXPECT_TRUE(behaviorContext.m_ebuses.contains("JoltRagdollRequestBus"));
        EXPECT_TRUE(behaviorContext.m_ebuses.contains("JoltRagdollNotificationBus"));
        EXPECT_TRUE(behaviorContext.m_ebuses.contains("JoltWheeledVehicleRequestBus"));
        EXPECT_TRUE(behaviorContext.m_ebuses.contains("JoltMotorcycleRequestBus"));
        EXPECT_TRUE(behaviorContext.m_ebuses.contains("JoltTrackedVehicleRequestBus"));
        EXPECT_TRUE(behaviorContext.m_ebuses.contains("JoltVehicleNotificationBus"));
        EXPECT_TRUE(behaviorContext.m_ebuses.contains("JoltVirtualCharacterRequestBus"));
        EXPECT_TRUE(behaviorContext.m_ebuses.contains("JoltVirtualCharacterNotificationBus"));
        EXPECT_TRUE(behaviorContext.m_ebuses.contains("JoltSoftBodyRequestBus"));
        EXPECT_TRUE(behaviorContext.m_ebuses.contains("JoltSkeletonRequestBus"));
        EXPECT_TRUE(behaviorContext.m_ebuses.contains("JoltSkeletonComponentRequestBus"));
        EXPECT_TRUE(behaviorContext.m_ebuses.contains("JoltSkeletonComponentNotificationBus"));
        EXPECT_TRUE(behaviorContext.m_ebuses.contains("JoltSceneRequestBus"));
        EXPECT_TRUE(behaviorContext.m_ebuses.contains("JoltSceneNotificationBus"));
        EXPECT_TRUE(behaviorContext.m_ebuses.contains("JoltWorldNotificationBus"));
        EXPECT_TRUE(behaviorContext.m_ebuses.contains("JoltWorldQueryRequestBus"));

        EXPECT_TRUE(behaviorContext.m_properties.contains("ActivationState_Active"));
        EXPECT_TRUE(behaviorContext.m_properties.contains("ActiveEdgeMode_CollideWithAll"));
        EXPECT_TRUE(behaviorContext.m_properties.contains("AllowedDofs_All"));
        EXPECT_TRUE(behaviorContext.m_properties.contains("ConstraintKind_Hinge"));
        EXPECT_TRUE(behaviorContext.m_properties.contains("FaceCollectionMode_Collect"));
        EXPECT_TRUE(behaviorContext.m_properties.contains("GroundState_OnGround"));
        EXPECT_TRUE(behaviorContext.m_properties.contains("MaterialCombineMode_GeometricMean"));
        EXPECT_TRUE(behaviorContext.m_properties.contains("Precision_Single"));
        EXPECT_TRUE(behaviorContext.m_properties.contains("RestoreSafety_Transactional"));
        EXPECT_TRUE(behaviorContext.m_properties.contains("ShapeKind_ConvexHull"));
        EXPECT_TRUE(behaviorContext.m_properties.contains("SimdLevel_WasmSimd"));
        EXPECT_TRUE(behaviorContext.m_properties.contains("SimulationError_InvalidRequest"));
        EXPECT_TRUE(behaviorContext.m_properties.contains("StateSnapshotFlags_Bodies"));
        EXPECT_TRUE(behaviorContext.m_properties.contains("VehicleCollisionTestMode_Cylinder"));

        constexpr AZStd::array requiredEnumValues = {
            "CompoundShapeKind_None",
            "CompoundShapeKind_Mutable",
            "CompoundShapeKind_Static",
            "ConstraintSpace_None",
            "ConstraintSpace_LocalToCenterOfMass",
            "ConstraintSpace_World",
            "ContactDecision_None",
            "ContactDecision_AcceptAllForPair",
            "ContactDecision_Accept",
            "ContactDecision_Reject",
            "ContactDecision_RejectAllForPair",
            "DebugCaptureFlags_None",
            "DebugCaptureFlags_All",
            "DebugCaptureFlags_ContactManifolds",
            "DebugCaptureFlags_ContactPointReduction",
            "DebugCaptureFlags_ContactPoints",
            "DebugCaptureFlags_ContactSupportingFaces",
            "DebugCaptureFlags_MotionQualityLinearCasts",
            "DebugCaptureFlags_SubmergedVolumes",
            "DebugCaptureFlags_VirtualCharacterConstraints",
            "DebugCaptureFlags_VirtualCharacterStickToFloor",
            "DebugCaptureFlags_VirtualCharacterSupportingVolumes",
            "DebugCaptureFlags_VirtualCharacterWalkStairs",
            "DebugCullMode_None",
            "DebugCullMode_BackFace",
            "DebugCullMode_FrontFace",
            "DebugDrawMode_None",
            "DebugDrawMode_Solid",
            "DebugDrawMode_Wireframe",
            "DebugHairDrawFlags_None",
            "DebugHairDrawFlags_All",
            "DebugHairDrawFlags_AngularVelocities",
            "DebugHairDrawFlags_GridDensities",
            "DebugHairDrawFlags_GridVelocities",
            "DebugHairDrawFlags_InitialGravity",
            "DebugHairDrawFlags_NeutralDensities",
            "DebugHairDrawFlags_Orientations",
            "DebugHairDrawFlags_RenderStrands",
            "DebugHairDrawFlags_Rods",
            "DebugHairDrawFlags_SkinPoints",
            "DebugHairDrawFlags_UnloadedRods",
            "DebugHairDrawFlags_VertexVelocities",
            "DebugHairStrandColor_None",
            "DebugHairStrandColor_GlobalPose",
            "DebugHairStrandColor_GravityFactor",
            "DebugHairStrandColor_GridVelocityFactor",
            "DebugHairStrandColor_PerRenderStrand",
            "DebugHairStrandColor_PerSimulatedStrand",
            "DebugHairStrandColor_SkinGlobalPose",
            "DebugHairStrandColor_WorldTransformInfluence",
            "DebugShapeColor_None",
            "DebugShapeColor_Instance",
            "DebugShapeColor_Island",
            "DebugShapeColor_Material",
            "DebugShapeColor_MotionType",
            "DebugShapeColor_ShapeType",
            "DebugShapeColor_SleepState",
            "DebugSoftBodyConstraintColor_None",
            "DebugSoftBodyConstraintColor_ConstraintGroup",
            "DebugSoftBodyConstraintColor_ConstraintOrder",
            "DebugSoftBodyConstraintColor_ConstraintType",
            "DebugDrawFlags_None",
            "DebugDrawFlags_All",
            "DebugDrawFlags_BoundingBoxes",
            "DebugDrawFlags_CapturedSimulation",
            "DebugDrawFlags_CenterOfMassTransforms",
            "DebugDrawFlags_Constraints",
            "DebugDrawFlags_ConstraintLimits",
            "DebugDrawFlags_ConstraintReferenceFrames",
            "DebugDrawFlags_ConvexHullFaceOutlines",
            "DebugDrawFlags_HeightfieldTriangleOutlines",
            "DebugDrawFlags_MassAndInertia",
            "DebugDrawFlags_MeshTriangleGroups",
            "DebugDrawFlags_MeshTriangleOutlines",
            "DebugDrawFlags_Shapes",
            "DebugDrawFlags_ShapeSupportingFaces",
            "DebugDrawFlags_ShapeSupportDirections",
            "DebugDrawFlags_ShapeSupportFunctions",
            "DebugDrawFlags_ShapeWireframes",
            "DebugDrawFlags_SleepStatistics",
            "DebugDrawFlags_SoftBodyBendConstraints",
            "DebugDrawFlags_SoftBodyEdgeConstraints",
            "DebugDrawFlags_SoftBodyLongRangeConstraints",
            "DebugDrawFlags_SoftBodyPredictedBounds",
            "DebugDrawFlags_SoftBodyRodBendTwistConstraints",
            "DebugDrawFlags_SoftBodyRods",
            "DebugDrawFlags_SoftBodyRodStates",
            "DebugDrawFlags_SoftBodySkinConstraints",
            "DebugDrawFlags_SoftBodyVertexVelocities",
            "DebugDrawFlags_SoftBodyVertices",
            "DebugDrawFlags_SoftBodyVolumeConstraints",
            "DebugDrawFlags_Velocities",
            "DebugDrawFlags_WorldTransforms",
            "MeshBuildQuality_None",
            "MeshBuildQuality_FavorBuildSpeed",
            "MeshBuildQuality_FavorRuntimePerformance",
            "MotorState_None",
            "MotorState_Off",
            "MotorState_Position",
            "MotorState_PositionAndVelocity",
            "MotorState_Velocity",
            "PathRotationConstraint_None",
            "PathRotationConstraint_ConstrainAroundBinormal",
            "PathRotationConstraint_ConstrainAroundNormal",
            "PathRotationConstraint_ConstrainAroundTangent",
            "PathRotationConstraint_ConstrainToPath",
            "PathRotationConstraint_Free",
            "PathRotationConstraint_FullyConstrained",
            "SixDofAxisMode_None",
            "SixDofAxisMode_Fixed",
            "SixDofAxisMode_Free",
            "SixDofAxisMode_Limited",
            "SoftBodyBendType_None",
            "SoftBodyBendType_Dihedral",
            "SoftBodyBendType_Distance",
            "SoftBodyContactDecision_None",
            "SoftBodyContactDecision_Accept",
            "SoftBodyContactDecision_Reject",
            "SoftBodyLongRangeAttachmentType_None",
            "SoftBodyLongRangeAttachmentType_EuclideanDistance",
            "SoftBodyLongRangeAttachmentType_GeodesicDistance",
            "SpringMode_None",
            "SpringMode_FrequencyAndDamping",
            "SpringMode_MassNormalizedStiffnessAndDamping",
            "SpringMode_StiffnessAndDamping",
            "SwingType_None",
            "SwingType_Cone",
            "SwingType_Pyramid",
        };
        for (const char* enumValue : requiredEnumValues)
        {
            SCOPED_TRACE(enumValue);
            EXPECT_TRUE(behaviorContext.m_properties.contains(enumValue));
        }

        EXPECT_TRUE(behaviorContext.m_classes.contains("DebugCaptureConfiguration"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("DebugCaptureStatistics"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("DebugDrawSettings"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("DebugHairDrawSettings"));

        constexpr AZStd::array reflectedHandles = {
            "JoltBodyHandle",
            "JoltBodySnapshotHandle",
            "JoltCharacterHandle",
            "JoltConstraintHandle",
            "JoltCookedShapeHandle",
            "JoltGroupFilterHandle",
            "JoltHairDefinitionHandle",
            "JoltHairHandle",
            "JoltMaterialHandle",
            "JoltPathHandle",
            "JoltRagdollDefinitionHandle",
            "JoltRagdollHandle",
            "JoltSceneDefinitionHandle",
            "JoltSceneInstanceHandle",
            "JoltShapeHandle",
            "JoltSkeletalAnimationHandle",
            "JoltSkeletonDefinitionHandle",
            "JoltSkeletonMapperHandle",
            "JoltSkeletonPoseHandle",
            "JoltSoftBodyDefinitionHandle",
            "JoltStateSnapshotHandle",
            "JoltVehicleHandle",
            "JoltVirtualCharacterHandle",
            "JoltWorldHandle",
        };

        for (const char* reflectedHandle : reflectedHandles)
        {
            EXPECT_TRUE(behaviorContext.m_classes.contains(reflectedHandle)) << reflectedHandle;
        }

        EXPECT_TRUE(behaviorContext.m_classes.contains("JoltBodyMoveEvent"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("JoltBodyId"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("JoltBodyState"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("JoltRuntimeInfo"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("JoltVersion"));

        const AZ::BehaviorClass* runtimeInfo = behaviorContext.m_classes.at("JoltRuntimeInfo");
        EXPECT_TRUE(runtimeInfo->m_properties.contains("buildFingerprint"));
        EXPECT_TRUE(runtimeInfo->m_methods.contains("GetConfiguration"));
        EXPECT_TRUE(runtimeInfo->m_methods.contains("GetPatchHash"));
        EXPECT_TRUE(runtimeInfo->m_methods.contains("GetPatchRevision"));
        EXPECT_TRUE(runtimeInfo->m_methods.contains("GetSourceRevision"));
        EXPECT_TRUE(runtimeInfo->m_properties.contains("broadPhaseStatistics"));
        EXPECT_TRUE(runtimeInfo->m_properties.contains("narrowPhaseStatistics"));

        EXPECT_TRUE(behaviorContext.m_classes.contains("BodyCollection"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("JoltBoxShapeConfiguration"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("CustomConvexShapeConfiguration"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("CustomConvexShapeInfo"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("CustomShapeConfiguration"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("CustomShapeInfo"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("CustomConstraintInfo"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("HermitePathPoint"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("PathSample"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("PathState"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("BroadPhaseLayerConfiguration"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("BroadPhaseCastRequest"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("BroadPhaseOverlapRequest"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("JoltCapsuleShapeConfiguration"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("JoltBroadPhaseLayer"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("JoltCharacterState"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("CharacterRuntimeConfiguration"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("ActivationEvent"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("JoltContactEvent"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("JoltContactPoint"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("ContactPointView"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("JoltShapeProperties"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("SimulationResult"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("StateSnapshotArchive"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("StateSnapshotConfiguration"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("StateValidationResult"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("SubmergedVolumeRequest"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("SubmergedVolumeResult"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("JoltObjectLayer"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("ObjectLayerConfiguration"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("HairDefinitionState"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("HairGridCellState"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("HairReadbackResult"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("HairState"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("HairVertexState"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("SoftBodyDefinitionState"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("SoftBodyDihedralBendConstraint"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("SoftBodyEdgeConstraint"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("SoftBodyInverseBind"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("SoftBodyLongRangeConstraint"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("SoftBodyRodBendTwistConstraint"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("SoftBodyRodStretchShearConstraint"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("SoftBodySkinConstraint"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("SoftBodySkinWeight"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("SoftBodyVolumeConstraint"));
        const AZ::BehaviorClass* softBodySkinConstraint =
            behaviorContext.m_classes.at("SoftBodySkinConstraint");
        EXPECT_TRUE(softBodySkinConstraint->m_methods.contains("GetWeight"));
        EXPECT_TRUE(softBodySkinConstraint->m_methods.contains("SetWeight"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("MaterialCollection"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("HeightfieldMaterialIndexCollection"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("HeightfieldSampleCollection"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("RagdollState"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("SkeletonJoint"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("SkeletonDefinitionConfiguration"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("SkeletonMapperConfiguration"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("SkeletalAnimationConfiguration"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("JoltQueryFilter"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("QueryResult"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("BufferResult"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("BodySimulationStatistics"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("BroadPhaseStatistics"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("DiagnosticStatisticsResult"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("NarrowPhaseStatistics"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("RaycastHit"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("JoltRaycastRequestCollection"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("ClosestRaycastResultCollection"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("JoltRaycastRequest"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("JoltOverlapHit"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("JoltCylinderShapeConfiguration"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("EmptyShapeConfiguration"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("OffsetCenterOfMassShapeConfiguration"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("PlaneShapeConfiguration"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("ShapeCastHit"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("JoltShapeCastRequest"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("ShapeOverlapHit"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("ShapePlacement"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("SupportingFaceVertexCollection"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("SubShapeTransform"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("TransformedShape"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("TransformedShapeCastHit"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("TransformedShapeCastRequest"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("TransformedShapeCollisionHit"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("TransformedShapeCollisionRequest"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("TransformedShapeCollection"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("TransformedTriangleCollection"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("WorldTransform"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("WorldCapacity"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("JoltWorldConfiguration"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("WorldRuntimeConfiguration"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("WorldStateDigest"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("WorldStatistics"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("JoltSystemConfiguration"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("VehicleAntiRollBarConfiguration"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("VehicleCollisionConfiguration"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("VehicleDifferentialConfiguration"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("VehicleEngineConfiguration"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("VehiclePowertrainControl"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("VehiclePowertrainState"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("VehicleRuntimeConfiguration"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("VehicleTransmissionConfiguration"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("VehicleTrackConfiguration"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("MotorcycleControllerUpdateConfiguration"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("TrackedVehicleInput"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("WheeledVehicleInput"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("WheelBasis"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("WheelMotion"));

        constexpr AZStd::array constructibleVehicleTypes = {
            "MotorcycleControllerUpdateConfiguration",
            "TrackedVehicleInput",
            "VehicleAntiRollBarConfiguration",
            "VehicleCollisionConfiguration",
            "VehicleDifferentialConfiguration",
            "VehicleEngineConfiguration",
            "VehiclePowertrainControl",
            "VehicleRuntimeConfiguration",
            "VehicleTrackConfiguration",
            "VehicleTransmissionConfiguration",
            "WheelMotion",
            "WheeledVehicleInput",
        };

        for (const char* constructibleVehicleType : constructibleVehicleTypes)
        {
            EXPECT_EQ(behaviorContext.m_classes.at(constructibleVehicleType)->m_constructors.size(), 1)
                << constructibleVehicleType;
        }

        const AZ::BehaviorEBus* bodyRequestBus = behaviorContext.m_ebuses.at("JoltBodyRequestBus");
        EXPECT_TRUE(bodyRequestBus->m_events.contains("IsSimulationEnabled"));
        EXPECT_TRUE(bodyRequestBus->m_events.contains("GetWorldHandle"));
        EXPECT_TRUE(bodyRequestBus->m_events.contains("GetBodyHandle"));
        EXPECT_TRUE(bodyRequestBus->m_events.contains("GetUserData"));
        EXPECT_TRUE(bodyRequestBus->m_events.contains("SetUserData"));
        EXPECT_TRUE(bodyRequestBus->m_events.contains("GetCenterOfMassTransform"));

        const AZ::BehaviorEBus* constraintRequestBus =
            behaviorContext.m_ebuses.at("JoltConstraintRequestBus");
        EXPECT_TRUE(constraintRequestBus->m_events.contains("GetDebugDrawSize"));
        EXPECT_TRUE(constraintRequestBus->m_events.contains("GetUserData"));
        EXPECT_TRUE(constraintRequestBus->m_events.contains("SetDebugDrawSize"));
        EXPECT_TRUE(constraintRequestBus->m_events.contains("SetUserData"));
        EXPECT_TRUE(constraintRequestBus->m_events.contains("SetHingeTargetOrientation"));

        EXPECT_TRUE(behaviorContext.m_classes.contains("WheelState"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("WheeledVehicleState"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("MotorcycleState"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("TrackedVehicleState"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("VirtualCharacterState"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("CharacterCollisionHit"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("CharacterCollisionRequest"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("VirtualCharacterContact"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("VirtualCharacterRuntimeConfiguration"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("VirtualCharacterStairConfiguration"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("VirtualCharacterMoveEvent"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("SoftBodyRuntimeConfiguration"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("JoltBodyRuntimeConfiguration"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("JoltMassPropertiesConfiguration"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("SoftBodyFace"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("SoftBodyRodState"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("SoftBodyVertex"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("CompoundChildConfiguration"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("ConvexHullState"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("ConvexHullTopology"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("ShapeStats"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("RotatedTranslatedShapeConfiguration"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("ScaledShapeConfiguration"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("JoltSphereShapeConfiguration"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("TaperedCapsuleShapeConfiguration"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("TaperedCylinderShapeConfiguration"));
        EXPECT_TRUE(behaviorContext.m_classes.contains("TriangleShapeConfiguration"));

        const AZ::BehaviorClass* hairState = behaviorContext.m_classes.at("HairState");
        EXPECT_TRUE(hairState->m_properties.contains("scalpToHeadTransform"));

        const AZ::BehaviorClass* worldStatistics = behaviorContext.m_classes.at("WorldStatistics");
        EXPECT_TRUE(worldStatistics->m_properties.contains("bodySnapshotCount"));
        EXPECT_TRUE(worldStatistics->m_properties.contains("hairCount"));
        EXPECT_TRUE(worldStatistics->m_properties.contains("lastUpdateJobCount"));
        EXPECT_TRUE(worldStatistics->m_properties.contains("lastUpdateMaximumTaskCount"));
        EXPECT_TRUE(worldStatistics->m_properties.contains("lastUpdateTaskCount"));
        EXPECT_TRUE(worldStatistics->m_properties.contains("ragdollCount"));
        EXPECT_TRUE(worldStatistics->m_properties.contains("sceneInstanceCount"));
        EXPECT_TRUE(worldStatistics->m_properties.contains("stateSnapshotCount"));

        const AZ::BehaviorClass* systemConfiguration =
            behaviorContext.m_classes.at("JoltSystemConfiguration");
        EXPECT_TRUE(systemConfiguration->m_properties.contains("softBodyTriangleThickness"));

        const AZ::BehaviorClass* convexHullTopology = behaviorContext.m_classes.at("ConvexHullTopology");
        EXPECT_TRUE(convexHullTopology->m_methods.contains("GetFaceCount"));
        EXPECT_TRUE(convexHullTopology->m_methods.contains("GetFaceVertexCount"));
        EXPECT_TRUE(convexHullTopology->m_methods.contains("GetFaceVertexIndex"));
        EXPECT_TRUE(convexHullTopology->m_methods.contains("GetPlaneCount"));
        EXPECT_TRUE(convexHullTopology->m_methods.contains("GetPlaneRelativeToCenterOfMass"));
        EXPECT_TRUE(convexHullTopology->m_methods.contains("GetPointCount"));
        EXPECT_TRUE(convexHullTopology->m_methods.contains("GetPointRelativeToCenterOfMass"));
        EXPECT_TRUE(convexHullTopology->m_methods.contains("GetState"));
        EXPECT_TRUE(convexHullTopology->m_methods.contains("HasOverflow"));
        EXPECT_TRUE(convexHullTopology->m_methods.contains("IsComplete"));

        const AZ::BehaviorClass* materialCollection = behaviorContext.m_classes.at("MaterialCollection");
        EXPECT_TRUE(materialCollection->m_methods.contains("AddMaterial"));
        EXPECT_TRUE(materialCollection->m_methods.contains("Clear"));
        EXPECT_TRUE(materialCollection->m_methods.contains("GetMaterial"));
        EXPECT_TRUE(materialCollection->m_methods.contains("GetMaterialCount"));
        EXPECT_TRUE(materialCollection->m_methods.contains("GetRequiredMaterialCount"));
        EXPECT_TRUE(materialCollection->m_methods.contains("HasOverflow"));

        const AZ::BehaviorEBus* rigidBodyRequestBus = behaviorContext.m_ebuses.at("JoltRigidBodyRequestBus");
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("AddForceAndTorque"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("AddLinearVelocity"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("ApplyBuoyancyImpulse"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("GetCenterOfMassTransform"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("GetInverseMass"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("GetAngularVelocity"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("GetVelocities"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("GetFriction"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("GetGravityFactor"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("GetLinearVelocity"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("GetMaximumAngularVelocity"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("GetMaximumLinearVelocity"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("GetMotionQuality"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("GetMotionType"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("GetObjectLayer"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("GetCollisionGroup"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("GetShapeHandle"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("GetAccumulatedForceAndTorque"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("ResetAccumulatedForce"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("ResetAccumulatedTorque"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("ResetMotion"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("GetBounds"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("GetSubmergedVolume"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("GetSurfaceNormal"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("GetMaterial"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("GetPointVelocity"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("GetPosition"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("GetRestitution"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("GetRotation"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("GetRuntimeConfiguration"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("InvalidateContactCache"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("IsManifoldReductionEnabled"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("IsSensor"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("GetLinearDamping"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("GetAngularDamping"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("IsSleepingAllowed"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("IsGyroscopicForceEnabled"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("IsKinematicVsNonDynamicCollisionEnabled"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("IsEnhancedInternalEdgeRemovalEnabled"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("GetSolverStepCounts"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("ResetSleepTimer"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("SetAngularVelocity"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("SetFriction"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("SetGravityFactor"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("SetLinearVelocity"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("SetManifoldReductionEnabled"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("SetMaximumAngularVelocity"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("SetMaximumLinearVelocity"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("SetMotionQuality"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("SetPosition"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("SetRestitution"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("SetRotation"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("SetSensor"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("SetLinearDamping"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("SetAngularDamping"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("SetSleepingAllowed"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("SetGyroscopicForceEnabled"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("SetKinematicVsNonDynamicCollisionEnabled"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("SetEnhancedInternalEdgeRemovalEnabled"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("SetSolverStepCounts"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("SetTransform"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("SetTransformWhenChanged"));
        EXPECT_TRUE(rigidBodyRequestBus->m_events.contains("UpdateRuntimeConfiguration"));

        const AZ::BehaviorEBus* characterRequestBus = behaviorContext.m_ebuses.at("JoltCharacterRequestBus");
        EXPECT_TRUE(characterRequestBus->m_events.contains("CheckCollision"));
        EXPECT_TRUE(characterRequestBus->m_events.contains("GetCenterOfMassTransform"));
        EXPECT_TRUE(characterRequestBus->m_events.contains("GetRuntimeConfiguration"));
        EXPECT_TRUE(characterRequestBus->m_events.contains("UpdateRuntimeConfiguration"));

        const AZ::BehaviorEBus* colliderRequestBus = behaviorContext.m_ebuses.at("JoltColliderRequestBus");
        EXPECT_TRUE(colliderRequestBus->m_events.contains("GetRootBoxConfiguration"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("GetRootCapsuleConfiguration"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("GetRootConvexHullState"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("GetRootConvexHullTopology"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("GetRootCylinderConfiguration"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("GetRootEmptyConfiguration"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("GetRootCompoundChild"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("GetRootCompoundChildCount"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("GetRootCompoundChildIndex"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("GetRootDirectChildShape"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("GetRootMeshMaterials"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("GetRootMeshTriangleMaterialIndex"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("GetRootMeshTriangleUserData"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("GetRootOffsetCenterOfMassConfiguration"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("GetRootPlaneConfiguration"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("GetRootRotatedTranslatedConfiguration"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("GetRootScaledConfiguration"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("GetRootSphereConfiguration"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("GetRootTaperedCapsuleConfiguration"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("GetRootTaperedCylinderConfiguration"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("GetRootTriangleConfiguration"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("GetRootHeightfieldHeights"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("GetRootHeightfieldMaterialIndices"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("GetRootHeightfieldMaterials"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("GetRootHeightfieldPosition"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("GetRootHeightfieldState"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("GetRootHeightfieldSubShapeCoordinates"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("GetRootShapeMaterial"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("GetRootShapeProperties"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("GetRootShapeSubmergedVolume"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("GetRootShapeStats"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("GetRootShapeStatsRecursive"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("GetRootShapeSubShapeUserData"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("GetRootShapeSurfaceNormal"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("GetRootShapeUserData"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("IsRootShapeScaleValid"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("IsRootHeightfieldNoCollision"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("MakeRootShapeScaleValid"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("UpdateRootHeightfieldHeights"));
        EXPECT_TRUE(colliderRequestBus->m_events.contains("UpdateRootHeightfieldMaterials"));

        const AZ::BehaviorEBus* hairRequestBus = behaviorContext.m_ebuses.at("JoltHairRequestBus");
        EXPECT_TRUE(hairRequestBus->m_events.contains("CopyGridCellStates"));
        EXPECT_TRUE(hairRequestBus->m_events.contains("CopyNeutralDensity"));
        EXPECT_TRUE(hairRequestBus->m_events.contains("CopyRenderPositions"));
        EXPECT_TRUE(hairRequestBus->m_events.contains("CopyScalpPositions"));
        EXPECT_TRUE(hairRequestBus->m_events.contains("CopySkinnedScalpVertices"));
        EXPECT_TRUE(hairRequestBus->m_events.contains("CopyVertexStates"));
        EXPECT_TRUE(hairRequestBus->m_events.contains("GetDefinitionState"));
        EXPECT_TRUE(hairRequestBus->m_events.contains("SetScalpToHeadTransform"));

        const AZ::BehaviorEBus* pathRequestBus = behaviorContext.m_ebuses.at("JoltPathRequestBus");
        EXPECT_TRUE(pathRequestBus->m_events.contains("CopyPoints"));
        EXPECT_TRUE(pathRequestBus->m_events.contains("FindClosestPoint"));
        EXPECT_TRUE(pathRequestBus->m_events.contains("GetPathHandle"));
        EXPECT_TRUE(pathRequestBus->m_events.contains("GetState"));
        EXPECT_TRUE(pathRequestBus->m_events.contains("Sample"));

        const AZ::BehaviorEBus* ragdollRequestBus = behaviorContext.m_ebuses.at("JoltRagdollRequestBus");
        EXPECT_TRUE(ragdollRequestBus->m_events.contains("DriveMotorsWithVelocity"));
        EXPECT_TRUE(ragdollRequestBus->m_events.contains("SetCollisionGroupId"));
        EXPECT_TRUE(ragdollRequestBus->m_events.contains("SetLinearVelocity"));

        const AZ::BehaviorClass* ragdollState = behaviorContext.m_classes.at("RagdollState");
        EXPECT_TRUE(ragdollState->m_properties.contains("definitionHandle"));
        EXPECT_TRUE(ragdollState->m_properties.contains("isInSimulation"));

        const AZ::BehaviorEBus* skeletonRequestBus =
            behaviorContext.m_ebuses.at("JoltSkeletonRequestBus");
        EXPECT_TRUE(skeletonRequestBus->m_events.contains("CreateSkeletonDefinition"));
        EXPECT_TRUE(skeletonRequestBus->m_events.contains("CreateSkeletalAnimation"));
        EXPECT_TRUE(skeletonRequestBus->m_events.contains("CreateSkeletonMapper"));
        EXPECT_TRUE(skeletonRequestBus->m_events.contains("CreateSkeletonPose"));
        EXPECT_TRUE(skeletonRequestBus->m_events.contains("MapSkeletonPose"));
        EXPECT_TRUE(skeletonRequestBus->m_events.contains("MapSkeletonPoseReverse"));
        EXPECT_TRUE(skeletonRequestBus->m_events.contains("SampleSkeletalAnimation"));

        const AZ::BehaviorEBus* virtualCharacterRequestBus =
            behaviorContext.m_ebuses.at("JoltVirtualCharacterRequestBus");
        EXPECT_TRUE(virtualCharacterRequestBus->m_events.contains("BeginContactTracking"));
        EXPECT_TRUE(virtualCharacterRequestBus->m_events.contains("CancelVelocityTowardsSteepSlopes"));
        EXPECT_TRUE(virtualCharacterRequestBus->m_events.contains("CanWalkStairs"));
        EXPECT_TRUE(virtualCharacterRequestBus->m_events.contains("CheckCollision"));
        EXPECT_TRUE(virtualCharacterRequestBus->m_events.contains("EndContactTracking"));
        EXPECT_TRUE(virtualCharacterRequestBus->m_events.contains("GetContacts"));
        EXPECT_TRUE(virtualCharacterRequestBus->m_events.contains("GetRuntimeConfiguration"));
        EXPECT_TRUE(virtualCharacterRequestBus->m_events.contains("GetUserData"));
        EXPECT_TRUE(virtualCharacterRequestBus->m_events.contains("HasCollidedWithBody"));
        EXPECT_TRUE(virtualCharacterRequestBus->m_events.contains("HasCollidedWithCharacter"));
        EXPECT_TRUE(virtualCharacterRequestBus->m_events.contains("RefreshContacts"));
        EXPECT_TRUE(virtualCharacterRequestBus->m_events.contains("StickToFloor"));
        EXPECT_TRUE(virtualCharacterRequestBus->m_events.contains("SetUserData"));
        EXPECT_TRUE(virtualCharacterRequestBus->m_events.contains("UpdateGroundVelocity"));
        EXPECT_TRUE(virtualCharacterRequestBus->m_events.contains("UpdateRuntimeConfiguration"));
        EXPECT_TRUE(virtualCharacterRequestBus->m_events.contains("WalkStairs"));

        const AZ::BehaviorEBus* worldQueryRequestBus =
            behaviorContext.m_ebuses.at("JoltWorldQueryRequestBus");
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("CollideShape"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("CaptureWorldState"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("CaptureWorldStateConfigured"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("CaptureWorldStateParts"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("CollectShapeTriangles"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("CollectTransformedShapeChildren"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("CollectTransformedShapeTriangles"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("CollideShapePoint"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("CollideShapePointAny"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("CollideTransformedShapePoint"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("CollideTransformedShapePointAny"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("ConfigureDebugCapture"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("CreateWorld"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("DestroyStateSnapshot"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("DestroyWorld"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("ExportWorldStateArchive"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("GetBodies"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("GetBodyId"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("GetDebugCaptureStatistics"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("GetGravity"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("GetRuntimeInfo"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("GetRuntimeConfiguration"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("GetSimulationConfiguration"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("GetTransformedShapeSupportingFace"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("GetTransformedShapeSurfaceNormal"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("GetWorldStateDigest"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("GetWorldStatistics"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("IsStateSnapshotValid"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("IsWorldValid"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("ImportWorldStateArchive"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("OverlapShape"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("CastShapeClosestPerBody"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("RaycastClosestBatch"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("RaycastClosestPerBody"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("RaycastShapeAll"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("RaycastShapeClosest"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("RaycastTransformedShapeAll"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("RaycastTransformedShapeClosest"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("RecaptureWorldState"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("RecaptureWorldStateConfigured"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("RestoreWorldState"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("RestoreWorldStateParts"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("SetGravity"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("StepWorld"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("UpdateSimulationConfiguration"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("UpdateRuntimeConfiguration"));
        EXPECT_TRUE(worldQueryRequestBus->m_events.contains("ValidateWorldState"));

        const AZ::BehaviorEBus* softBodyRequestBus =
            behaviorContext.m_ebuses.at("JoltSoftBodyRequestBus");
        EXPECT_TRUE(softBodyRequestBus->m_events.contains("GetCenterOfMassTransform"));
        EXPECT_TRUE(softBodyRequestBus->m_events.contains("GetDefinitionHandle"));
        EXPECT_TRUE(softBodyRequestBus->m_events.contains("GetDefinitionState"));
        EXPECT_TRUE(softBodyRequestBus->m_events.contains("CopyDefinitionDihedralBendConstraints"));
        EXPECT_TRUE(softBodyRequestBus->m_events.contains("CopyDefinitionEdgeConstraints"));
        EXPECT_TRUE(softBodyRequestBus->m_events.contains("CopyDefinitionFaces"));
        EXPECT_TRUE(softBodyRequestBus->m_events.contains("CopyDefinitionInverseBinds"));
        EXPECT_TRUE(softBodyRequestBus->m_events.contains("CopyDefinitionLongRangeConstraints"));
        EXPECT_TRUE(softBodyRequestBus->m_events.contains("CopyDefinitionMaterials"));
        EXPECT_TRUE(softBodyRequestBus->m_events.contains("CopyDefinitionRodBendTwistConstraints"));
        EXPECT_TRUE(softBodyRequestBus->m_events.contains("CopyDefinitionRodStretchShearConstraints"));
        EXPECT_TRUE(softBodyRequestBus->m_events.contains("CopyDefinitionSkinConstraints"));
        EXPECT_TRUE(softBodyRequestBus->m_events.contains("CopyDefinitionVertices"));
        EXPECT_TRUE(softBodyRequestBus->m_events.contains("CopyDefinitionVolumeConstraints"));
        EXPECT_TRUE(softBodyRequestBus->m_events.contains("GetInverseInertia"));
        EXPECT_TRUE(softBodyRequestBus->m_events.contains("GetInverseMass"));
        EXPECT_TRUE(softBodyRequestBus->m_events.contains("RecalculateMassProperties"));
        EXPECT_TRUE(softBodyRequestBus->m_events.contains("SetVertexInverseMasses"));
        EXPECT_TRUE(softBodyRequestBus->m_events.contains("SetVertexVelocities"));

        const AZ::BehaviorEBus* staticRigidBodyRequestBus =
            behaviorContext.m_ebuses.at("JoltStaticRigidBodyRequestBus");
        EXPECT_TRUE(staticRigidBodyRequestBus->m_events.contains("GetCenterOfMassTransform"));

        constexpr AZStd::array vehicleRequestBusNames = {
            "JoltMotorcycleRequestBus",
            "JoltTrackedVehicleRequestBus",
            "JoltWheeledVehicleRequestBus",
        };
        for (const char* vehicleRequestBusName : vehicleRequestBusNames)
        {
            const AZ::BehaviorEBus* vehicleRequestBus = behaviorContext.m_ebuses.at(vehicleRequestBusName);
            EXPECT_TRUE(vehicleRequestBus->m_events.contains("ApplyEngineDamping"));
            EXPECT_TRUE(vehicleRequestBus->m_events.contains("ApplyEngineTorque"));
            EXPECT_TRUE(vehicleRequestBus->m_events.contains("CalculateEngineTorque"));
            EXPECT_TRUE(vehicleRequestBus->m_events.contains("CopyAntiRollBars"));
            EXPECT_TRUE(vehicleRequestBus->m_events.contains("CopyDifferentials"));
            EXPECT_TRUE(vehicleRequestBus->m_events.contains("GetCollisionConfiguration"));
            EXPECT_TRUE(vehicleRequestBus->m_events.contains("GetEngineConfiguration"));
            EXPECT_TRUE(vehicleRequestBus->m_events.contains("GetDifferentialLimitedSlipRatio"));
            EXPECT_TRUE(vehicleRequestBus->m_events.contains("GetPowertrainState"));
            EXPECT_TRUE(vehicleRequestBus->m_events.contains("GetRuntimeConfiguration"));
            EXPECT_TRUE(vehicleRequestBus->m_events.contains("GetTransmissionConfiguration"));
            EXPECT_TRUE(vehicleRequestBus->m_events.contains("GetWheelLocalBasis"));
            EXPECT_TRUE(vehicleRequestBus->m_events.contains("GetWheelLocalTransform"));
            EXPECT_TRUE(vehicleRequestBus->m_events.contains("GetWheelWorldTransform"));
            EXPECT_TRUE(vehicleRequestBus->m_events.contains("SetPowertrainControl"));
            EXPECT_TRUE(vehicleRequestBus->m_events.contains("SetDifferentialLimitedSlipRatio"));
            EXPECT_TRUE(vehicleRequestBus->m_events.contains("SetWheelMotion"));
            EXPECT_TRUE(vehicleRequestBus->m_events.contains("UpdateAntiRollBars"));
            EXPECT_TRUE(vehicleRequestBus->m_events.contains("UpdateCollisionConfiguration"));
            EXPECT_TRUE(vehicleRequestBus->m_events.contains("UpdateDifferentials"));
            EXPECT_TRUE(vehicleRequestBus->m_events.contains("UpdateEngineConfiguration"));
            EXPECT_TRUE(vehicleRequestBus->m_events.contains("UpdateRuntimeConfiguration"));
            EXPECT_TRUE(vehicleRequestBus->m_events.contains("UpdateTransmissionConfiguration"));
        }

        const AZ::BehaviorEBus* trackedVehicleRequestBus =
            behaviorContext.m_ebuses.at("JoltTrackedVehicleRequestBus");
        EXPECT_TRUE(trackedVehicleRequestBus->m_events.contains("GetTrackConfiguration"));
        EXPECT_TRUE(trackedVehicleRequestBus->m_events.contains("SetTrackAngularVelocity"));
        EXPECT_TRUE(trackedVehicleRequestBus->m_events.contains("UpdateTrackConfiguration"));

        const AZ::BehaviorClass* broadPhaseCastRequest = behaviorContext.m_classes.at("BroadPhaseCastRequest");
        EXPECT_TRUE(broadPhaseCastRequest->m_methods.contains("SetAabbCast"));
        EXPECT_TRUE(broadPhaseCastRequest->m_methods.contains("SetRay"));

        const AZ::BehaviorClass* broadPhaseOverlapRequest = behaviorContext.m_classes.at("BroadPhaseOverlapRequest");
        EXPECT_TRUE(broadPhaseOverlapRequest->m_methods.contains("SetAabb"));
        EXPECT_TRUE(broadPhaseOverlapRequest->m_methods.contains("SetOrientedBox"));
        EXPECT_TRUE(broadPhaseOverlapRequest->m_methods.contains("SetPoint"));
        EXPECT_TRUE(broadPhaseOverlapRequest->m_methods.contains("SetSphere"));

        ASSERT_TRUE(behaviorContext.m_classes.contains("SkeletonPoseState"));
        const AZ::BehaviorClass* skeletonPoseState = behaviorContext.m_classes.at("SkeletonPoseState");
        ASSERT_TRUE(skeletonPoseState->m_properties.contains("skeletonHandle"));
        const AZ::BehaviorProperty* skeletonHandleProperty =
            skeletonPoseState->m_properties.at("skeletonHandle");
        EXPECT_TRUE(skeletonHandleProperty->m_getter);
        EXPECT_FALSE(skeletonHandleProperty->m_setter);

        behaviorContext.EnableRemoveReflection();
        reflectComponents();
        behaviorContext.DisableRemoveReflection();

        EXPECT_FALSE(behaviorContext.m_ebuses.contains("JoltVehicleNotificationBus"));
        EXPECT_FALSE(behaviorContext.m_classes.contains("SubShapeId"));
        EXPECT_FALSE(behaviorContext.m_classes.contains("SubShapeTransform"));
    }

    TEST(ComponentTests, ColliderExposesAuthoredShapeAndCompoundMetadata)
    {
        ComponentNameDictionaryScope nameDictionary;
        AZ::ComponentDescriptor* transformDescriptor = AzFramework::TransformComponent::CreateDescriptor();
        AZ::ComponentDescriptor* colliderDescriptor = ColliderComponent::CreateDescriptor();
        AZ::ComponentDescriptor* bodyDescriptor = StaticRigidBodyComponent::CreateDescriptor();
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::RegisterComponentDescriptor,
            transformDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::RegisterComponentDescriptor,
            colliderDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::RegisterComponentDescriptor,
            bodyDescriptor);

        System system(CreateComponentSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        ColliderShapeConfiguration leftShape;
        leftShape.m_shape.m_geometry = BoxShapeConfiguration{};
        leftShape.m_shape.m_userData = 101;
        leftShape.m_localTransform = AZ::Transform::CreateTranslation(-2.0f * AZ::Vector3::CreateAxisX());
        leftShape.m_compoundUserData = 11;

        ColliderShapeConfiguration rightShape;
        rightShape.m_shape.m_geometry = SphereShapeConfiguration{};
        rightShape.m_shape.m_userData = 202;
        rightShape.m_localTransform = AZ::Transform::CreateTranslation(2.0f * AZ::Vector3::CreateAxisX());
        rightShape.m_compoundUserData = 22;

        AZ::Entity entity("Metadata collider");
        entity.CreateComponent<AzFramework::TransformComponent>();
        ColliderComponent* collider =
            entity.CreateComponent<ColliderComponent>(AZStd::vector{leftShape, rightShape});
        ASSERT_TRUE(collider);
        entity.CreateComponent<StaticRigidBodyComponent>();
        entity.Init();
        entity.Activate();

        ASSERT_EQ(collider->GetShapeHandles().size(), 2);
        EXPECT_TRUE(collider->GetRootShapeHandle());

        AZ::u64 userData = 999;
        ASSERT_TRUE(collider->GetRootShapeUserData(userData));
        EXPECT_EQ(userData, 0);

        AZ::u32 childCount = 0;
        ASSERT_TRUE(collider->GetRootCompoundChildCount(childCount));
        ASSERT_EQ(childCount, 2);

        CompoundChildConfiguration child;
        ASSERT_TRUE(collider->GetRootCompoundChild(1, child));
        EXPECT_EQ(child.m_shapeHandle, collider->GetShapeHandles()[1]);
        EXPECT_TRUE(child.m_position.IsClose(2.0f * AZ::Vector3::CreateAxisX()));
        EXPECT_EQ(child.m_userData, 22);

        RaycastRequest request;
        request.m_start = WorldPosition(2.0, 0.0, 5.0);
        request.m_displacement = -10.0f * AZ::Vector3::CreateAxisZ();
        RaycastHit hit;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ASSERT_TRUE(system.RaycastClosest(worldHandle, request, hit));
        EXPECT_EQ(hit.m_shapeHandle, collider->GetRootShapeHandle());

        AZ::u32 childIndex = 0;
        ASSERT_TRUE(collider->GetRootCompoundChildIndex(hit.m_subShapeId, childIndex));
        EXPECT_EQ(childIndex, 1);

        ASSERT_TRUE(collider->GetRootShapeSubShapeUserData(hit.m_subShapeId, userData));
        EXPECT_EQ(userData, 202);

        AZ::u32 triangleUserData = 0;
        EXPECT_FALSE(collider->GetRootMeshTriangleUserData(hit.m_subShapeId, triangleUserData));

        entity.Deactivate();

        ColliderShapeConfiguration transformedShape;
        transformedShape.m_shape.m_geometry = SphereShapeConfiguration{};
        transformedShape.m_shape.m_userData = 303;
        transformedShape.m_localTransform = AZ::Transform::CreateTranslation(
            2.0f * AZ::Vector3::CreateAxisX());

        AZ::Entity transformedEntity("Transformed collider");
        transformedEntity.CreateComponent<AzFramework::TransformComponent>();
        ColliderComponent* transformedCollider = transformedEntity.CreateComponent<ColliderComponent>(
            AZStd::vector{transformedShape});
        ASSERT_TRUE(transformedCollider);
        transformedEntity.CreateComponent<StaticRigidBodyComponent>();
        transformedEntity.Init();
        transformedEntity.Activate();

        RotatedTranslatedShapeConfiguration transformedConfiguration;
        ASSERT_TRUE(transformedCollider->GetRootRotatedTranslatedConfiguration(transformedConfiguration));
        EXPECT_TRUE(transformedConfiguration.m_shapeHandle);
        EXPECT_NE(transformedConfiguration.m_shapeHandle, transformedCollider->GetRootShapeHandle());
        EXPECT_TRUE(transformedConfiguration.m_position.IsClose(2.0f * AZ::Vector3::CreateAxisX()));
        EXPECT_TRUE(transformedConfiguration.m_rotation.IsClose(AZ::Quaternion::CreateIdentity()));

        OffsetCenterOfMassShapeConfiguration offsetConfiguration;
        ScaledShapeConfiguration scaledConfiguration;
        EXPECT_FALSE(transformedCollider->GetRootOffsetCenterOfMassConfiguration(offsetConfiguration));
        EXPECT_FALSE(transformedCollider->GetRootScaledConfiguration(scaledConfiguration));

        transformedEntity.Deactivate();

        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::UnregisterComponentDescriptor,
            bodyDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::UnregisterComponentDescriptor,
            colliderDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::UnregisterComponentDescriptor,
            transformDescriptor);
        bodyDescriptor->ReleaseDescriptor();
        colliderDescriptor->ReleaseDescriptor();
        transformDescriptor->ReleaseDescriptor();
    }

    TEST(ComponentTests, ColliderExposesScriptSafeMeshMaterialsAndTriangleMetadata)
    {
        ComponentNameDictionaryScope nameDictionary;
        AZ::ComponentDescriptor* transformDescriptor = AzFramework::TransformComponent::CreateDescriptor();
        AZ::ComponentDescriptor* colliderDescriptor = ColliderComponent::CreateDescriptor();
        AZ::ComponentDescriptor* bodyDescriptor = StaticRigidBodyComponent::CreateDescriptor();
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::RegisterComponentDescriptor,
            transformDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::RegisterComponentDescriptor,
            colliderDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::RegisterComponentDescriptor,
            bodyDescriptor);

        System system(CreateComponentSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        MeshShapeConfiguration mesh;
        mesh.m_vertices = {
            AZ::Vector3(-3.0f, -1.0f, 0.0f),
            AZ::Vector3(-1.0f, -1.0f, 0.0f),
            AZ::Vector3(-2.0f, 1.0f, 0.0f),
            AZ::Vector3(1.0f, -1.0f, 0.0f),
            AZ::Vector3(3.0f, -1.0f, 0.0f),
            AZ::Vector3(2.0f, 1.0f, 0.0f),
        };
        mesh.m_triangles = {
            {.m_secondVertex = 1, .m_thirdVertex = 2, .m_userData = 17},
            {
                .m_firstVertex = 3,
                .m_secondVertex = 4,
                .m_thirdVertex = 5,
                .m_materialIndex = 1,
                .m_userData = 29,
            },
        };
        mesh.m_perTriangleUserData = true;

        ColliderShapeConfiguration colliderShape;
        colliderShape.m_shape.m_geometry = AZStd::move(mesh);
        colliderShape.m_materials = {
            MaterialConfiguration{.m_debugName = "FirstMeshMaterial"},
            MaterialConfiguration{.m_debugName = "SecondMeshMaterial"},
        };
        colliderShape.m_localTransform = AZ::Transform::CreateTranslation(
            5.0f * AZ::Vector3::CreateAxisX());

        AZ::Entity entity("Mesh collider");
        entity.CreateComponent<AzFramework::TransformComponent>();
        ColliderComponent* collider = entity.CreateComponent<ColliderComponent>(
            AZStd::vector{colliderShape});
        ASSERT_TRUE(collider);
        entity.CreateComponent<StaticRigidBodyComponent>();
        entity.Init();
        entity.Activate();

        MaterialCollection materials = collider->GetRootMeshMaterials();
        ASSERT_EQ(materials.GetMaterialCount(), 2);
        EXPECT_EQ(materials.GetRequiredMaterialCount(), 2);
        EXPECT_FALSE(materials.HasOverflow());
        EXPECT_TRUE(materials.GetMaterial(0));
        EXPECT_TRUE(materials.GetMaterial(1));
        EXPECT_NE(materials.GetMaterial(0), materials.GetMaterial(1));

        RaycastRequest request;
        request.m_start = WorldPosition(3.0, 0.0, 1.0);
        request.m_displacement = -2.0f * AZ::Vector3::CreateAxisZ();
        RaycastHit hit;
        ASSERT_TRUE(system.RaycastClosest(system.GetDefaultWorldHandle(), request, hit));

        AZ::u32 materialIndex = AZStd::numeric_limits<AZ::u32>::max();
        ASSERT_TRUE(collider->GetRootMeshTriangleMaterialIndex(hit.m_subShapeId, materialIndex));
        EXPECT_EQ(materialIndex, 0);
        AZ::u32 userData = 0;
        ASSERT_TRUE(collider->GetRootMeshTriangleUserData(hit.m_subShapeId, userData));
        EXPECT_EQ(userData, 17);

        request.m_start = WorldPosition(7.0, 0.0, 1.0);
        ASSERT_TRUE(system.RaycastClosest(system.GetDefaultWorldHandle(), request, hit));
        ASSERT_TRUE(collider->GetRootMeshTriangleMaterialIndex(hit.m_subShapeId, materialIndex));
        EXPECT_EQ(materialIndex, 1);
        ASSERT_TRUE(collider->GetRootMeshTriangleUserData(hit.m_subShapeId, userData));
        EXPECT_EQ(userData, 29);

        entity.Deactivate();

        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::UnregisterComponentDescriptor,
            bodyDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::UnregisterComponentDescriptor,
            colliderDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::UnregisterComponentDescriptor,
            transformDescriptor);
        bodyDescriptor->ReleaseDescriptor();
        colliderDescriptor->ReleaseDescriptor();
        transformDescriptor->ReleaseDescriptor();
    }

    TEST(ComponentTests, ColliderExposesScriptSafeHeightfieldInspectionAndMutation)
    {
        ComponentNameDictionaryScope nameDictionary;
        AZ::ComponentDescriptor* transformDescriptor = AzFramework::TransformComponent::CreateDescriptor();
        AZ::ComponentDescriptor* colliderDescriptor = ColliderComponent::CreateDescriptor();
        AZ::ComponentDescriptor* bodyDescriptor = StaticRigidBodyComponent::CreateDescriptor();
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::RegisterComponentDescriptor,
            transformDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::RegisterComponentDescriptor,
            colliderDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::RegisterComponentDescriptor,
            bodyDescriptor);

        System system(CreateComponentSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        HeightfieldShapeConfiguration heightfield;
        heightfield.m_sampleCount = 4;
        heightfield.m_blockSize = 2;
        heightfield.m_bitsPerSample = 16;
        heightfield.m_materialCapacity = 2;
        heightfield.m_heights = {
            0.0f,
            0.0f,
            0.0f,
            0.0f,
            1.0f,
            1.0f,
            1.0f,
            1.0f,
            2.0f,
            2.0f,
            2.0f,
            2.0f,
            3.0f,
            3.0f,
            3.0f,
            3.0f,
        };
        heightfield.m_materialIndices.resize(9, 0);

        ColliderShapeConfiguration colliderShape;
        colliderShape.m_shape.m_geometry = heightfield;
        colliderShape.m_materials = {
            MaterialConfiguration{.m_debugName = "FirstHeightfieldMaterial"},
            MaterialConfiguration{.m_debugName = "SecondHeightfieldMaterial"},
        };

        AZ::Entity entity("Heightfield collider");
        entity.CreateComponent<AzFramework::TransformComponent>();
        ColliderComponent* collider = entity.CreateComponent<ColliderComponent>(
            AZStd::vector{colliderShape});
        ASSERT_TRUE(collider);
        entity.CreateComponent<StaticRigidBodyComponent>();
        entity.Init();
        entity.Activate();

        HeightfieldState state;
        ASSERT_TRUE(collider->GetRootHeightfieldState(state));
        EXPECT_EQ(state.m_sampleCount, 4);
        EXPECT_EQ(state.m_materialCount, 2);

        RotatedTranslatedShapeConfiguration internalRotation;
        EXPECT_FALSE(collider->GetRootRotatedTranslatedConfiguration(internalRotation));

        AZ::Vector3 position = AZ::Vector3::CreateZero();
        ASSERT_TRUE(collider->GetRootHeightfieldPosition(1, 2, position));
        EXPECT_NEAR(position.GetX(), 1.0f, 1.0e-4f);
        EXPECT_NEAR(position.GetY(), 2.0f, 1.0e-4f);
        EXPECT_NEAR(position.GetZ(), 2.0f, 0.01f);

        bool noCollision = true;
        ASSERT_TRUE(collider->IsRootHeightfieldNoCollision(1, 2, noCollision));
        EXPECT_FALSE(noCollision);

        const HeightfieldRegion region{
            .m_columnCount = 2,
            .m_rowCount = 2,
        };
        HeightfieldSampleCollection samples = collider->GetRootHeightfieldHeights(region);
        ASSERT_EQ(samples.GetSampleCount(), 4);
        EXPECT_EQ(samples.GetRequiredSampleCount(), 4);
        EXPECT_FALSE(samples.HasOverflow());
        EXPECT_NEAR(samples.GetSample(2), 1.0f, 0.01f);

        samples.Clear();
        EXPECT_TRUE(samples.AddSample(0.25f));
        EXPECT_TRUE(samples.AddSample(0.5f));
        EXPECT_TRUE(samples.AddSample(0.75f));
        EXPECT_TRUE(samples.AddSample(1.0f));
        EXPECT_TRUE(collider->UpdateRootHeightfieldHeights(
            region,
            samples,
            HeightfieldUpdateConfiguration{}));

        samples = collider->GetRootHeightfieldHeights(region);
        ASSERT_EQ(samples.GetSampleCount(), 4);
        EXPECT_NEAR(samples.GetSample(0), 0.25f, 0.01f);
        EXPECT_NEAR(samples.GetSample(3), 1.0f, 0.01f);

        MaterialCollection materials = collider->GetRootHeightfieldMaterials();
        ASSERT_EQ(materials.GetMaterialCount(), 2);
        EXPECT_FALSE(materials.HasOverflow());

        HeightfieldMaterialIndexCollection materialIndices;
        EXPECT_TRUE(materialIndices.AddIndex(1));
        EXPECT_TRUE(materialIndices.AddIndex(0));
        EXPECT_TRUE(materialIndices.AddIndex(0));
        EXPECT_TRUE(materialIndices.AddIndex(1));
        EXPECT_TRUE(collider->UpdateRootHeightfieldMaterials(
            region,
            materialIndices,
            materials,
            false));

        materialIndices = collider->GetRootHeightfieldMaterialIndices(region);
        ASSERT_EQ(materialIndices.GetIndexCount(), 4);
        EXPECT_EQ(materialIndices.GetIndex(0), 1);
        EXPECT_EQ(materialIndices.GetIndex(3), 1);
        EXPECT_FALSE(materialIndices.HasOverflow());

        entity.Deactivate();

        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::UnregisterComponentDescriptor,
            bodyDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::UnregisterComponentDescriptor,
            colliderDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::UnregisterComponentDescriptor,
            transformDescriptor);
        bodyDescriptor->ReleaseDescriptor();
        colliderDescriptor->ReleaseDescriptor();
        transformDescriptor->ReleaseDescriptor();
    }

    TEST(ComponentTests, WorldQueryBusUsesBoundedCollectionsAndReportsOverflow)
    {
        ComponentNameDictionaryScope nameDictionary;
        SystemComponent component;
        component.Activate();

        ISystem* system = AZ::Interface<ISystem>::Get();
        if (!system)
        {
            component.Deactivate();
            FAIL() << "The system component did not create its runtime system.";
            return;
        }

        WorldHandle worldHandle;
        WorldQueryRequestBus::BroadcastResult(
            worldHandle,
            &IWorldQueryRequests::GetDefaultWorldHandle);
        EXPECT_TRUE(worldHandle);

        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = BoxShapeConfiguration{};
        const ShapeHandle shapeHandle = system->CreateShape(worldHandle, shapeConfiguration);
        EXPECT_TRUE(shapeHandle);

        BodyConfiguration bodyConfiguration;
        bodyConfiguration.m_shapeHandle = shapeHandle;
        bodyConfiguration.m_motionType = MotionType::Static;
        bodyConfiguration.m_objectLayer = DefaultLayers::NonMoving;
        bodyConfiguration.m_activate = false;
        const BodyHandle firstBodyHandle = system->CreateBody(worldHandle, bodyConfiguration);
        bodyConfiguration.m_transform.m_position.m_z = 2.0;
        const BodyHandle secondBodyHandle = system->CreateBody(worldHandle, bodyConfiguration);
        EXPECT_TRUE(firstBodyHandle);
        EXPECT_TRUE(secondBodyHandle);

        BodyId firstBodyId;
        bool foundBodyId = false;
        WorldQueryRequestBus::BroadcastResult(
            foundBodyId,
            &IWorldQueryRequests::GetBodyId,
            worldHandle,
            firstBodyHandle,
            firstBodyId);
        EXPECT_TRUE(foundBodyId);
        EXPECT_TRUE(firstBodyId);

        BodyCollection bodies;
        WorldQueryRequestBus::BroadcastResult(
            bodies,
            &IWorldQueryRequests::GetBodies,
            worldHandle,
            BodyKind::Rigid,
            false,
            1);
        EXPECT_EQ(bodies.GetBodyCount(), 1);
        EXPECT_EQ(bodies.GetRequiredBodyCount(), 2);
        EXPECT_TRUE(bodies.HasOverflow());
        EXPECT_EQ(bodies.GetBody(0), firstBodyHandle);
        EXPECT_FALSE(bodies.GetBody(1));

        WorldStatistics statistics;
        bool foundStatistics = false;
        WorldQueryRequestBus::BroadcastResult(
            foundStatistics,
            &IWorldQueryRequests::GetWorldStatistics,
            worldHandle,
            statistics);
        EXPECT_TRUE(foundStatistics);
        EXPECT_EQ(statistics.m_bodyCount, 2);
        EXPECT_EQ(statistics.m_bodySnapshotCount, 0);
        EXPECT_EQ(statistics.m_stateSnapshotCount, 0);

        WorldStateDigest digest;
        bool foundDigest = false;
        WorldQueryRequestBus::BroadcastResult(
            foundDigest,
            &IWorldQueryRequests::GetWorldStateDigest,
            worldHandle,
            digest);
        EXPECT_TRUE(foundDigest);
        EXPECT_GT(digest.m_stateByteCount, 0);

        StateSnapshotHandle snapshotHandle;
        WorldQueryRequestBus::BroadcastResult(
            snapshotHandle,
            &IWorldQueryRequests::CaptureWorldState,
            worldHandle);
        ASSERT_TRUE(snapshotHandle);

        ASSERT_TRUE(system->GetWorldStatistics(worldHandle, statistics));
        EXPECT_EQ(statistics.m_stateSnapshotCount, 1);

        bool snapshotIsValid = false;
        WorldQueryRequestBus::BroadcastResult(
            snapshotIsValid,
            &IWorldQueryRequests::IsStateSnapshotValid,
            worldHandle,
            snapshotHandle);
        EXPECT_TRUE(snapshotIsValid);

        WorldTransform movedTransform;
        movedTransform.m_position.m_z = 5.0;
        ASSERT_TRUE(system->SetBodyTransform(
            worldHandle,
            firstBodyHandle,
            movedTransform,
            false));

        StateValidationResult validationResult;
        bool validated = false;
        WorldQueryRequestBus::BroadcastResult(
            validated,
            &IWorldQueryRequests::ValidateWorldState,
            worldHandle,
            snapshotHandle,
            validationResult);
        EXPECT_TRUE(validated);
        EXPECT_FALSE(validationResult.m_matches);

        BodyState bodyStateAfterValidation;
        ASSERT_TRUE(system->GetBodyState(worldHandle, firstBodyHandle, bodyStateAfterValidation));
        EXPECT_DOUBLE_EQ(bodyStateAfterValidation.m_transform.m_position.m_z, 5.0);

        WorldQueryRequestBus::BroadcastResult(
            validated,
            &IWorldQueryRequests::ValidateWorldState,
            worldHandle,
            snapshotHandle,
            validationResult);
        EXPECT_TRUE(validated);
        EXPECT_FALSE(validationResult.m_matches);

        bool restored = false;
        WorldQueryRequestBus::BroadcastResult(
            restored,
            &IWorldQueryRequests::RestoreWorldState,
            worldHandle,
            snapshotHandle);
        EXPECT_TRUE(restored);

        BodyState restoredBodyState;
        ASSERT_TRUE(system->GetBodyState(worldHandle, firstBodyHandle, restoredBodyState));
        EXPECT_DOUBLE_EQ(restoredBodyState.m_transform.m_position.m_z, 0.0);

        bool recaptured = false;
        WorldQueryRequestBus::BroadcastResult(
            recaptured,
            &IWorldQueryRequests::RecaptureWorldState,
            worldHandle,
            snapshotHandle);
        EXPECT_TRUE(recaptured);

        movedTransform.m_position.m_z = 7.0;
        ASSERT_TRUE(system->SetBodyTransform(
            worldHandle,
            firstBodyHandle,
            movedTransform,
            false));
        WorldQueryRequestBus::BroadcastResult(
            restored,
            &IWorldQueryRequests::RestoreWorldState,
            worldHandle,
            snapshotHandle);
        EXPECT_TRUE(restored);

        ASSERT_TRUE(system->GetBodyState(worldHandle, firstBodyHandle, restoredBodyState));
        EXPECT_DOUBLE_EQ(restoredBodyState.m_transform.m_position.m_z, 0.0);

        StateSnapshotConfiguration filteredConfiguration;
        filteredConfiguration.m_flags = StateSnapshotFlags::Bodies;
        filteredConfiguration.m_filterBodies = true;
        const AZStd::vector<BodyHandle> filteredBodies = {firstBodyHandle};
        StateSnapshotHandle filteredSnapshotHandle;
        WorldQueryRequestBus::BroadcastResult(
            filteredSnapshotHandle,
            &IWorldQueryRequests::CaptureWorldStateConfigured,
            worldHandle,
            filteredConfiguration,
            filteredBodies);
        ASSERT_TRUE(filteredSnapshotHandle);

        bool recapturedFilteredState = false;
        WorldQueryRequestBus::BroadcastResult(
            recapturedFilteredState,
            &IWorldQueryRequests::RecaptureWorldStateConfigured,
            worldHandle,
            filteredSnapshotHandle,
            filteredConfiguration,
            filteredBodies);
        EXPECT_TRUE(recapturedFilteredState);

        bool destroyedSnapshot = false;
        WorldQueryRequestBus::BroadcastResult(
            destroyedSnapshot,
            &IWorldQueryRequests::DestroyStateSnapshot,
            worldHandle,
            filteredSnapshotHandle);
        EXPECT_TRUE(destroyedSnapshot);

        WorldQueryRequestBus::BroadcastResult(
            snapshotIsValid,
            &IWorldQueryRequests::IsStateSnapshotValid,
            worldHandle,
            filteredSnapshotHandle);
        EXPECT_FALSE(snapshotIsValid);
        ASSERT_TRUE(system->GetWorldStatistics(worldHandle, statistics));
        EXPECT_EQ(statistics.m_stateSnapshotCount, 1);

        RaycastRequest request;
        request.m_start.m_z = -2.0;
        request.m_displacement = AZ::Vector3(0.0f, 0.0f, 6.0f);

        ClosestRaycastResult closestResult;
        WorldQueryRequestBus::BroadcastResult(
            closestResult,
            &IWorldQueryRequests::RaycastClosest,
            worldHandle,
            request);
        EXPECT_TRUE(closestResult.m_found);
        EXPECT_EQ(closestResult.m_hit.m_bodyHandle, firstBodyHandle);

        ShapeRaycastRequest shapeRaycastRequest;
        shapeRaycastRequest.m_shapeHandle = shapeHandle;
        shapeRaycastRequest.m_start = AZ::Vector3::CreateAxisX(-2.0f);
        shapeRaycastRequest.m_displacement = AZ::Vector3::CreateAxisX(4.0f);
        ClosestShapeRaycastResult closestShapeRaycastResult;
        WorldQueryRequestBus::BroadcastResult(
            closestShapeRaycastResult,
            &IWorldQueryRequests::RaycastShapeClosest,
            worldHandle,
            shapeRaycastRequest);
        ASSERT_TRUE(closestShapeRaycastResult.m_found);
        EXPECT_NEAR(closestShapeRaycastResult.m_hit.m_fraction, 0.375f, 1.0e-4f);

        ShapeRaycastHitCollection shapeRaycastHits;
        WorldQueryRequestBus::BroadcastResult(
            shapeRaycastHits,
            &IWorldQueryRequests::RaycastShapeAll,
            worldHandle,
            shapeRaycastRequest,
            1);
        EXPECT_EQ(shapeRaycastHits.GetHitCount(), 1);
        EXPECT_EQ(shapeRaycastHits.GetRequiredHitCount(), 1);
        EXPECT_FALSE(shapeRaycastHits.HasOverflow());
        EXPECT_EQ(
            shapeRaycastHits.GetHit(0).m_subShapeId,
            closestShapeRaycastResult.m_hit.m_subShapeId);

        ShapePointHitCollection shapePointHits;
        WorldQueryRequestBus::BroadcastResult(
            shapePointHits,
            &IWorldQueryRequests::CollideShapePoint,
            worldHandle,
            shapeHandle,
            AZ::Vector3::CreateZero(),
            1);
        EXPECT_EQ(shapePointHits.GetHitCount(), 1);
        EXPECT_EQ(shapePointHits.GetRequiredHitCount(), 1);
        EXPECT_FALSE(shapePointHits.HasOverflow());
        EXPECT_EQ(shapePointHits.GetHit(0).m_subShapeId, closestShapeRaycastResult.m_hit.m_subShapeId);

        bool foundShapePoint = false;
        WorldQueryRequestBus::BroadcastResult(
            foundShapePoint,
            &IWorldQueryRequests::CollideShapePointAny,
            worldHandle,
            shapeHandle,
            AZ::Vector3::CreateZero());
        EXPECT_TRUE(foundShapePoint);

        ShapeTriangleCollectionRequest shapeTriangleRequest;
        shapeTriangleRequest.m_shapeHandle = shapeHandle;
        shapeTriangleRequest.m_boundsHalfExtents = AZ::Vector3(2.0f);
        ShapeTriangleCollection shapeTriangles;
        WorldQueryRequestBus::BroadcastResult(
            shapeTriangles,
            &IWorldQueryRequests::CollectShapeTriangles,
            worldHandle,
            shapeTriangleRequest,
            1);
        EXPECT_EQ(shapeTriangles.GetTriangleCount(), 1);
        EXPECT_EQ(shapeTriangles.GetRequiredTriangleCount(), 12);
        EXPECT_TRUE(shapeTriangles.HasOverflow());

        RaycastRequestCollection batchRequests;
        EXPECT_TRUE(batchRequests.AddRequest(request));
        RaycastRequest missedRequest = request;
        missedRequest.m_start.m_x = 10.0;
        EXPECT_TRUE(batchRequests.AddRequest(missedRequest));
        ClosestRaycastResultCollection batchResults;
        WorldQueryRequestBus::BroadcastResult(
            batchResults,
            &IWorldQueryRequests::RaycastClosestBatch,
            worldHandle,
            batchRequests);
        EXPECT_EQ(batchResults.GetResultCount(), 2);
        EXPECT_EQ(batchResults.GetRequiredResultCount(), 2);
        EXPECT_FALSE(batchResults.HasOverflow());
        EXPECT_TRUE(batchResults.GetResult(0).m_found);
        EXPECT_EQ(batchResults.GetResult(0).m_hit.m_bodyHandle, firstBodyHandle);
        EXPECT_FALSE(batchResults.GetResult(1).m_found);
        EXPECT_FALSE(batchResults.GetResult(2).m_found);

        RaycastHitCollection hits;
        WorldQueryRequestBus::BroadcastResult(
            hits,
            &IWorldQueryRequests::RaycastAll,
            worldHandle,
            request,
            1);
        EXPECT_EQ(hits.GetHitCount(), 1);
        EXPECT_EQ(hits.GetRequiredHitCount(), 2);
        EXPECT_TRUE(hits.HasOverflow());
        EXPECT_TRUE(hits.GetHit(0).m_bodyHandle);
        EXPECT_FALSE(hits.GetHit(1).m_bodyHandle);

        WorldQueryRequestBus::BroadcastResult(
            hits,
            &IWorldQueryRequests::RaycastClosestPerBody,
            worldHandle,
            request,
            2);
        EXPECT_EQ(hits.GetHitCount(), 2);
        EXPECT_EQ(hits.GetRequiredHitCount(), 2);
        EXPECT_FALSE(hits.HasOverflow());

        ShapeOverlapRequest shapeOverlapRequest;
        shapeOverlapRequest.m_shapeHandle = shapeHandle;
        shapeOverlapRequest.m_faceCollectionMode = FaceCollectionMode::Collect;
        ShapeOverlapHitCollection shapeOverlapHits;
        WorldQueryRequestBus::BroadcastResult(
            shapeOverlapHits,
            &IWorldQueryRequests::CollideShape,
            worldHandle,
            shapeOverlapRequest,
            1);
        ASSERT_EQ(shapeOverlapHits.GetHitCount(), 1);
        EXPECT_EQ(shapeOverlapHits.GetQueryFaceVertexCount(0), 4);
        EXPECT_EQ(shapeOverlapHits.GetTargetFaceVertexCount(0), 4);
        EXPECT_NE(shapeOverlapHits.GetQueryFaceVertex(0, 0), WorldPosition{});
        EXPECT_NE(shapeOverlapHits.GetTargetFaceVertex(0, 0), WorldPosition{});
        EXPECT_EQ(shapeOverlapHits.GetQueryFaceVertex(0, 4), WorldPosition{});
        EXPECT_EQ(shapeOverlapHits.GetTargetFaceVertex(1, 0), WorldPosition{});

        ShapeCastRequest shapeCastRequest;
        shapeCastRequest.m_shapeHandle = shapeHandle;
        shapeCastRequest.m_start.m_position.m_x = -2.0;
        shapeCastRequest.m_displacement.SetX(4.0f);
        shapeCastRequest.m_faceCollectionMode = FaceCollectionMode::Collect;
        ClosestShapeCastResult closestShapeCastResult;
        WorldQueryRequestBus::BroadcastResult(
            closestShapeCastResult,
            &IWorldQueryRequests::CastShapeClosest,
            worldHandle,
            shapeCastRequest);
        ASSERT_TRUE(closestShapeCastResult.m_found);
        EXPECT_EQ(closestShapeCastResult.GetQueryFaceVertexCount(), 4);
        EXPECT_EQ(closestShapeCastResult.GetTargetFaceVertexCount(), 4);
        EXPECT_NE(closestShapeCastResult.GetQueryFaceVertex(0), WorldPosition{});
        EXPECT_NE(closestShapeCastResult.GetTargetFaceVertex(0), WorldPosition{});
        EXPECT_EQ(closestShapeCastResult.GetQueryFaceVertex(4), WorldPosition{});

        bool foundAny = false;
        WorldQueryRequestBus::BroadcastResult(
            foundAny,
            &IWorldQueryRequests::RaycastAny,
            worldHandle,
            request);
        EXPECT_TRUE(foundAny);

        SupportingFaceRequest faceRequest;
        faceRequest.m_bodyHandle = firstBodyHandle;
        SupportingFaceVertexCollection vertices;
        WorldQueryRequestBus::BroadcastResult(
            vertices,
            &IWorldQueryRequests::GetSupportingFace,
            worldHandle,
            faceRequest,
            2);
        EXPECT_EQ(vertices.GetVertexCount(), 2);
        EXPECT_EQ(vertices.GetRequiredVertexCount(), 4);
        EXPECT_TRUE(vertices.HasOverflow());

        TriangleCollectionRequest triangleRequest;
        triangleRequest.m_bodyHandle = firstBodyHandle;
        triangleRequest.m_bounds.m_halfExtents = AZ::Vector3(2.0f);
        TransformedTriangleCollection triangles;
        WorldQueryRequestBus::BroadcastResult(
            triangles,
            &IWorldQueryRequests::CollectTriangles,
            worldHandle,
            triangleRequest,
            1);
        EXPECT_EQ(triangles.GetTriangleCount(), 1);
        EXPECT_GT(triangles.GetRequiredTriangleCount(), 1);
        EXPECT_TRUE(triangles.HasOverflow());

        ShapeCollectionRequest shapeCollectionRequest;
        shapeCollectionRequest.m_bounds.m_halfExtents = AZ::Vector3(0.75f);
        TransformedShapeCollection transformedShapes;
        WorldQueryRequestBus::BroadcastResult(
            transformedShapes,
            &IWorldQueryRequests::CollectShapesInBounds,
            worldHandle,
            shapeCollectionRequest,
            1);
        ASSERT_EQ(transformedShapes.GetShapeCount(), 1);
        ASSERT_EQ(transformedShapes.GetRequiredShapeCount(), 1);
        ASSERT_FALSE(transformedShapes.HasOverflow());
        const TransformedShape transformedShape = transformedShapes.GetShape(0);
        ASSERT_TRUE(transformedShape);
        EXPECT_EQ(transformedShape.GetBodyHandle(), firstBodyHandle);

        TransformedShapeRaycastRequest transformedRaycastRequest;
        transformedRaycastRequest.m_start.m_x = -2.0;
        transformedRaycastRequest.m_displacement = AZ::Vector3::CreateAxisX(4.0f);
        WorldQueryRequestBus::BroadcastResult(
            closestResult,
            &IWorldQueryRequests::RaycastTransformedShapeClosest,
            worldHandle,
            transformedShape,
            transformedRaycastRequest);
        ASSERT_TRUE(closestResult.m_found);
        EXPECT_EQ(closestResult.m_hit.m_bodyHandle, firstBodyHandle);

        WorldQueryRequestBus::BroadcastResult(
            hits,
            &IWorldQueryRequests::RaycastTransformedShapeAll,
            worldHandle,
            transformedShape,
            transformedRaycastRequest,
            1);
        EXPECT_EQ(hits.GetHitCount(), 1);
        EXPECT_EQ(hits.GetRequiredHitCount(), 1);
        EXPECT_FALSE(hits.HasOverflow());

        OverlapHitCollection transformedPointHits;
        WorldQueryRequestBus::BroadcastResult(
            transformedPointHits,
            &IWorldQueryRequests::CollideTransformedShapePoint,
            worldHandle,
            transformedShape,
            WorldPosition{},
            1);
        EXPECT_EQ(transformedPointHits.GetHitCount(), 1);
        EXPECT_EQ(transformedPointHits.GetRequiredHitCount(), 1);
        EXPECT_FALSE(transformedPointHits.HasOverflow());

        bool foundTransformedPoint = false;
        WorldQueryRequestBus::BroadcastResult(
            foundTransformedPoint,
            &IWorldQueryRequests::CollideTransformedShapePointAny,
            worldHandle,
            transformedShape,
            WorldPosition{});
        EXPECT_TRUE(foundTransformedPoint);

        TransformedShapeCollection childShapes;
        WorldQueryRequestBus::BroadcastResult(
            childShapes,
            &IWorldQueryRequests::CollectTransformedShapeChildren,
            worldHandle,
            transformedShape,
            shapeCollectionRequest.m_bounds,
            1);
        EXPECT_EQ(childShapes.GetShapeCount(), 1);
        EXPECT_EQ(childShapes.GetRequiredShapeCount(), 1);
        EXPECT_FALSE(childShapes.HasOverflow());

        WorldQueryRequestBus::BroadcastResult(
            triangles,
            &IWorldQueryRequests::CollectTransformedShapeTriangles,
            worldHandle,
            transformedShape,
            shapeCollectionRequest.m_bounds,
            1);
        EXPECT_EQ(triangles.GetTriangleCount(), 1);
        EXPECT_EQ(triangles.GetRequiredTriangleCount(), 12);
        EXPECT_TRUE(triangles.HasOverflow());

        SurfaceNormalResult surfaceNormalResult;
        WorldQueryRequestBus::BroadcastResult(
            surfaceNormalResult,
            &IWorldQueryRequests::GetTransformedShapeSurfaceNormal,
            worldHandle,
            transformedShape,
            closestResult.m_hit.m_subShapeId,
            closestResult.m_hit.m_position);
        EXPECT_TRUE(surfaceNormalResult.m_found);
        EXPECT_TRUE(surfaceNormalResult.m_normal.IsClose(-AZ::Vector3::CreateAxisX()));

        WorldQueryRequestBus::BroadcastResult(
            vertices,
            &IWorldQueryRequests::GetTransformedShapeSupportingFace,
            worldHandle,
            transformedShape,
            closestResult.m_hit.m_subShapeId,
            AZ::Vector3::CreateAxisX(),
            2);
        EXPECT_EQ(vertices.GetVertexCount(), 2);
        EXPECT_EQ(vertices.GetRequiredVertexCount(), 4);
        EXPECT_TRUE(vertices.HasOverflow());

        WorldConfiguration additionalWorldConfiguration = CreateComponentSystemConfiguration().m_defaultWorld;
        additionalWorldConfiguration.m_name = AZ::Name("ScriptWorld");
        additionalWorldConfiguration.m_collectActivationEvents = true;
        additionalWorldConfiguration.m_collectContactEvents = true;
        WorldHandle additionalWorldHandle;
        WorldQueryRequestBus::BroadcastResult(
            additionalWorldHandle,
            &IWorldQueryRequests::CreateWorld,
            additionalWorldConfiguration);
        ASSERT_TRUE(additionalWorldHandle);
        EXPECT_NE(additionalWorldHandle, worldHandle);

        bool additionalWorldIsValid = false;
        WorldQueryRequestBus::BroadcastResult(
            additionalWorldIsValid,
            &IWorldQueryRequests::IsWorldValid,
            additionalWorldHandle);
        EXPECT_TRUE(additionalWorldIsValid);

        ComponentWorldNotifications notifications;
        notifications.BusConnect(additionalWorldHandle);

        const ShapeHandle additionalShapeHandle = system->CreateShape(
            additionalWorldHandle,
            shapeConfiguration);
        ASSERT_TRUE(additionalShapeHandle);

        BodyConfiguration additionalBodyConfiguration;
        additionalBodyConfiguration.m_shapeHandle = additionalShapeHandle;
        additionalBodyConfiguration.m_motionType = MotionType::Static;
        additionalBodyConfiguration.m_objectLayer = DefaultLayers::NonMoving;
        additionalBodyConfiguration.m_activate = false;
        const BodyHandle floorBodyHandle = system->CreateBody(
            additionalWorldHandle,
            additionalBodyConfiguration);
        ASSERT_TRUE(floorBodyHandle);

        additionalBodyConfiguration.m_transform.m_position.m_z = 0.75;
        additionalBodyConfiguration.m_motionType = MotionType::Dynamic;
        additionalBodyConfiguration.m_objectLayer = DefaultLayers::Moving;
        additionalBodyConfiguration.m_activate = true;
        const BodyHandle movingBodyHandle = system->CreateBody(
            additionalWorldHandle,
            additionalBodyConfiguration);
        ASSERT_TRUE(movingBodyHandle);
        ASSERT_TRUE(system->SetBodyMoveEventsEnabled(
            additionalWorldHandle,
            movingBodyHandle,
            true));

        SimulationResult simulationResult;
        WorldQueryRequestBus::BroadcastResult(
            simulationResult,
            &IWorldQueryRequests::StepWorld,
            additionalWorldHandle,
            1.0f / 60.0f);
        EXPECT_TRUE(simulationResult);
        EXPECT_EQ(simulationResult.m_stepCount, 1);
        EXPECT_GT(notifications.m_activationCount, 0);
        EXPECT_GT(notifications.m_bodyMoveCount, 0);
        EXPECT_GT(notifications.m_contactCount, 0);
        EXPECT_GT(notifications.m_lastContactPointCount, 0);
        EXPECT_TRUE(
            notifications.m_lastContact.m_firstBodyHandle == floorBodyHandle
            || notifications.m_lastContact.m_secondBodyHandle == floorBodyHandle);
        EXPECT_TRUE(notifications.m_lastBodyMove.m_bodyHandle == movingBodyHandle);

        const AZ::u32 activationCount = notifications.m_activationCount;
        const AZ::u32 bodyMoveCount = notifications.m_bodyMoveCount;
        const AZ::u32 contactCount = notifications.m_contactCount;
        AZ::TickBus::Broadcast(
            &AZ::TickEvents::OnTick,
            0.0f,
            AZ::ScriptTimePoint{});
        EXPECT_EQ(notifications.m_activationCount, activationCount);
        EXPECT_EQ(notifications.m_bodyMoveCount, bodyMoveCount);
        EXPECT_EQ(notifications.m_contactCount, contactCount);

        notifications.BusDisconnect();

        bool destroyedWorld = false;
        WorldQueryRequestBus::BroadcastResult(
            destroyedWorld,
            &IWorldQueryRequests::DestroyWorld,
            additionalWorldHandle);
        EXPECT_TRUE(destroyedWorld);

        WorldQueryRequestBus::BroadcastResult(
            additionalWorldIsValid,
            &IWorldQueryRequests::IsWorldValid,
            additionalWorldHandle);
        EXPECT_FALSE(additionalWorldIsValid);

        component.Deactivate();
    }

    TEST(ComponentTests, HairOwnsDefinitionAutoUpdatesAndRespondsToUniformScale)
    {
        ComponentNameDictionaryScope nameDictionary;
        AZ::ComponentDescriptor* transformDescriptor = AzFramework::TransformComponent::CreateDescriptor();
        AZ::ComponentDescriptor* hairDescriptor = HairComponent::CreateDescriptor();
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::RegisterComponentDescriptor,
            transformDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::RegisterComponentDescriptor,
            hairDescriptor);

        System system(CreateComponentSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        AZ::Entity entity("Hair");
        entity.CreateComponent<AzFramework::TransformComponent>();
        HairComponent* hair = entity.CreateComponent<HairComponent>();
        ASSERT_TRUE(hair);
        entity.Init();
        entity.Activate();

        const WorldHandle worldHandle = hair->GetWorldHandle();
        const HairHandle initialHairHandle = hair->GetHairHandle();
        ASSERT_TRUE(initialHairHandle);
        EXPECT_TRUE(system.IsValid(worldHandle, initialHairHandle));
        WorldStatistics statistics;
        ASSERT_TRUE(system.GetWorldStatistics(worldHandle, statistics));
        EXPECT_EQ(statistics.m_hairCount, 1);
        EXPECT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));

        HairReadbackResult readbackResult;
        ASSERT_TRUE(hair->QueryReadback({}, readbackResult));
        EXPECT_EQ(readbackResult.m_vertexStates.m_requiredHitCount, 3);
        EXPECT_EQ(readbackResult.m_renderPositions.m_requiredHitCount, 3);
        EXPECT_EQ(readbackResult.m_scalpPositions.m_requiredHitCount, 0);
        EXPECT_EQ(readbackResult.m_gridCells.m_requiredHitCount, 32 * 32 * 32);

        EXPECT_EQ(hair->CopyVertexStates().size(), 3);
        EXPECT_EQ(hair->CopyRenderPositions().size(), 3);
        EXPECT_TRUE(hair->CopyScalpPositions().empty());
        EXPECT_EQ(hair->CopyGridCellStates().size(), 32 * 32 * 32);
        EXPECT_EQ(hair->CopyNeutralDensity().size(), 32 * 32 * 32);
        EXPECT_TRUE(hair->CopySkinnedScalpVertices(
            AZ::Transform::CreateIdentity(),
            {}).empty());
        const HairDefinitionState definitionState = hair->GetDefinitionState();
        EXPECT_EQ(definitionState.m_simulationVertexCount, 3);
        EXPECT_EQ(definitionState.m_renderVertexCount, 3);

        AZ::Transform scalpToHeadTransform = AZ::Transform::CreateIdentity();
        scalpToHeadTransform.SetTranslation(AZ::Vector3::CreateAxisZ(0.25f));
        ASSERT_TRUE(hair->SetScalpToHeadTransform(scalpToHeadTransform));
        HairState hairState = hair->GetState();
        EXPECT_TRUE(hairState.m_scalpToHeadTransform.IsClose(scalpToHeadTransform));

        AZ::Transform transform = AZ::Transform::CreateIdentity();
        transform.SetUniformScale(2.0f);
        AZ::TransformBus::Event(
            entity.GetId(),
            &AZ::TransformInterface::SetWorldTM,
            transform);

        const HairHandle scaledHairHandle = hair->GetHairHandle();
        EXPECT_TRUE(scaledHairHandle);
        EXPECT_NE(scaledHairHandle, initialHairHandle);
        EXPECT_FALSE(system.IsValid(worldHandle, initialHairHandle));
        EXPECT_TRUE(system.IsValid(worldHandle, scaledHairHandle));
        EXPECT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        const AZStd::vector<HairVertexState> scaledStates = hair->CopyVertexStates();
        ASSERT_EQ(scaledStates.size(), 3);
        EXPECT_NEAR(scaledStates.front().m_localPosition.GetZ(), 2.0f, 1.0e-3f);
        hairState = hair->GetState();
        EXPECT_NEAR(hairState.m_scalpToHeadTransform.GetTranslation().GetZ(), 0.5f, 1.0e-6f);

        entity.Deactivate();
        EXPECT_FALSE(system.IsValid(worldHandle, scaledHairHandle));

        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::UnregisterComponentDescriptor,
            hairDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::UnregisterComponentDescriptor,
            transformDescriptor);
        hairDescriptor->ReleaseDescriptor();
        transformDescriptor->ReleaseDescriptor();
    }

    TEST(ComponentTests, SkeletonBusOwnsAnimationPoseAndMappingResources)
    {
        ComponentNameDictionaryScope nameDictionary;
        SystemComponent component;
        component.Activate();

        SkeletonDefinitionConfiguration skeletonConfiguration;
        skeletonConfiguration.m_joints = {
            {.m_name = AZ::Name("root"), .m_parentIndex = -1},
            {.m_name = AZ::Name("child"), .m_parentIndex = 0},
        };

        SkeletonDefinitionHandle sourceSkeletonHandle;
        SkeletonRequestBus::BroadcastResult(
            sourceSkeletonHandle,
            &ISkeletonRequests::CreateSkeletonDefinition,
            skeletonConfiguration);
        ASSERT_TRUE(sourceSkeletonHandle);

        SkeletonDefinitionHandle targetSkeletonHandle;
        SkeletonRequestBus::BroadcastResult(
            targetSkeletonHandle,
            &ISkeletonRequests::CreateSkeletonDefinition,
            skeletonConfiguration);
        ASSERT_TRUE(targetSkeletonHandle);

        AZStd::vector<SkeletonJoint> joints;
        SkeletonRequestBus::BroadcastResult(
            joints,
            &ISkeletonRequests::CopySkeletonJoints,
            sourceSkeletonHandle);
        ASSERT_EQ(joints.size(), 2);
        EXPECT_EQ(joints[1].m_parentIndex, 0);

        SkeletalAnimationConfiguration animationConfiguration;
        SkeletalAnimatedJoint animatedJoint;
        animatedJoint.m_name = AZ::Name("child");
        animatedJoint.m_keyframes = {
            {.m_time = 0.0f},
            {.m_translation = AZ::Vector3::CreateAxisX(2.0f), .m_time = 1.0f},
        };
        animationConfiguration.m_joints.push_back(animatedJoint);

        SkeletalAnimationHandle animationHandle;
        SkeletonRequestBus::BroadcastResult(
            animationHandle,
            &ISkeletonRequests::CreateSkeletalAnimation,
            animationConfiguration);
        ASSERT_TRUE(animationHandle);

        SkeletonPoseHandle poseHandle;
        SkeletonRequestBus::BroadcastResult(
            poseHandle,
            &ISkeletonRequests::CreateSkeletonPose,
            sourceSkeletonHandle);
        ASSERT_TRUE(poseHandle);

        bool sampled = false;
        SkeletonRequestBus::BroadcastResult(
            sampled,
            &ISkeletonRequests::SampleSkeletalAnimation,
            animationHandle,
            poseHandle,
            0.5f);
        ASSERT_TRUE(sampled);

        AZStd::vector<AZ::Transform> sampledTransforms;
        SkeletonRequestBus::BroadcastResult(
            sampledTransforms,
            &ISkeletonRequests::CopySkeletonPoseModelTransforms,
            poseHandle);
        ASSERT_EQ(sampledTransforms.size(), 2);
        EXPECT_TRUE(sampledTransforms[1].GetTranslation().IsClose(AZ::Vector3::CreateAxisX()));

        SkeletonMapperConfiguration mapperConfiguration;
        mapperConfiguration.m_sourceSkeletonHandle = sourceSkeletonHandle;
        mapperConfiguration.m_targetSkeletonHandle = targetSkeletonHandle;
        mapperConfiguration.m_sourceNeutralModelTransforms = {
            AZ::Transform::CreateIdentity(),
            AZ::Transform::CreateIdentity(),
        };
        mapperConfiguration.m_targetNeutralModelTransforms = mapperConfiguration.m_sourceNeutralModelTransforms;
        SkeletonMapperHandle mapperHandle;
        SkeletonRequestBus::BroadcastResult(
            mapperHandle,
            &ISkeletonRequests::CreateSkeletonMapper,
            mapperConfiguration);
        ASSERT_TRUE(mapperHandle);

        AZStd::vector<AZ::Transform> mappedTransforms;
        SkeletonRequestBus::BroadcastResult(
            mappedTransforms,
            &ISkeletonRequests::MapSkeletonPose,
            mapperHandle,
            sampledTransforms,
            mapperConfiguration.m_targetNeutralModelTransforms);
        ASSERT_EQ(mappedTransforms.size(), 2);
        EXPECT_TRUE(mappedTransforms[1].GetTranslation().IsClose(AZ::Vector3::CreateAxisX()));

        bool destroyed = false;
        SkeletonRequestBus::BroadcastResult(
            destroyed,
            &ISkeletonRequests::DestroySkeletonMapper,
            mapperHandle);
        EXPECT_TRUE(destroyed);
        SkeletonRequestBus::BroadcastResult(
            destroyed,
            &ISkeletonRequests::DestroySkeletonPose,
            poseHandle);
        EXPECT_TRUE(destroyed);
        SkeletonRequestBus::BroadcastResult(
            destroyed,
            &ISkeletonRequests::DestroySkeletalAnimation,
            animationHandle);
        EXPECT_TRUE(destroyed);
        SkeletonRequestBus::BroadcastResult(
            destroyed,
            &ISkeletonRequests::DestroySkeletonDefinition,
            sourceSkeletonHandle);
        EXPECT_TRUE(destroyed);
        SkeletonRequestBus::BroadcastResult(
            destroyed,
            &ISkeletonRequests::DestroySkeletonDefinition,
            targetSkeletonHandle);
        EXPECT_TRUE(destroyed);

        component.Deactivate();
    }

    TEST(ComponentTests, SkeletonComponentLoadsReloadsAndOwnsAssetResources)
    {
        ComponentNameDictionaryScope nameDictionary;
        ComponentAssetManagerScope assetManager;
        SkeletonAssetHandler assetHandler;
        EXPECT_EQ(assetHandler.GetComponentTypeId(), EditorSkeletonComponentTypeId);
        EXPECT_TRUE(assetHandler.CanCreateComponent({}));
        AZStd::vector<AZStd::string> extensions;
        assetHandler.GetAssetTypeExtensions(extensions);
        EXPECT_EQ(extensions, AZStd::vector<AZStd::string>{"jolt"});

        System system(CreateComponentSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        SkeletonDefinitionConfiguration skeletonConfiguration;
        skeletonConfiguration.m_joints = {
            {.m_name = AZ::Name("root"), .m_parentIndex = -1},
            {.m_name = AZ::Name("child"), .m_parentIndex = 0},
        };
        const SkeletonDefinitionHandle sourceSkeletonHandle =
            system.CreateSkeletonDefinition(skeletonConfiguration);
        ASSERT_TRUE(sourceSkeletonHandle);

        SkeletalAnimationConfiguration animationConfiguration;
        animationConfiguration.m_joints = {
            {
                .m_name = AZ::Name("child"),
                .m_keyframes = {
                    {},
                    {
                        .m_translation = AZ::Vector3::CreateAxisX(2.0f),
                        .m_time = 1.0f,
                    },
                },
            },
        };
        const SkeletalAnimationHandle sourceAnimationHandle =
            system.CreateSkeletalAnimation(animationConfiguration);
        ASSERT_TRUE(sourceAnimationHandle);

        SkeletonDefinitionArchive skeletonArchive;
        SkeletalAnimationArchive animationArchive;
        ASSERT_TRUE(system.ExportSkeletonDefinition(sourceSkeletonHandle, skeletonArchive));
        ASSERT_TRUE(system.ExportSkeletalAnimation(sourceAnimationHandle, animationArchive));
        ASSERT_TRUE(system.DestroySkeletalAnimation(sourceAnimationHandle));
        ASSERT_TRUE(system.DestroySkeletonDefinition(sourceSkeletonHandle));

        const AZ::Data::AssetId assetId(AZ::Uuid::CreateRandom(), 1);
        const auto createAsset =
            [&assetId, &skeletonArchive](
                const AZ::Name animationName,
                const SkeletalAnimationArchive& sourceArchive)
        {
            auto* assetData = aznew SkeletonAsset(
                assetId,
                AZ::Data::AssetData::AssetStatus::Ready);
            assetData->m_data.m_name = AZ::Name("test_skeleton");
            assetData->m_data.m_skeleton = skeletonArchive;
            assetData->m_data.m_animations.push_back({
                .m_archive = sourceArchive,
                .m_name = animationName,
            });
            return AZ::Data::Asset<SkeletonAsset>(
                assetData,
                AZ::Data::AssetLoadBehavior::NoLoad);
        };

        SkeletonComponentConfiguration componentConfiguration;
        componentConfiguration.m_asset = createAsset(AZ::Name("walk"), animationArchive);
        AZ::ComponentDescriptor* componentDescriptor = SkeletonComponent::CreateDescriptor();
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::RegisterComponentDescriptor,
            componentDescriptor);

        AZ::Entity entity("Skeleton");
        SkeletonComponent* component =
            entity.CreateComponent<SkeletonComponent>(AZStd::move(componentConfiguration));
        ASSERT_TRUE(component);
        entity.Init();

        ComponentSkeletonNotifications notifications;
        notifications.BusConnect(entity.GetId());
        entity.Activate();

        const SkeletonDefinitionHandle initialSkeletonHandle = component->GetSkeletonHandle();
        const SkeletalAnimationHandle initialAnimationHandle = component->FindAnimation(AZ::Name("walk"));
        ASSERT_TRUE(component->IsReady());
        ASSERT_TRUE(initialSkeletonHandle);
        ASSERT_TRUE(initialAnimationHandle);
        EXPECT_TRUE(system.IsValid(initialSkeletonHandle));
        EXPECT_TRUE(system.IsValid(initialAnimationHandle));
        AZStd::array<AZ::Name, 1> animationNames;
        const BufferResult animationNameResult = component->GetAnimationNames(animationNames);
        EXPECT_TRUE(animationNameResult.IsComplete());
        EXPECT_EQ(animationNames.front(), AZ::Name("walk"));
        const BufferResult boundedAnimationNameResult = component->GetAnimationNames({});
        EXPECT_TRUE(boundedAnimationNameResult.HasOverflow());
        EXPECT_EQ(boundedAnimationNameResult.m_requiredCount, 1);
        EXPECT_EQ(component->CopyAnimationNames(), AZStd::vector<AZ::Name>{AZ::Name("walk")});
        EXPECT_EQ(notifications.m_readyCount, 1);
        EXPECT_EQ(notifications.m_lastReadyHandle, initialSkeletonHandle);

        SkeletalAnimationArchive invalidArchive = animationArchive;
        ++invalidArchive.m_buildFingerprint;
        AZ::Data::Asset<SkeletonAsset> invalidAsset = createAsset(AZ::Name("invalid"), invalidArchive);
        AZ_TEST_START_TRACE_SUPPRESSION;
        AZ::Data::AssetBus::Event(
            assetId,
            &AZ::Data::AssetBus::Events::OnAssetReloaded,
            AZ::Data::Asset<AZ::Data::AssetData>(invalidAsset));
        AZ_TEST_STOP_TRACE_SUPPRESSION(1);
        EXPECT_EQ(component->GetSkeletonHandle(), initialSkeletonHandle);
        EXPECT_EQ(component->FindAnimation(AZ::Name("walk")), initialAnimationHandle);
        EXPECT_TRUE(system.IsValid(initialSkeletonHandle));
        EXPECT_TRUE(system.IsValid(initialAnimationHandle));
        EXPECT_EQ(notifications.m_readyCount, 1);
        EXPECT_EQ(notifications.m_reloadingCount, 0);

        AZ::Data::Asset<SkeletonAsset> reloadedAsset = createAsset(AZ::Name("run"), animationArchive);
        AZ::Data::AssetBus::Event(
            assetId,
            &AZ::Data::AssetBus::Events::OnAssetReloaded,
            AZ::Data::Asset<AZ::Data::AssetData>(reloadedAsset));
        const SkeletonDefinitionHandle reloadedSkeletonHandle = component->GetSkeletonHandle();
        const SkeletalAnimationHandle reloadedAnimationHandle = component->FindAnimation(AZ::Name("run"));
        ASSERT_TRUE(reloadedSkeletonHandle);
        ASSERT_TRUE(reloadedAnimationHandle);
        EXPECT_NE(reloadedSkeletonHandle, initialSkeletonHandle);
        EXPECT_FALSE(component->FindAnimation(AZ::Name("walk")));
        EXPECT_FALSE(system.IsValid(initialSkeletonHandle));
        EXPECT_FALSE(system.IsValid(initialAnimationHandle));
        EXPECT_TRUE(system.IsValid(reloadedSkeletonHandle));
        EXPECT_TRUE(system.IsValid(reloadedAnimationHandle));
        EXPECT_EQ(notifications.m_readyCount, 2);
        EXPECT_EQ(notifications.m_reloadingCount, 1);
        EXPECT_EQ(notifications.m_lastReloadingHandle, initialSkeletonHandle);
        EXPECT_EQ(notifications.m_lastReadyHandle, reloadedSkeletonHandle);

        entity.Deactivate();
        EXPECT_FALSE(system.IsValid(reloadedSkeletonHandle));
        EXPECT_FALSE(system.IsValid(reloadedAnimationHandle));
        EXPECT_EQ(notifications.m_releasedCount, 1);
        notifications.BusDisconnect();

        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::UnregisterComponentDescriptor,
            componentDescriptor);
        componentDescriptor->ReleaseDescriptor();
    }

    TEST(ComponentTests, SceneComponentLoadsReloadsAndOwnsAssetResources)
    {
        ComponentNameDictionaryScope nameDictionary;
        ComponentAssetManagerScope assetManager;
        SceneAssetHandler assetHandler;
        EXPECT_EQ(assetHandler.GetComponentTypeId(), EditorSceneComponentTypeId);
        EXPECT_TRUE(assetHandler.CanCreateComponent({}));
        AZStd::vector<AZStd::string> extensions;
        assetHandler.GetAssetTypeExtensions(extensions);
        EXPECT_EQ(extensions, AZStd::vector<AZStd::string>{"jolt"});

        System system(CreateComponentSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        SceneSourceData sourceData;
        sourceData.m_name = AZ::Name("ComponentScene");
        sourceData.m_shapes.push_back(SceneSourceShapeData{
            .m_geometry = BoxShapeConfiguration{},
        });
        sourceData.m_bodies.emplace_back(SceneAssetRigidBody{
            .m_objectLayer = DefaultLayers::NonMoving,
            .m_motionType = MotionType::Static,
            .m_shapeIndex = 0,
        });
        SceneAssetData sceneData;
        ASSERT_TRUE(system.BuildSceneAsset(sourceData, sceneData));

        const AZ::Data::AssetId assetId(AZ::Uuid::CreateRandom(), 1);
        const auto createAsset =
            [&assetId](const SceneAssetData& data)
        {
            auto* assetData = aznew SceneAsset(
                assetId,
                AZ::Data::AssetData::AssetStatus::Ready);
            assetData->m_data = data;
            return AZ::Data::Asset<SceneAsset>(
                assetData,
                AZ::Data::AssetLoadBehavior::NoLoad);
        };

        SceneComponentConfiguration componentConfiguration;
        componentConfiguration.m_asset = createAsset(sceneData);
        AZ::ComponentDescriptor* componentDescriptor = SceneComponent::CreateDescriptor();
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::RegisterComponentDescriptor,
            componentDescriptor);

        AZ::Entity entity("Scene");
        SceneComponent* component =
            entity.CreateComponent<SceneComponent>(AZStd::move(componentConfiguration));
        ASSERT_TRUE(component);
        entity.Init();

        ComponentSceneNotifications notifications;
        notifications.BusConnect(entity.GetId());
        entity.Activate();

        const SceneDefinitionHandle initialDefinitionHandle = component->GetDefinitionHandle();
        const SceneInstanceHandle initialInstanceHandle = component->GetInstanceHandle();
        ASSERT_TRUE(component->IsReady());
        ASSERT_TRUE(initialDefinitionHandle);
        ASSERT_TRUE(initialInstanceHandle);
        EXPECT_TRUE(system.IsValid(initialDefinitionHandle));
        EXPECT_TRUE(system.IsValid(system.GetDefaultWorldHandle(), initialInstanceHandle));
        ASSERT_EQ(component->CopyBodies().size(), 1);
        EXPECT_TRUE(component->CopyConstraints().empty());
        EXPECT_EQ(notifications.m_readyCount, 1);
        EXPECT_EQ(notifications.m_lastReadyHandle, initialInstanceHandle);

        SceneAssetData invalidSceneData = sceneData;
        ++invalidSceneData.m_shapes[0].m_archive.m_buildFingerprint;
        AZ::Data::Asset<SceneAsset> invalidAsset = createAsset(invalidSceneData);
        AZ_TEST_START_TRACE_SUPPRESSION;
        AZ::Data::AssetBus::Event(
            assetId,
            &AZ::Data::AssetBus::Events::OnAssetReloaded,
            AZ::Data::Asset<AZ::Data::AssetData>(invalidAsset));
        AZ_TEST_STOP_TRACE_SUPPRESSION(1);
        EXPECT_EQ(component->GetDefinitionHandle(), initialDefinitionHandle);
        EXPECT_EQ(component->GetInstanceHandle(), initialInstanceHandle);
        EXPECT_TRUE(system.IsValid(initialDefinitionHandle));
        EXPECT_TRUE(system.IsValid(system.GetDefaultWorldHandle(), initialInstanceHandle));
        EXPECT_EQ(notifications.m_readyCount, 1);
        EXPECT_EQ(notifications.m_reloadingCount, 0);

        SceneAssetData reloadedSceneData = sceneData;
        reloadedSceneData.m_name = AZ::Name("ReloadedComponentScene");
        AZ::Data::Asset<SceneAsset> reloadedAsset = createAsset(reloadedSceneData);
        AZ::Data::AssetBus::Event(
            assetId,
            &AZ::Data::AssetBus::Events::OnAssetReloaded,
            AZ::Data::Asset<AZ::Data::AssetData>(reloadedAsset));
        const SceneDefinitionHandle reloadedDefinitionHandle = component->GetDefinitionHandle();
        const SceneInstanceHandle reloadedInstanceHandle = component->GetInstanceHandle();
        ASSERT_TRUE(reloadedDefinitionHandle);
        ASSERT_TRUE(reloadedInstanceHandle);
        EXPECT_NE(reloadedDefinitionHandle, initialDefinitionHandle);
        EXPECT_NE(reloadedInstanceHandle, initialInstanceHandle);
        EXPECT_FALSE(system.IsValid(initialDefinitionHandle));
        EXPECT_FALSE(system.IsValid(system.GetDefaultWorldHandle(), initialInstanceHandle));
        EXPECT_TRUE(system.IsValid(reloadedDefinitionHandle));
        EXPECT_TRUE(system.IsValid(system.GetDefaultWorldHandle(), reloadedInstanceHandle));
        EXPECT_EQ(notifications.m_readyCount, 2);
        EXPECT_EQ(notifications.m_reloadingCount, 1);
        EXPECT_EQ(notifications.m_lastReloadingHandle, initialInstanceHandle);
        EXPECT_EQ(notifications.m_lastReadyHandle, reloadedInstanceHandle);

        entity.Deactivate();
        EXPECT_FALSE(system.IsValid(reloadedDefinitionHandle));
        EXPECT_FALSE(system.IsValid(system.GetDefaultWorldHandle(), reloadedInstanceHandle));
        EXPECT_EQ(notifications.m_releasedCount, 1);
        notifications.BusDisconnect();

        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::UnregisterComponentDescriptor,
            componentDescriptor);
        componentDescriptor->ReleaseDescriptor();
    }

    TEST(ComponentTests, RagdollOwnsDefinitionsShapesAndRespondsToUniformScale)
    {
        ComponentNameDictionaryScope nameDictionary;
        AZ::ComponentDescriptor* transformDescriptor = AzFramework::TransformComponent::CreateDescriptor();
        AZ::ComponentDescriptor* ragdollDescriptor = RagdollComponent::CreateDescriptor();
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::RegisterComponentDescriptor,
            transformDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::RegisterComponentDescriptor,
            ragdollDescriptor);

        System system(CreateComponentSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        AZ::Entity entity("Ragdoll");
        entity.CreateComponent<AzFramework::TransformComponent>();
        RagdollComponent* ragdoll =
            entity.CreateComponent<RagdollComponent>(RagdollComponentConfiguration::CreateDefault());
        ASSERT_TRUE(ragdoll);
        entity.Init();
        entity.Activate();

        const WorldHandle worldHandle = ragdoll->GetWorldHandle();
        const RagdollHandle initialRagdollHandle = ragdoll->GetRagdollHandle();
        ASSERT_TRUE(initialRagdollHandle);
        EXPECT_TRUE(system.IsValid(worldHandle, initialRagdollHandle));
        WorldStatistics statistics;
        ASSERT_TRUE(system.GetWorldStatistics(worldHandle, statistics));
        EXPECT_EQ(statistics.m_ragdollCount, 1);

        const RagdollState state = ragdoll->GetState();
        EXPECT_EQ(state.m_bodyCount, 2);
        EXPECT_EQ(state.m_constraintCount, 1);
        EXPECT_EQ(state.m_entityId, entity.GetId());

        const AZStd::vector<BodyHandle> bodies = ragdoll->CopyBodies();
        ASSERT_EQ(bodies.size(), 2);
        EXPECT_TRUE(system.IsValid(worldHandle, bodies.front()));

        const AZStd::vector<ConstraintHandle> constraints = ragdoll->CopyConstraints();
        ASSERT_EQ(constraints.size(), 1);
        EXPECT_TRUE(system.IsValid(worldHandle, constraints.front()));

        EXPECT_TRUE(ragdoll->IsSimulationEnabled());
        EXPECT_TRUE(ragdoll->DisableSimulation());
        EXPECT_FALSE(ragdoll->IsSimulationEnabled());
        EXPECT_EQ(ragdoll->GetRagdollHandle(), initialRagdollHandle);
        EXPECT_TRUE(system.IsValid(worldHandle, initialRagdollHandle));
        EXPECT_TRUE(system.IsValid(worldHandle, bodies.front()));
        EXPECT_FALSE(system.IsBodyInSimulation(worldHandle, bodies.front()));
        EXPECT_EQ(ragdoll->GetState().m_bodyCount, 2);
        EXPECT_FALSE(ragdoll->GetState().m_isInSimulation);

        EXPECT_TRUE(ragdoll->EnableSimulation());
        EXPECT_TRUE(ragdoll->IsSimulationEnabled());
        EXPECT_EQ(ragdoll->GetRagdollHandle(), initialRagdollHandle);
        EXPECT_TRUE(system.IsBodyInSimulation(worldHandle, bodies.front()));

        const AZStd::vector<AZ::Transform> pose = ragdoll->CopyPose();
        ASSERT_EQ(pose.size(), 2);
        EXPECT_TRUE(ragdoll->DriveMotors(pose));
        EXPECT_TRUE(ragdoll->DriveMotorsWithVelocity(pose, pose, 1.0f / 60.0f));
        EXPECT_FALSE(ragdoll->DriveMotorsWithVelocity({}, pose, 1.0f / 60.0f));
        EXPECT_FALSE(ragdoll->DriveMotorsWithVelocity(pose, pose, 0.0f));
        EXPECT_TRUE(ragdoll->SetLinearVelocity(AZ::Vector3::CreateAxisX()));
        const float nan = AZStd::numeric_limits<float>::quiet_NaN();
        EXPECT_FALSE(ragdoll->SetLinearVelocity(AZ::Vector3(nan)));
        EXPECT_TRUE(ragdoll->SetCollisionGroupId(42));
        EXPECT_EQ(ragdoll->GetState().m_collisionGroupId, 42);

        AZ::Transform transform = AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisX(3.0f));
        transform.SetUniformScale(2.0f);
        AZ::TransformBus::Event(
            entity.GetId(),
            &AZ::TransformInterface::SetWorldTM,
            transform);

        const RagdollHandle scaledRagdollHandle = ragdoll->GetRagdollHandle();
        EXPECT_TRUE(scaledRagdollHandle);
        EXPECT_NE(scaledRagdollHandle, initialRagdollHandle);
        EXPECT_FALSE(system.IsValid(worldHandle, initialRagdollHandle));
        EXPECT_TRUE(system.IsValid(worldHandle, scaledRagdollHandle));

        EXPECT_TRUE(ragdoll->DisableSimulation());
        EXPECT_FALSE(ragdoll->IsSimulationEnabled());
        transform.SetUniformScale(3.0f);
        AZ::TransformBus::Event(
            entity.GetId(),
            &AZ::TransformInterface::SetWorldTM,
            transform);
        EXPECT_FALSE(ragdoll->GetRagdollHandle());
        EXPECT_FALSE(ragdoll->IsSimulationEnabled());
        EXPECT_FALSE(system.IsValid(worldHandle, scaledRagdollHandle));

        EXPECT_TRUE(ragdoll->EnableSimulation());
        const RagdollHandle rebuiltRagdollHandle = ragdoll->GetRagdollHandle();
        EXPECT_TRUE(rebuiltRagdollHandle);
        EXPECT_TRUE(ragdoll->IsSimulationEnabled());

        entity.Deactivate();
        EXPECT_FALSE(system.IsValid(worldHandle, rebuiltRagdollHandle));

        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::UnregisterComponentDescriptor,
            ragdollDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::UnregisterComponentDescriptor,
            transformDescriptor);
        ragdollDescriptor->ReleaseDescriptor();
        transformDescriptor->ReleaseDescriptor();
    }

    TEST(ComponentTests, SoftBodyOwnsDefinitionMaterialsAndRespondsToUniformScale)
    {
        ComponentNameDictionaryScope nameDictionary;
        AZ::ComponentDescriptor* transformDescriptor = AzFramework::TransformComponent::CreateDescriptor();
        AZ::ComponentDescriptor* softBodyDescriptor = SoftBodyComponent::CreateDescriptor();
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::RegisterComponentDescriptor,
            transformDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::RegisterComponentDescriptor,
            softBodyDescriptor);

        System system(CreateComponentSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        SoftBodyComponentConfiguration configuration;
        configuration.m_definition.m_vertices = {
            {.m_position = AZ::Vector3(-1.0f, 0.0f, 0.0f)},
            {.m_position = AZ::Vector3(1.0f, 0.0f, 0.0f)},
            {.m_position = AZ::Vector3(0.0f, 1.0f, 0.0f)},
        };
        configuration.m_definition.m_faces = {
            {.m_firstVertex = 0, .m_secondVertex = 1, .m_thirdVertex = 2},
        };
        configuration.m_definition.m_edgeConstraints = {
            {.m_firstVertex = 0, .m_secondVertex = 1},
            {.m_firstVertex = 1, .m_secondVertex = 2},
            {.m_firstVertex = 2, .m_secondVertex = 0},
        };
        configuration.m_definition.m_createFaceConstraints = false;
        configuration.m_materials.emplace_back();
        configuration.m_body.m_manualUpdate = true;

        AZ::Entity entity("Soft body");
        entity.CreateComponent<AzFramework::TransformComponent>();
        SoftBodyComponent* softBody = entity.CreateComponent<SoftBodyComponent>(configuration);
        ASSERT_TRUE(softBody);
        entity.Init();
        AZ::Transform initialTransform = AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisZ(5.0f));
        AZ::TransformBus::Event(
            entity.GetId(),
            &AZ::TransformInterface::SetWorldTM,
            initialTransform);
        entity.Activate();

        const WorldHandle worldHandle = softBody->GetWorldHandle();
        const BodyHandle initialBodyHandle = softBody->GetBodyHandle();
        ASSERT_TRUE(initialBodyHandle);
        ASSERT_TRUE(system.IsValid(worldHandle, initialBodyHandle));
        const SoftBodyDefinitionHandle initialDefinitionHandle = softBody->GetDefinitionHandle();
        ASSERT_TRUE(initialDefinitionHandle);
        EXPECT_TRUE(system.IsValid(initialDefinitionHandle));
        const SoftBodyDefinitionState initialDefinitionState = softBody->GetDefinitionState();
        EXPECT_EQ(initialDefinitionState.m_vertexCount, 3);
        EXPECT_EQ(initialDefinitionState.m_faceCount, 1);
        EXPECT_EQ(initialDefinitionState.m_materialCount, 1);
        EXPECT_EQ(initialDefinitionState.m_edgeConstraintCount, 3);
        EXPECT_EQ(softBody->CopyVertices().size(), 3);
        EXPECT_EQ(softBody->CopyFaces().size(), 1);
        EXPECT_EQ(softBody->CopyMaterials().size(), 1);
        EXPECT_TRUE(softBody->CopyDefinitionDihedralBendConstraints().empty());
        EXPECT_EQ(softBody->CopyDefinitionEdgeConstraints().size(), 3);
        EXPECT_EQ(softBody->CopyDefinitionFaces().size(), 1);
        EXPECT_TRUE(softBody->CopyDefinitionInverseBinds().empty());
        EXPECT_TRUE(softBody->CopyDefinitionLongRangeConstraints().empty());
        EXPECT_EQ(softBody->CopyDefinitionMaterials().size(), 1);
        EXPECT_TRUE(softBody->CopyDefinitionRodBendTwistConstraints().empty());
        EXPECT_TRUE(softBody->CopyDefinitionRodStretchShearConstraints().empty());
        EXPECT_TRUE(softBody->CopyDefinitionSkinConstraints().empty());
        EXPECT_EQ(softBody->CopyDefinitionVertices().size(), 3);
        EXPECT_TRUE(softBody->CopyDefinitionVolumeConstraints().empty());

        AZ::Aabb localBounds = AZ::Aabb::CreateNull();
        EXPECT_TRUE(softBody->GetLocalBounds(localBounds));
        EXPECT_TRUE(localBounds.IsValid());

        const BodyState initialState = softBody->GetState();
        EXPECT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        const BodyState worldSteppedState = softBody->GetState();
        EXPECT_DOUBLE_EQ(worldSteppedState.m_transform.m_position.m_z, initialState.m_transform.m_position.m_z);
        const AZStd::array inverseMasses = {0.5f, 0.75f, 1.0f};
        EXPECT_TRUE(softBody->SetVertexInverseMasses(0, inverseMasses));
        const AZStd::vector velocities = {
            -AZ::Vector3::CreateAxisZ(),
            -AZ::Vector3::CreateAxisZ(),
            -AZ::Vector3::CreateAxisZ(),
        };
        EXPECT_TRUE(softBody->SetVertexVelocitiesFromVectors(0, velocities));
        for (AZ::u32 step = 0; step < 10; ++step)
        {
            EXPECT_TRUE(softBody->UpdateManually(1.0f / 60.0f));
        }
        const BodyState manuallyUpdatedState = softBody->GetState();
        EXPECT_LT(manuallyUpdatedState.m_transform.m_position.m_z, initialState.m_transform.m_position.m_z);
        AZ::Transform manuallyUpdatedTransform = AZ::Transform::CreateIdentity();
        AZ::TransformBus::EventResult(
            manuallyUpdatedTransform,
            entity.GetId(),
            &AZ::TransformInterface::GetWorldTM);
        EXPECT_LT(manuallyUpdatedTransform.GetTranslation().GetZ(), initialTransform.GetTranslation().GetZ());

        EXPECT_TRUE(softBody->SetVertexVelocity(1, AZ::Vector3::CreateAxisZ()));

        SoftBodyRuntimeConfiguration runtimeConfiguration;
        ASSERT_TRUE(softBody->GetRuntimeConfiguration(runtimeConfiguration));
        runtimeConfiguration.m_iterationCount = 7;
        EXPECT_TRUE(softBody->UpdateRuntimeConfiguration(runtimeConfiguration));

        AZ::Transform transform = AZ::Transform::CreateIdentity();
        transform.SetUniformScale(2.0f);
        AZ::TransformBus::Event(
            entity.GetId(),
            &AZ::TransformInterface::SetWorldTM,
            transform);
        const BodyHandle scaledBodyHandle = softBody->GetBodyHandle();
        EXPECT_TRUE(scaledBodyHandle);
        EXPECT_NE(scaledBodyHandle, initialBodyHandle);
        EXPECT_FALSE(system.IsValid(worldHandle, initialBodyHandle));
        const SoftBodyDefinitionHandle scaledDefinitionHandle = softBody->GetDefinitionHandle();
        EXPECT_TRUE(scaledDefinitionHandle);
        EXPECT_NE(scaledDefinitionHandle, initialDefinitionHandle);
        EXPECT_FALSE(system.IsValid(initialDefinitionHandle));
        EXPECT_TRUE(system.IsValid(scaledDefinitionHandle));
        const AZStd::vector<SoftBodyVertex> scaledVertices = softBody->CopyVertices();
        ASSERT_EQ(scaledVertices.size(), 3);
        EXPECT_TRUE(scaledVertices[0].m_position.IsClose(AZ::Vector3(-2.0f, 0.0f, 0.0f)));

        entity.Deactivate();
        EXPECT_FALSE(system.IsValid(worldHandle, scaledBodyHandle));
        EXPECT_FALSE(system.IsValid(scaledDefinitionHandle));

        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::UnregisterComponentDescriptor,
            softBodyDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::UnregisterComponentDescriptor,
            transformDescriptor);
        softBodyDescriptor->ReleaseDescriptor();
        transformDescriptor->ReleaseDescriptor();
    }

    TEST(ComponentTests, PathOwnsTransientNativeHandle)
    {
        ComponentNameDictionaryScope nameDictionary;
        AZ::ComponentDescriptor* pathDescriptor = PathComponent::CreateDescriptor();
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::RegisterComponentDescriptor,
            pathDescriptor);
        System system(CreateComponentSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        HermitePathConfiguration configuration;
        configuration.m_points = {
            HermitePathPoint{
                .m_position = AZ::Vector3::CreateZero(),
            },
            HermitePathPoint{
                .m_position = AZ::Vector3::CreateAxisX(2.0f),
            },
        };
        AZ::Entity entity("Constraint path");
        PathComponent* path = entity.CreateComponent<PathComponent>(configuration);
        ASSERT_TRUE(path);
        entity.Init();
        entity.Activate();
        const PathHandle pathHandle = path->GetPathHandle();
        EXPECT_TRUE(pathHandle);
        EXPECT_TRUE(system.IsValid(pathHandle));

        const AZStd::vector<HermitePathPoint> points = path->CopyPoints();
        ASSERT_EQ(points.size(), 2);
        EXPECT_TRUE(points[0].m_position.IsClose(AZ::Vector3::CreateZero()));
        EXPECT_TRUE(points[1].m_position.IsClose(AZ::Vector3::CreateAxisX(2.0f)));

        const PathState state = path->GetState();
        EXPECT_FLOAT_EQ(state.m_maximumFraction, 1.0f);
        EXPECT_FALSE(state.m_isLooping);

        const PathSample sample = path->Sample(0.5f);
        EXPECT_TRUE(sample);
        EXPECT_TRUE(sample.m_position.IsClose(AZ::Vector3::CreateAxisX(1.0f)));
        EXPECT_FLOAT_EQ(sample.m_fraction, 0.5f);

        const PathSample closest = path->FindClosestPoint(
            AZ::Vector3(0.5f, 1.0f, 0.0f),
            0.25f);
        EXPECT_TRUE(closest);
        EXPECT_TRUE(closest.m_position.IsClose(AZ::Vector3::CreateAxisX(0.5f)));
        EXPECT_GT(closest.m_fraction, 0.0f);
        EXPECT_LT(closest.m_fraction, state.m_maximumFraction);

        entity.Deactivate();
        EXPECT_FALSE(system.IsValid(pathHandle));

        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::UnregisterComponentDescriptor,
            pathDescriptor);
        pathDescriptor->ReleaseDescriptor();
    }

    TEST(ComponentTests, WheeledVehicleTracksChassisLifecycleWithoutPolling)
    {
        ComponentNameDictionaryScope nameDictionary;
        AZ::ComponentDescriptor* transformDescriptor = AzFramework::TransformComponent::CreateDescriptor();
        AZ::ComponentDescriptor* colliderDescriptor = ColliderComponent::CreateDescriptor();
        AZ::ComponentDescriptor* bodyDescriptor = RigidBodyComponent::CreateDescriptor();
        AZ::ComponentDescriptor* vehicleDescriptor = WheeledVehicleComponent::CreateDescriptor();
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::RegisterComponentDescriptor,
            transformDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::RegisterComponentDescriptor,
            colliderDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::RegisterComponentDescriptor,
            bodyDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::RegisterComponentDescriptor,
            vehicleDescriptor);

        System system(CreateComponentSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        ColliderShapeConfiguration colliderShape;
        colliderShape.m_shape.m_density = 250.0f;
        colliderShape.m_shape.m_geometry = BoxShapeConfiguration{
            .m_dimensions = AZ::Vector3(1.8f, 4.0f, 0.6f),
        };
        WheeledVehicleComponentConfiguration vehicleConfiguration;
        vehicleConfiguration.m_vehicle.m_wheels = {
            {.m_position = AZ::Vector3(-0.8f, 1.4f, -0.25f)},
            {.m_position = AZ::Vector3(0.8f, 1.4f, -0.25f)},
            {.m_position = AZ::Vector3(-0.8f, -1.4f, -0.25f)},
            {.m_position = AZ::Vector3(0.8f, -1.4f, -0.25f)},
        };
        vehicleConfiguration.m_vehicle.m_differentials = {
            {.m_leftWheel = 0, .m_rightWheel = 1},
        };

        AZ::Entity entity("Wheeled vehicle");
        entity.CreateComponent<AzFramework::TransformComponent>();
        entity.CreateComponent<ColliderComponent>(AZStd::vector{colliderShape});
        RigidBodyComponent* body = entity.CreateComponent<RigidBodyComponent>();
        WheeledVehicleComponent* vehicle =
            entity.CreateComponent<WheeledVehicleComponent>(vehicleConfiguration);
        ASSERT_TRUE(body);
        ASSERT_TRUE(vehicle);
        entity.Init();
        entity.Activate();

        const WorldHandle worldHandle = body->GetWorldHandle();
        const VehicleHandle firstVehicleHandle = vehicle->GetVehicleHandle();
        ASSERT_TRUE(firstVehicleHandle);
        EXPECT_TRUE(system.IsValid(worldHandle, firstVehicleHandle));

        AZStd::array<WheelState, 4> wheels;
        WheeledVehicleState state;
        EXPECT_TRUE(vehicle->QueryState(state, wheels).IsComplete());
        EXPECT_EQ(state.m_bodyHandle, body->GetBodyHandle());
        EXPECT_TRUE(vehicle->SetInput({.m_forward = 1.0f}));

        ComponentVehicleCallbacks vehicleCallbacks;
        ComponentVehicleCollisionFilter vehicleCollisionFilter;
        EXPECT_TRUE(vehicle->SetCallbacks(&vehicleCallbacks));
        EXPECT_TRUE(vehicle->SetCollisionFilter(&vehicleCollisionFilter));
        EXPECT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        EXPECT_GT(vehicleCallbacks.m_preStepCount, 0);

        VehicleEngineConfiguration engineConfiguration = vehicle->GetEngineConfiguration();
        engineConfiguration.m_maximumTorque = 700.0f;
        ASSERT_TRUE(vehicle->UpdateEngineConfiguration(engineConfiguration));
        EXPECT_FLOAT_EQ(vehicle->GetEngineConfiguration().m_maximumTorque, 700.0f);
        EXPECT_GT(vehicle->CalculateEngineTorque(1.0f), 0.0f);
        EXPECT_TRUE(vehicle->ApplyEngineTorque(50.0f, 1.0f / 60.0f));
        EXPECT_TRUE(vehicle->ApplyEngineDamping(1.0f / 60.0f));

        EXPECT_FLOAT_EQ(vehicle->GetDifferentialLimitedSlipRatio(), 1.4f);
        ASSERT_TRUE(vehicle->SetDifferentialLimitedSlipRatio(1.75f));
        EXPECT_FLOAT_EQ(vehicle->GetDifferentialLimitedSlipRatio(), 1.75f);

        const WheelBasis wheelBasis = vehicle->GetWheelLocalBasis(0);
        EXPECT_TRUE(wheelBasis.m_forward.IsNormalized());
        EXPECT_TRUE(vehicle->GetWheelLocalTransform(
            0,
            AZ::Vector3::CreateAxisX(),
            AZ::Vector3::CreateAxisZ()).IsFinite());
        EXPECT_TRUE(vehicle->GetWheelWorldTransform(
            0,
            AZ::Vector3::CreateAxisX(),
            AZ::Vector3::CreateAxisZ()).m_rotation.IsFinite());

        AZStd::vector<VehicleDifferentialConfiguration> differentials = vehicle->CopyDifferentials();
        ASSERT_EQ(differentials.size(), 1);
        differentials[0].m_engineTorqueRatio = 0.9f;
        EXPECT_FALSE(vehicle->UpdateDifferentials(differentials));
        differentials[0].m_engineTorqueRatio = 1.0f;
        differentials[0].m_differentialRatio = 4.0f;
        ASSERT_TRUE(vehicle->UpdateDifferentials(differentials));
        EXPECT_FLOAT_EQ(vehicle->CopyDifferentials()[0].m_differentialRatio, 4.0f);

        VehicleTransmissionConfiguration transmissionConfiguration = vehicle->GetTransmissionConfiguration();
        transmissionConfiguration.m_mode = TransmissionMode::Manual;
        ASSERT_TRUE(vehicle->UpdateTransmissionConfiguration(transmissionConfiguration));
        ASSERT_TRUE(vehicle->SetPowertrainControl({
            .m_currentGear = 1,
            .m_clutchFriction = 0.5f,
            .m_engineRpm = 2'000.0f,
        }));
        const VehiclePowertrainState powertrainState = vehicle->GetPowertrainState();
        EXPECT_EQ(powertrainState.m_currentGear, 1);
        EXPECT_FLOAT_EQ(powertrainState.m_clutchFriction, 0.5f);
        EXPECT_FLOAT_EQ(powertrainState.m_engineRpm, 2'000.0f);

        ASSERT_TRUE(body->DisableSimulation());
        EXPECT_FALSE(system.IsValid(worldHandle, firstVehicleHandle));
        EXPECT_FALSE(vehicle->IsSimulationEnabled());

        ASSERT_TRUE(body->EnableSimulation());
        const VehicleHandle secondVehicleHandle = vehicle->GetVehicleHandle();
        EXPECT_TRUE(secondVehicleHandle);
        EXPECT_NE(secondVehicleHandle, firstVehicleHandle);
        EXPECT_TRUE(vehicle->IsSimulationEnabled());
        const AZ::u32 previousPreStepCount = vehicleCallbacks.m_preStepCount;
        EXPECT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        EXPECT_GT(vehicleCallbacks.m_preStepCount, previousPreStepCount);
        EXPECT_TRUE(vehicle->SetCallbacks(nullptr));
        EXPECT_TRUE(vehicle->SetCollisionFilter(nullptr));

        entity.Deactivate();
        EXPECT_FALSE(system.IsValid(worldHandle, secondVehicleHandle));

        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::UnregisterComponentDescriptor,
            vehicleDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::UnregisterComponentDescriptor,
            bodyDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::UnregisterComponentDescriptor,
            colliderDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::UnregisterComponentDescriptor,
            transformDescriptor);
        vehicleDescriptor->ReleaseDescriptor();
        bodyDescriptor->ReleaseDescriptor();
        colliderDescriptor->ReleaseDescriptor();
        transformDescriptor->ReleaseDescriptor();
    }

    TEST(ComponentTests, VirtualCharacterAutoUpdatesAndSynchronizesTransform)
    {
        ComponentNameDictionaryScope nameDictionary;
        AZ::ComponentDescriptor* transformDescriptor = AzFramework::TransformComponent::CreateDescriptor();
        AZ::ComponentDescriptor* colliderDescriptor = ColliderComponent::CreateDescriptor();
        AZ::ComponentDescriptor* characterDescriptor = VirtualCharacterControllerComponent::CreateDescriptor();
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::RegisterComponentDescriptor,
            transformDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::RegisterComponentDescriptor,
            colliderDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::RegisterComponentDescriptor,
            characterDescriptor);

        SystemConfiguration systemConfiguration = CreateComponentSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_autoSimulate = true;
        System system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        ColliderShapeConfiguration colliderShape;
        colliderShape.m_shape.m_geometry = SphereShapeConfiguration{.m_radius = 0.5f};
        VirtualCharacterComponentConfiguration configuration;
        configuration.m_createInnerBody = true;
        configuration.m_update.m_extended = false;
        configuration.m_update.m_gravity = AZ::Vector3::CreateZero();

        AZ::Entity entity("Virtual character");
        entity.CreateComponent<AzFramework::TransformComponent>();
        ColliderComponent* collider =
            entity.CreateComponent<ColliderComponent>(AZStd::vector{colliderShape});
        VirtualCharacterControllerComponent* character =
            entity.CreateComponent<VirtualCharacterControllerComponent>(configuration);
        ASSERT_TRUE(collider);
        ASSERT_TRUE(character);
        entity.Init();
        entity.Activate();

        const WorldHandle worldHandle = character->GetWorldHandle();
        const VirtualCharacterHandle characterHandle = character->GetCharacterHandle();
        ASSERT_TRUE(characterHandle);
        ASSERT_TRUE(character->SetVelocity(AZ::Vector3::CreateAxisX(2.0f)));
        ASSERT_TRUE(system.StepAutoSimulatedWorlds(1.0f / 60.0f));
        const AZStd::span<const VirtualCharacterMoveEvent> moves =
            system.GetEvents(worldHandle).GetVirtualCharacterMoves();
        ASSERT_EQ(moves.size(), 1);
        VirtualCharacterNotificationBus::Event(
            entity.GetId(),
            &IVirtualCharacterNotifications::OnCharacterMoved,
            moves.front());

        AZ::Transform transform = AZ::Transform::CreateIdentity();
        AZ::TransformBus::EventResult(
            transform,
            entity.GetId(),
            &AZ::TransformInterface::GetWorldTM);
        EXPECT_GT(transform.GetTranslation().GetX(), 0.0f);

        const VirtualCharacterState initialState = character->GetState();
        const ShapeHandle initialShapeHandle = initialState.m_shapeHandle;
        const BodyHandle innerBodyHandle = initialState.m_innerBodyHandle;
        ASSERT_TRUE(innerBodyHandle);
        EXPECT_EQ(initialState.m_innerBodyShapeHandle, initialShapeHandle);
        transform.SetUniformScale(2.0f);
        AZ::TransformBus::Event(
            entity.GetId(),
            &AZ::TransformInterface::SetWorldTM,
            transform);
        const VirtualCharacterState scaledState = character->GetState();
        EXPECT_NE(scaledState.m_shapeHandle, initialShapeHandle);
        EXPECT_EQ(scaledState.m_shapeHandle, collider->GetRootShapeHandle());
        EXPECT_EQ(scaledState.m_innerBodyHandle, innerBodyHandle);
        EXPECT_EQ(scaledState.m_innerBodyShapeHandle, collider->GetRootShapeHandle());

        BodyState innerBodyState;
        ASSERT_TRUE(system.GetBodyState(worldHandle, innerBodyHandle, innerBodyState));
        EXPECT_EQ(innerBodyState.m_shapeHandle, collider->GetRootShapeHandle());

        entity.Deactivate();
        EXPECT_FALSE(system.IsValid(worldHandle, characterHandle));
        EXPECT_FALSE(system.IsValid(worldHandle, innerBodyHandle));

        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::UnregisterComponentDescriptor,
            characterDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::UnregisterComponentDescriptor,
            colliderDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::UnregisterComponentDescriptor,
            transformDescriptor);
        characterDescriptor->ReleaseDescriptor();
        colliderDescriptor->ReleaseDescriptor();
        transformDescriptor->ReleaseDescriptor();
    }

    TEST(ComponentTests, ConstraintTracksBodyLifecycleWithoutPolling)
    {
        ComponentNameDictionaryScope nameDictionary;
        AZ::ComponentDescriptor* transformDescriptor = AzFramework::TransformComponent::CreateDescriptor();
        AZ::ComponentDescriptor* colliderDescriptor = ColliderComponent::CreateDescriptor();
        AZ::ComponentDescriptor* bodyDescriptor = RigidBodyComponent::CreateDescriptor();
        AZ::ComponentDescriptor* staticBodyDescriptor = StaticRigidBodyComponent::CreateDescriptor();
        AZ::ComponentDescriptor* constraintDescriptor = ConstraintComponent::CreateDescriptor();
        AZ::ComponentDescriptor* pathDescriptor = PathComponent::CreateDescriptor();
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::RegisterComponentDescriptor,
            transformDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::RegisterComponentDescriptor,
            colliderDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::RegisterComponentDescriptor,
            bodyDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::RegisterComponentDescriptor,
            staticBodyDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::RegisterComponentDescriptor,
            constraintDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::RegisterComponentDescriptor,
            pathDescriptor);
        System system(CreateComponentSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        ColliderShapeConfiguration colliderShape;
        colliderShape.m_shape.m_geometry = SphereShapeConfiguration{};
        AZ::Entity firstBodyEntity("First constrained body");
        firstBodyEntity.CreateComponent<AzFramework::TransformComponent>();
        firstBodyEntity.CreateComponent<ColliderComponent>(AZStd::vector{colliderShape});
        StaticRigidBodyComponent* firstBody = firstBodyEntity.CreateComponent<StaticRigidBodyComponent>();
        ASSERT_TRUE(firstBody);
        firstBodyEntity.Init();
        firstBodyEntity.Activate();

        AZ::Entity secondBodyEntity("Second constrained body");
        secondBodyEntity.CreateComponent<AzFramework::TransformComponent>();
        secondBodyEntity.CreateComponent<ColliderComponent>(AZStd::vector{colliderShape});
        RigidBodyComponent* secondBody = secondBodyEntity.CreateComponent<RigidBodyComponent>();
        ASSERT_TRUE(secondBody);
        secondBodyEntity.Init();
        secondBodyEntity.Activate();

        HermitePathConfiguration pathConfiguration;
        pathConfiguration.m_points = {
            HermitePathPoint{
                .m_position = AZ::Vector3::CreateZero(),
            },
            HermitePathPoint{
                .m_position = AZ::Vector3::CreateAxisX(2.0f),
            },
        };
        AZ::Entity pathEntity("Constraint path");
        PathComponent* path = pathEntity.CreateComponent<PathComponent>(pathConfiguration);
        ASSERT_TRUE(path);
        pathEntity.Init();
        pathEntity.Activate();

        ConstraintComponentConfiguration constraintConfiguration;
        constraintConfiguration.m_firstBodyEntityId = firstBodyEntity.GetId();
        constraintConfiguration.m_secondBodyEntityId = secondBodyEntity.GetId();
        constraintConfiguration.m_geometry = PathConstraintComponentConfiguration{
            .m_pathEntityId = pathEntity.GetId(),
        };
        AZ::Entity constraintEntity("Path constraint");
        ConstraintComponent* constraint =
            constraintEntity.CreateComponent<ConstraintComponent>(constraintConfiguration);
        ASSERT_TRUE(constraint);
        constraintEntity.Init();
        constraintEntity.Activate();
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const ConstraintHandle firstConstraintHandle = constraint->GetConstraintHandle();
        ASSERT_TRUE(firstConstraintHandle);
        EXPECT_TRUE(system.IsValid(worldHandle, firstConstraintHandle));

        const BodyHandle firstBodyHandle = firstBody->GetBodyHandle();
        firstBodyEntity.Deactivate();
        EXPECT_FALSE(system.IsValid(worldHandle, firstConstraintHandle));
        EXPECT_FALSE(system.IsValid(worldHandle, firstBodyHandle));
        EXPECT_FALSE(constraint->IsSimulationEnabled());

        firstBodyEntity.Activate();
        const ConstraintHandle secondConstraintHandle = constraint->GetConstraintHandle();
        EXPECT_TRUE(secondConstraintHandle);
        EXPECT_NE(secondConstraintHandle, firstConstraintHandle);
        EXPECT_TRUE(system.IsValid(worldHandle, secondConstraintHandle));

        pathEntity.Deactivate();
        EXPECT_FALSE(system.IsValid(worldHandle, secondConstraintHandle));
        EXPECT_FALSE(constraint->IsSimulationEnabled());
        pathEntity.Activate();
        const ConstraintHandle thirdConstraintHandle = constraint->GetConstraintHandle();
        EXPECT_TRUE(thirdConstraintHandle);
        EXPECT_NE(thirdConstraintHandle, secondConstraintHandle);
        EXPECT_TRUE(system.IsValid(worldHandle, thirdConstraintHandle));

        constraintEntity.Deactivate();
        pathEntity.Deactivate();
        firstBodyEntity.Deactivate();
        secondBodyEntity.Deactivate();

        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::UnregisterComponentDescriptor,
            constraintDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::UnregisterComponentDescriptor,
            pathDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::UnregisterComponentDescriptor,
            staticBodyDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::UnregisterComponentDescriptor,
            bodyDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::UnregisterComponentDescriptor,
            colliderDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::UnregisterComponentDescriptor,
            transformDescriptor);
        constraintDescriptor->ReleaseDescriptor();
        pathDescriptor->ReleaseDescriptor();
        staticBodyDescriptor->ReleaseDescriptor();
        bodyDescriptor->ReleaseDescriptor();
        colliderDescriptor->ReleaseDescriptor();
        transformDescriptor->ReleaseDescriptor();
    }

    TEST(ComponentTests, RigidBodyOwnsShapesAndSynchronizesTransformWithoutPolling)
    {
        ComponentNameDictionaryScope nameDictionary;
        AZ::ComponentDescriptor* transformDescriptor = AzFramework::TransformComponent::CreateDescriptor();
        AZ::ComponentDescriptor* colliderDescriptor = ColliderComponent::CreateDescriptor();
        AZ::ComponentDescriptor* characterDescriptor = CharacterControllerComponent::CreateDescriptor();
        AZ::ComponentDescriptor* bodyDescriptor = RigidBodyComponent::CreateDescriptor();
        AZ::ComponentDescriptor* staticBodyDescriptor = StaticRigidBodyComponent::CreateDescriptor();
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::RegisterComponentDescriptor,
            transformDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::RegisterComponentDescriptor,
            colliderDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::RegisterComponentDescriptor,
            characterDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::RegisterComponentDescriptor,
            bodyDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::RegisterComponentDescriptor,
            staticBodyDescriptor);
        System system(CreateComponentSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        AZ::Entity entity("Moving sphere");
        entity.CreateComponent<AzFramework::TransformComponent>();

        ColliderShapeConfiguration colliderShape;
        colliderShape.m_shape.m_geometry = SphereShapeConfiguration{};
        ColliderComponent* colliderComponent =
            entity.CreateComponent<ColliderComponent>(AZStd::vector{colliderShape});
        ASSERT_TRUE(colliderComponent);

        RigidBodyConfiguration bodyConfiguration;
        bodyConfiguration.m_initialLinearVelocity = AZ::Vector3::CreateAxisX(1.0f);
        RigidBodyComponent* bodyComponent = entity.CreateComponent<RigidBodyComponent>(bodyConfiguration);
        ASSERT_TRUE(bodyComponent);

        entity.Init();
        entity.Activate();
        ASSERT_TRUE(bodyComponent->IsSimulationEnabled());
        const BodyHandle bodyHandle = bodyComponent->GetBodyHandle();

        ShapeStats shapeStats;
        ASSERT_TRUE(colliderComponent->GetRootShapeStats(shapeStats));
        EXPECT_GT(shapeStats.m_memorySize, 0);
        ShapeStats recursiveShapeStats;
        ASSERT_TRUE(colliderComponent->GetRootShapeStatsRecursive(recursiveShapeStats));
        EXPECT_GE(recursiveShapeStats.m_memorySize, shapeStats.m_memorySize);
        ShapeProperties shapeProperties;
        ASSERT_TRUE(colliderComponent->GetRootShapeProperties(shapeProperties));
        EXPECT_EQ(shapeProperties.m_kind, ShapeKind::Sphere);
        SubmergedVolumeRequest submergedVolumeRequest;
        SubmergedVolumeResult submergedVolumeResult;
        ASSERT_TRUE(colliderComponent->GetRootShapeSubmergedVolume(
            submergedVolumeRequest,
            submergedVolumeResult));
        EXPECT_NEAR(submergedVolumeResult.m_totalVolume, shapeProperties.m_volume, 1.0e-5f);
        EXPECT_NEAR(submergedVolumeResult.m_submergedVolume, 0.5f * shapeProperties.m_volume, 1.0e-5f);
        SphereShapeConfiguration sphereConfiguration;
        ASSERT_TRUE(colliderComponent->GetRootSphereConfiguration(sphereConfiguration));
        EXPECT_FLOAT_EQ(sphereConfiguration.m_radius, 0.5f);
        BoxShapeConfiguration boxConfiguration;
        EXPECT_FALSE(colliderComponent->GetRootBoxConfiguration(boxConfiguration));
        ConvexHullState convexHullState;
        EXPECT_FALSE(colliderComponent->GetRootConvexHullState(convexHullState));
        EXPECT_FALSE(colliderComponent->GetRootConvexHullTopology().IsComplete());
        MaterialHandle materialHandle;
        EXPECT_TRUE(colliderComponent->GetRootShapeMaterial(SubShapeId::Root, materialHandle));
        EXPECT_FALSE(materialHandle);
        ShapeHandle directChildShapeHandle;
        SubShapeTransform directChildTransform;
        EXPECT_TRUE(colliderComponent->GetRootDirectChildShape(
            SubShapeId::Root,
            directChildShapeHandle,
            directChildTransform));
        EXPECT_EQ(directChildShapeHandle, colliderComponent->GetRootShapeHandle());
        EXPECT_TRUE(directChildTransform.m_centerOfMassPosition.IsZero());
        EXPECT_TRUE(directChildTransform.m_rotation.IsClose(AZ::Quaternion::CreateIdentity()));
        EXPECT_TRUE(directChildTransform.m_scale.IsClose(AZ::Vector3::CreateOne()));
        EXPECT_EQ(directChildTransform.m_remainder, SubShapeId::Root);
        AZ::Vector3 surfaceNormal = AZ::Vector3::CreateZero();
        EXPECT_TRUE(colliderComponent->GetRootShapeSurfaceNormal(
            SubShapeId::Root,
            AZ::Vector3::CreateAxisX(0.5f),
            surfaceNormal));
        EXPECT_TRUE(surfaceNormal.IsClose(AZ::Vector3::CreateAxisX()));
        EXPECT_TRUE(colliderComponent->IsRootShapeScaleValid(AZ::Vector3(2.0f)));
        EXPECT_FALSE(colliderComponent->IsRootShapeScaleValid(AZ::Vector3(2.0f, 1.0f, 1.0f)));
        AZ::Vector3 validScale = AZ::Vector3::CreateZero();
        EXPECT_TRUE(colliderComponent->MakeRootShapeScaleValid(
            AZ::Vector3(2.0f, 1.0f, 1.0f),
            validScale));
        EXPECT_TRUE(colliderComponent->IsRootShapeScaleValid(validScale));

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        EXPECT_TRUE(bodyComponent->DeactivateBody());
        const BodyState inactiveState = bodyComponent->GetState();
        EXPECT_FALSE(inactiveState.m_isActive);
        EXPECT_TRUE(bodyComponent->SetTransformWhenChanged(
            inactiveState.m_transform,
            true));
        EXPECT_FALSE(bodyComponent->GetState().m_isActive);

        WorldTransform movedTransform = inactiveState.m_transform;
        movedTransform.m_position.m_x += 1.0;
        EXPECT_TRUE(bodyComponent->SetTransformWhenChanged(
            movedTransform,
            true));
        EXPECT_TRUE(bodyComponent->GetState().m_isActive);
        const WorldTransform centerOfMassTransform = bodyComponent->GetCenterOfMassTransform();
        EXPECT_EQ(centerOfMassTransform.m_position, movedTransform.m_position);
        EXPECT_TRUE(centerOfMassTransform.m_rotation.IsClose(movedTransform.m_rotation));

        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        const AZStd::span<const BodyMoveEvent> moves = system.GetEvents(worldHandle).GetBodyMoves();
        ASSERT_EQ(moves.size(), 1);
        EXPECT_EQ(moves.front().m_bodyHandle, bodyHandle);
        EXPECT_EQ(moves.front().m_entityId, entity.GetId());

        BodyNotificationBus::Event(
            entity.GetId(),
            &IBodyNotifications::OnBodyMoved,
            moves.front());
        AZ::Transform synchronizedTransform = AZ::Transform::CreateIdentity();
        AZ::TransformBus::EventResult(
            synchronizedTransform,
            entity.GetId(),
            &AZ::TransformInterface::GetWorldTM);
        EXPECT_GT(synchronizedTransform.GetTranslation().GetX(), 0.0f);

        AZ::TransformBus::Event(
            entity.GetId(),
            &AZ::TransformInterface::SetLocalUniformScale,
            2.0f);
        BodyState bodyState;
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, bodyState));
        PointOverlapRequest request;
        request.m_position = bodyState.m_transform.m_position;
        request.m_position.m_y += 0.75;
        EXPECT_TRUE(system.OverlapPointAny(worldHandle, request));

        entity.Deactivate();
        EXPECT_FALSE(system.IsValid(worldHandle, bodyHandle));

        AZ::Entity staticEntity("Static sphere");
        staticEntity.CreateComponent<AzFramework::TransformComponent>();
        staticEntity.CreateComponent<ColliderComponent>(AZStd::vector{colliderShape});
        StaticRigidBodyComponent* staticBody = staticEntity.CreateComponent<StaticRigidBodyComponent>();
        ASSERT_TRUE(staticBody);
        staticEntity.Init();
        AZ::Transform staticTransform = AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisZ(3.0f));
        staticTransform.SetUniformScale(2.0f);
        AZ::TransformBus::Event(
            staticEntity.GetId(),
            &AZ::TransformInterface::SetWorldTM,
            staticTransform);
        staticEntity.Activate();
        ASSERT_TRUE(staticBody->IsSimulationEnabled());
        BodyState staticState = staticBody->GetState();
        EXPECT_EQ(staticState.m_motionType, MotionType::Static);
        EXPECT_NEAR(staticState.m_transform.m_position.m_z, 3.0, 1.0e-6);

        staticTransform.SetTranslation(AZ::Vector3(2.0f, 1.0f, 4.0f));
        staticTransform.SetUniformScale(3.0f);
        AZ::TransformBus::Event(
            staticEntity.GetId(),
            &AZ::TransformInterface::SetWorldTM,
            staticTransform);
        staticState = staticBody->GetState();
        EXPECT_NEAR(staticState.m_transform.m_position.m_x, 2.0, 1.0e-6);
        staticEntity.Deactivate();

        AZ::Entity convexHullEntity("Static convex hull");
        convexHullEntity.CreateComponent<AzFramework::TransformComponent>();
        ColliderShapeConfiguration convexHullColliderShape;
        convexHullColliderShape.m_shape.m_geometry = ConvexHullShapeConfiguration{
            .m_points = {
                AZ::Vector3::CreateZero(),
                AZ::Vector3::CreateAxisX(4.0f),
                AZ::Vector3::CreateAxisY(2.0f),
                AZ::Vector3::CreateAxisZ(3.0f),
            },
        };
        ColliderComponent* convexHullCollider = convexHullEntity.CreateComponent<ColliderComponent>(
            AZStd::vector{convexHullColliderShape});
        ASSERT_TRUE(convexHullCollider);
        convexHullEntity.CreateComponent<StaticRigidBodyComponent>();
        convexHullEntity.Init();
        convexHullEntity.Activate();

        ASSERT_TRUE(convexHullCollider->GetRootConvexHullState(convexHullState));
        const ConvexHullTopology topology = convexHullCollider->GetRootConvexHullTopology();
        EXPECT_TRUE(topology.IsComplete());
        EXPECT_FALSE(topology.HasOverflow());
        EXPECT_EQ(topology.GetState().m_pointCount, convexHullState.m_pointCount);
        EXPECT_EQ(topology.GetPointCount(), convexHullState.m_pointCount);
        EXPECT_EQ(topology.GetPlaneCount(), convexHullState.m_faceCount);
        EXPECT_EQ(topology.GetFaceCount(), convexHullState.m_faceCount);
        for (AZ::u32 faceIndex = 0; faceIndex < topology.GetFaceCount(); ++faceIndex)
        {
            EXPECT_GE(topology.GetFaceVertexCount(faceIndex), 3);
            for (
                AZ::u32 faceVertexIndex = 0;
                faceVertexIndex < topology.GetFaceVertexCount(faceIndex);
                ++faceVertexIndex)
            {
                EXPECT_LT(
                    topology.GetFaceVertexIndex(faceIndex, faceVertexIndex),
                    topology.GetPointCount());
            }
        }
        convexHullEntity.Deactivate();

        AZ::Entity characterEntity("Character sphere");
        characterEntity.CreateComponent<AzFramework::TransformComponent>();
        ColliderComponent* characterCollider =
            characterEntity.CreateComponent<ColliderComponent>(AZStd::vector{colliderShape});
        ASSERT_TRUE(characterCollider);
        CharacterControllerComponent* character =
            characterEntity.CreateComponent<CharacterControllerComponent>();
        ASSERT_TRUE(character);
        characterEntity.Init();
        characterEntity.Activate();
        ASSERT_TRUE(character->IsSimulationEnabled());
        const CharacterHandle characterHandle = character->GetCharacterHandle();
        const CharacterState initialCharacterState = character->GetState();
        const BodyHandle characterBodyHandle = initialCharacterState.m_bodyHandle;
        EXPECT_TRUE(characterHandle);
        EXPECT_TRUE(characterBodyHandle);

        ASSERT_TRUE(character->SetVelocity(AZ::Vector3::CreateAxisY(2.0f)));
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        const AZStd::span<const BodyMoveEvent> characterMoves = system.GetEvents(worldHandle).GetBodyMoves();
        ASSERT_EQ(characterMoves.size(), 1);
        EXPECT_EQ(characterMoves.front().m_bodyHandle, characterBodyHandle);
        BodyNotificationBus::Event(
            characterEntity.GetId(),
            &IBodyNotifications::OnBodyMoved,
            characterMoves.front());

        AZ::Transform characterTransform = AZ::Transform::CreateIdentity();
        AZ::TransformBus::EventResult(
            characterTransform,
            characterEntity.GetId(),
            &AZ::TransformInterface::GetWorldTM);
        characterTransform.SetUniformScale(2.0f);
        AZ::TransformBus::Event(
            characterEntity.GetId(),
            &AZ::TransformInterface::SetWorldTM,
            characterTransform);
        CharacterState characterState = character->GetState();
        EXPECT_NE(characterState.m_shapeHandle, initialCharacterState.m_shapeHandle);
        EXPECT_EQ(characterState.m_shapeHandle, characterCollider->GetRootShapeHandle());
        PointOverlapRequest characterOverlap;
        characterOverlap.m_position = characterState.m_transform.m_position;
        characterOverlap.m_position.m_x += 0.75;
        EXPECT_TRUE(system.OverlapPointAny(worldHandle, characterOverlap));

        characterEntity.Deactivate();
        EXPECT_FALSE(system.IsValid(worldHandle, characterHandle));
        EXPECT_FALSE(system.IsValid(worldHandle, characterBodyHandle));

        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::UnregisterComponentDescriptor,
            staticBodyDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::UnregisterComponentDescriptor,
            bodyDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::UnregisterComponentDescriptor,
            characterDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::UnregisterComponentDescriptor,
            colliderDescriptor);
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::UnregisterComponentDescriptor,
            transformDescriptor);
        bodyDescriptor->ReleaseDescriptor();
        characterDescriptor->ReleaseDescriptor();
        staticBodyDescriptor->ReleaseDescriptor();
        colliderDescriptor->ReleaseDescriptor();
        transformDescriptor->ReleaseDescriptor();
    }
} // namespace Jolt
