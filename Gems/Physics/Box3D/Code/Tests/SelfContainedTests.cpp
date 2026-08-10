/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/CharacterControllerComponent.h>
#include <Box3D/ColliderComponent.h>
#include <Box3D/Cooking.h>
#include <Box3D/EffectComponents.h>
#include <Box3D/FloatEnvironment.h>
#include <Box3D/HeightfieldColliderComponent.h>
#include <Box3D/JointComponent.h>
#include <Box3D/RigidBodyComponent.h>
#include <Box3D/StaticRigidBodyComponent.h>
#include <Box3D/SystemComponent.h>
#include <Box3D/SystemInternal.h>

#include <AzCore/Interface/Interface.h>
#include <AzCore/Jobs/JobContext.h>
#include <AzCore/Jobs/JobManager.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Name/NameDictionary.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/limits.h>
#include <AzCore/std/utility/move.h>
#include <AzFramework/Physics/PhysicsSystem.h>
#include <AzTest/AzTest.h>

#include <cfenv>
#include <cstddef>

#if defined(_M_ARM64) || defined(_M_ARM64EC)
#include <arm64intr.h>
#include <intrin.h>
#elif defined(__aarch64__) && defined(__clang__)
#include <arm_acle.h>
#endif

AZ_UNIT_TEST_HOOK(DEFAULT_UNIT_TEST_ENV);

namespace Box3D::Tests
{
    static_assert(sizeof(ShapeProperties) <= 112);
    static_assert(sizeof(ShapeConfiguration) <= 272);
    static_assert(sizeof(HeightfieldShapeConfiguration) <= 112);
    static_assert(sizeof(QueryFilter) <= 48);
    static_assert(sizeof(RaycastRequest) <= 96);
    static_assert(sizeof(ShapeCastRequest) <= 144);
    static_assert(sizeof(OverlapRequest) <= 128);
    static_assert(sizeof(AabbOverlapRequest) <= 80);
    static_assert(sizeof(QueryHit) <= 80);
    static_assert(sizeof(CharacterConfiguration) <= 176);
    static_assert(sizeof(RigidBodyConfiguration) <= 160);
    static_assert(sizeof(SystemConfiguration) <= 88);

    namespace
    {
#if defined(_M_ARM64) || defined(_M_ARM64EC) || defined(__aarch64__)
        constexpr AZ::u64 Arm64FormatAndRoundingMask = AZ::u64{0x7FF} << 16;
        constexpr AZ::u64 Arm64ExceptionAndBFloatMask = (AZ::u64{1} << 15) | (AZ::u64{1} << 13) | (AZ::u64{0x1F} << 8);
        constexpr AZ::u64 Arm64AlternativeBehaviorMask = AZ::u64{0x7};
        constexpr AZ::u64 Arm64FloatControlModeMask =
            Arm64FormatAndRoundingMask | Arm64ExceptionAndBFloatMask | Arm64AlternativeBehaviorMask;
        constexpr AZ::u64 Arm64FloatControlPoison = (AZ::u64{1} << 22) | (AZ::u64{1} << 24) | (AZ::u64{1} << 25);

        [[nodiscard]]
        AZ::u64 ReadArm64FloatControl()
        {
#if defined(_M_ARM64) || defined(_M_ARM64EC)
            return static_cast<AZ::u64>(_ReadStatusReg(ARM64_FPCR));
#elif defined(__clang__)
            return __arm_rsr64("fpcr");
#else
            AZ::u64 control = 0;
            __asm__ volatile("mrs %0, fpcr" : "=r"(control));
            return control;
#endif
        }

        void WriteArm64FloatControl(
            AZ::u64 control)
        {
#if defined(_M_ARM64) || defined(_M_ARM64EC)
            _WriteStatusReg(ARM64_FPCR, static_cast<__int64>(control));
#elif defined(__clang__)
            __arm_wsr64("fpcr", control);
#else
            __asm__ volatile("msr fpcr, %0" : : "r"(control));
#endif
        }
#endif

        BodyHandle CreateBody(
            System& system,
            WorldHandle worldHandle,
            BodyType bodyType,
            const AZ::Vector3& position)
        {
            RigidBodyConfiguration configuration;
            configuration.m_bodyType = bodyType;
            configuration.m_transform = AZ::Transform::CreateTranslation(position);
            return system.CreateBody(worldHandle, configuration);
        }

        ShapeHandle CreateSphere(
            System& system,
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float radius = 0.5f)
        {
            ShapeConfiguration configuration;
            configuration.m_geometry = SphereShapeConfiguration{radius};
            return system.CreateShape(worldHandle, bodyHandle, configuration);
        }

        struct ReferenceSimulation final
        {
            AZ::u64 m_digest = 0;
            AZStd::vector<AZ::u8> m_recording;
        };

        ReferenceSimulation SimulateReferenceStack(
            AZ::u32 workerCount,
            AZ::JobContext* jobContext = nullptr)
        {
            SystemConfiguration systemConfiguration;
            systemConfiguration.m_workerCount = workerCount;
            System system(systemConfiguration, jobContext);
            const WorldHandle worldHandle = system.GetDefaultWorldHandle();
            EXPECT_TRUE(system.StartRecording(worldHandle, 0));

            const BodyHandle ground = CreateBody(system, worldHandle, BodyType::Static, AZ::Vector3(0.0f, 0.0f, -0.5f));
            ShapeConfiguration groundShape;
            groundShape.m_geometry = BoxShapeConfiguration{AZ::Vector3(10.0f, 10.0f, 0.5f)};
            EXPECT_TRUE(system.CreateShape(worldHandle, ground, groundShape).IsValid());

            const BodyHandle fallingBody = CreateBody(system, worldHandle, BodyType::Dynamic, AZ::Vector3(0.0f, 0.0f, 4.0f));
            EXPECT_TRUE(CreateSphere(system, worldHandle, fallingBody).IsValid());
            for (AZ::u32 tick = 0; tick < 180; ++tick)
            {
                EXPECT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
            }
            ReferenceSimulation result;
            result.m_digest = system.GetStateDigest(worldHandle);
            EXPECT_TRUE(system.StopRecording(worldHandle, result.m_recording));
            return result;
        }

        void SetJointBodies(
            JointConfiguration& configuration,
            BodyHandle parentBody,
            BodyHandle childBody)
        {
            AZStd::visit(
                [parentBody, childBody](auto& typedConfiguration)
                {
                    typedConfiguration.m_common.m_parentBody = parentBody;
                    typedConfiguration.m_common.m_childBody = childBody;
                },
                configuration);
        }

        struct ContactCallbackState final
        {
            AZ::u32 m_filterCalls = 0;
            AZ::u32 m_preSolveCalls = 0;
            bool m_shouldCollide = true;
            bool m_shouldSolve = true;
        };

        bool FilterContact(
            ShapeHandle,
            ShapeHandle,
            void* userData)
        {
            auto& state = *static_cast<ContactCallbackState*>(userData);
            ++state.m_filterCalls;
            return state.m_shouldCollide;
        }

        bool FilterPreSolve(
            ShapeHandle,
            ShapeHandle,
            const AZ::Vector3&,
            const AZ::Vector3&,
            void* userData)
        {
            auto& state = *static_cast<ContactCallbackState*>(userData);
            ++state.m_preSolveCalls;
            return state.m_shouldSolve;
        }

        float MixMaterials(
            float valueA,
            SurfaceTypeId,
            float valueB,
            SurfaceTypeId)
        {
            return 0.5f * (valueA + valueB);
        }

        struct MaterialMixState final
        {
            SurfaceTypeId m_surfaceTypeA = 0;
            SurfaceTypeId m_surfaceTypeB = 0;
            AZ::u32 m_callCount = 0;
        };

        MaterialMixState s_materialMixState;

        float MixMaterial(
            float valueA,
            SurfaceTypeId surfaceTypeA,
            float valueB,
            SurfaceTypeId surfaceTypeB)
        {
            s_materialMixState.m_surfaceTypeA = surfaceTypeA;
            s_materialMixState.m_surfaceTypeB = surfaceTypeB;
            ++s_materialMixState.m_callCount;
            return 0.5f * (valueA + valueB);
        }
    } // namespace

    TEST(
        Box3DSystemTests,
        RegistersOnlyProviderOwnedInterfaces)
    {
        AzPhysics::SystemInterface* genericSystem = AZ::Interface<AzPhysics::SystemInterface>::Get();
        EXPECT_EQ(AZ::Interface<ISystem>::Get(), nullptr);
        EXPECT_EQ(AZ::Interface<ICooking>::Get(), nullptr);
        EXPECT_EQ(AZ::Interface<IDiagnostics>::Get(), nullptr);
        EXPECT_EQ(AZ::Interface<IEffects>::Get(), nullptr);
        {
            System system;
            EXPECT_EQ(AZ::Interface<ISystem>::Get(), &system);
            EXPECT_EQ(AZ::Interface<ICooking>::Get(), &system);
            EXPECT_EQ(AZ::Interface<IDiagnostics>::Get(), &system);
            EXPECT_EQ(AZ::Interface<IEffects>::Get(), &system);
            EXPECT_EQ(AZ::Interface<AzPhysics::SystemInterface>::Get(), genericSystem);
        }
        EXPECT_EQ(AZ::Interface<ISystem>::Get(), nullptr);
        EXPECT_EQ(AZ::Interface<ICooking>::Get(), nullptr);
        EXPECT_EQ(AZ::Interface<IDiagnostics>::Get(), nullptr);
        EXPECT_EQ(AZ::Interface<IEffects>::Get(), nullptr);
        EXPECT_EQ(AZ::Interface<AzPhysics::SystemInterface>::Get(), genericSystem);
    }

    TEST(
        Box3DSystemTests,
        ScriptBusesUseProviderQualifiedNames)
    {
        AZ::BehaviorContext behaviorContext;
        SystemComponent::Reflect(&behaviorContext);
        RigidBodyComponent::Reflect(&behaviorContext);
        StaticRigidBodyComponent::Reflect(&behaviorContext);
        ColliderComponent::Reflect(&behaviorContext);
        CharacterControllerComponent::Reflect(&behaviorContext);
        JointComponent::Reflect(&behaviorContext);
        ExplosionComponent::Reflect(&behaviorContext);
        WindComponent::Reflect(&behaviorContext);
        HeightfieldColliderComponent::Reflect(&behaviorContext);

        EXPECT_NE(behaviorContext.m_classes.find("Box3D::RigidBodyComponent"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("Box3D::StaticRigidBodyComponent"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("Box3D::ColliderComponent"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("Box3D::CharacterControllerComponent"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("Box3D::JointComponent"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("Box3D::ExplosionComponent"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("Box3D::WindComponent"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("Box3D::HeightfieldColliderComponent"), behaviorContext.m_classes.end());

        const auto worldBus = behaviorContext.m_ebuses.find("Box3DWorldRequestBus");
        ASSERT_NE(worldBus, behaviorContext.m_ebuses.end());
        EXPECT_NE(worldBus->second->m_events.find("GetDefaultWorldHandle"), worldBus->second->m_events.end());
        EXPECT_NE(worldBus->second->m_events.find("RaycastClosest"), worldBus->second->m_events.end());
        EXPECT_NE(worldBus->second->m_events.find("RaycastClosestBatch"), worldBus->second->m_events.end());
        EXPECT_NE(worldBus->second->m_events.find("Raycast"), worldBus->second->m_events.end());
        EXPECT_NE(worldBus->second->m_events.find("OverlapAabb"), worldBus->second->m_events.end());
        EXPECT_NE(worldBus->second->m_events.find("GetContactPointCount"), worldBus->second->m_events.end());
        EXPECT_NE(worldBus->second->m_events.find("GetContactPointAt"), worldBus->second->m_events.end());
        EXPECT_NE(worldBus->second->m_events.find("ShapeCastSphere"), worldBus->second->m_events.end());
        EXPECT_NE(worldBus->second->m_events.find("OverlapConvexHull"), worldBus->second->m_events.end());
        EXPECT_NE(behaviorContext.m_classes.find("QueryHit"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("QueryHitCollection"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("ClosestQueryResultCollection"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("RaycastRequestCollection"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("RaycastRequest"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("ConvexCastParameters"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("SystemConfiguration"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("SphereShapeConfiguration"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("ContactPoint"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("ContactEvent"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("SensorEvent"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("BodyMoveEvent"), behaviorContext.m_classes.end());
        const auto worldNotificationBus = behaviorContext.m_ebuses.find("Box3DWorldNotificationBus");
        ASSERT_NE(worldNotificationBus, behaviorContext.m_ebuses.end());

        const auto rigidBodyBus = behaviorContext.m_ebuses.find("Box3DRigidBodyRequestBus");
        ASSERT_NE(rigidBodyBus, behaviorContext.m_ebuses.end());
        EXPECT_NE(rigidBodyBus->second->m_events.find("ApplyForce"), rigidBodyBus->second->m_events.end());
        EXPECT_NE(rigidBodyBus->second->m_events.find("SetHitEventsEnabled"), rigidBodyBus->second->m_events.end());
        EXPECT_NE(rigidBodyBus->second->m_events.find("SetKinematicTarget"), rigidBodyBus->second->m_events.end());
        EXPECT_NE(rigidBodyBus->second->m_events.find("GetState"), rigidBodyBus->second->m_events.end());
        EXPECT_NE(rigidBodyBus->second->m_events.find("GetName"), rigidBodyBus->second->m_events.end());
        EXPECT_NE(rigidBodyBus->second->m_events.find("SetName"), rigidBodyBus->second->m_events.end());
        EXPECT_NE(rigidBodyBus->second->m_events.find("GetProperties"), rigidBodyBus->second->m_events.end());
        EXPECT_NE(rigidBodyBus->second->m_events.find("GetClosestPoint"), rigidBodyBus->second->m_events.end());
        EXPECT_NE(rigidBodyBus->second->m_events.find("GetMassProperties"), rigidBodyBus->second->m_events.end());
        EXPECT_NE(rigidBodyBus->second->m_events.find("GetWorldHandle"), rigidBodyBus->second->m_events.end());
        EXPECT_NE(rigidBodyBus->second->m_events.find("GetBodyHandle"), rigidBodyBus->second->m_events.end());
        EXPECT_NE(behaviorContext.m_ebuses.find("Box3DRigidBodyNotificationBus"), behaviorContext.m_ebuses.end());
        EXPECT_NE(behaviorContext.m_classes.find("BodyState"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("BodyProperties"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("MassProperties"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("ClosestPoint"), behaviorContext.m_classes.end());

        const auto characterBus = behaviorContext.m_ebuses.find("Box3DCharacterRequestBus");
        ASSERT_NE(characterBus, behaviorContext.m_ebuses.end());
        EXPECT_NE(characterBus->second->m_events.find("Move"), characterBus->second->m_events.end());
        EXPECT_NE(characterBus->second->m_events.find("GetConfiguration"), characterBus->second->m_events.end());
        EXPECT_NE(characterBus->second->m_events.find("UpdateConfiguration"), characterBus->second->m_events.end());
        EXPECT_NE(characterBus->second->m_events.find("GetState"), characterBus->second->m_events.end());
        EXPECT_NE(behaviorContext.m_classes.find("CharacterConfiguration"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("CharacterSupport"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("CharacterState"), behaviorContext.m_classes.end());

        const auto heightfieldBus = behaviorContext.m_ebuses.find("Box3DHeightfieldRequestBus");
        ASSERT_NE(heightfieldBus, behaviorContext.m_ebuses.end());
        EXPECT_NE(heightfieldBus->second->m_events.find("GetColumnCount"), heightfieldBus->second->m_events.end());
        EXPECT_NE(heightfieldBus->second->m_events.find("GetShapeHandle"), heightfieldBus->second->m_events.end());
        EXPECT_NE(heightfieldBus->second->m_events.find("GetHeights"), heightfieldBus->second->m_events.end());
        EXPECT_NE(heightfieldBus->second->m_events.find("GetMaterialIndices"), heightfieldBus->second->m_events.end());
        EXPECT_NE(heightfieldBus->second->m_events.find("ReplaceHeightfield"), heightfieldBus->second->m_events.end());
        EXPECT_NE(heightfieldBus->second->m_events.find("UpdateHeights"), heightfieldBus->second->m_events.end());
        EXPECT_NE(heightfieldBus->second->m_events.find("UpdateMaterials"), heightfieldBus->second->m_events.end());
        EXPECT_NE(behaviorContext.m_classes.find("HeightfieldShapeConfiguration"), behaviorContext.m_classes.end());

        const auto colliderBus = behaviorContext.m_ebuses.find("Box3DColliderRequestBus");
        ASSERT_NE(colliderBus, behaviorContext.m_ebuses.end());
        EXPECT_NE(colliderBus->second->m_events.find("GetShapeCount"), colliderBus->second->m_events.end());
        EXPECT_NE(colliderBus->second->m_events.find("GetShapeHandleAt"), colliderBus->second->m_events.end());
        EXPECT_NE(colliderBus->second->m_events.find("SetCollisionFilter"), colliderBus->second->m_events.end());
        EXPECT_NE(colliderBus->second->m_events.find("UpdateSphere"), colliderBus->second->m_events.end());
        EXPECT_NE(colliderBus->second->m_events.find("UpdateTriangleMesh"), colliderBus->second->m_events.end());
        EXPECT_NE(colliderBus->second->m_events.find("UpdateCompound"), colliderBus->second->m_events.end());
        EXPECT_NE(colliderBus->second->m_events.find("SetMaterials"), colliderBus->second->m_events.end());
        EXPECT_NE(behaviorContext.m_classes.find("ShapeProperties"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("TriangleMeshShapeConfiguration"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("CompoundChildShapeConfiguration"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("CompoundShapeConfiguration"), behaviorContext.m_classes.end());

        const auto materialBus = behaviorContext.m_ebuses.find("Box3DMaterialRequestBus");
        ASSERT_NE(materialBus, behaviorContext.m_ebuses.end());
        EXPECT_NE(materialBus->second->m_events.find("CreateMaterial"), materialBus->second->m_events.end());
        EXPECT_NE(materialBus->second->m_events.find("UpdateMaterial"), materialBus->second->m_events.end());
        EXPECT_NE(materialBus->second->m_events.find("GetMaterial"), materialBus->second->m_events.end());
        EXPECT_NE(materialBus->second->m_events.find("DestroyMaterial"), materialBus->second->m_events.end());
        EXPECT_NE(behaviorContext.m_classes.find("MaterialConfiguration"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("MaterialHandleCollection"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("MaterialConfigurationCollection"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("MaterialResult"), behaviorContext.m_classes.end());

        const auto cookingBus = behaviorContext.m_ebuses.find("Box3DCookingRequestBus");
        ASSERT_NE(cookingBus, behaviorContext.m_ebuses.end());
        EXPECT_NE(cookingBus->second->m_events.find("CookSphere"), cookingBus->second->m_events.end());
        EXPECT_NE(cookingBus->second->m_events.find("CookCompound"), cookingBus->second->m_events.end());
        EXPECT_NE(cookingBus->second->m_events.find("RaycastCookedShape"), cookingBus->second->m_events.end());
        EXPECT_NE(cookingBus->second->m_events.find("DestroyCookedShape"), cookingBus->second->m_events.end());
        EXPECT_NE(behaviorContext.m_classes.find("GeometryHit"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("CookedRaycastResult"), behaviorContext.m_classes.end());

        const auto diagnosticsBus = behaviorContext.m_ebuses.find("Box3DDiagnosticsRequestBus");
        ASSERT_NE(diagnosticsBus, behaviorContext.m_ebuses.end());
        EXPECT_NE(diagnosticsBus->second->m_events.find("GetWorldStatistics"), diagnosticsBus->second->m_events.end());
        EXPECT_NE(diagnosticsBus->second->m_events.find("StartRecording"), diagnosticsBus->second->m_events.end());
        EXPECT_NE(diagnosticsBus->second->m_events.find("StopRecording"), diagnosticsBus->second->m_events.end());
        EXPECT_NE(diagnosticsBus->second->m_events.find("ValidateRecording"), diagnosticsBus->second->m_events.end());
        EXPECT_NE(diagnosticsBus->second->m_events.find("CreateReplay"), diagnosticsBus->second->m_events.end());
        EXPECT_NE(diagnosticsBus->second->m_events.find("StepReplay"), diagnosticsBus->second->m_events.end());
        EXPECT_NE(diagnosticsBus->second->m_events.find("GetReplayBody"), diagnosticsBus->second->m_events.end());
        EXPECT_NE(diagnosticsBus->second->m_events.find("GetReplayQueryHit"), diagnosticsBus->second->m_events.end());
        EXPECT_NE(diagnosticsBus->second->m_events.find("DestroyReplay"), diagnosticsBus->second->m_events.end());
        EXPECT_NE(diagnosticsBus->second->m_events.find("RebuildStaticTree"), diagnosticsBus->second->m_events.end());
        EXPECT_NE(behaviorContext.m_classes.find("StepProfileSnapshot"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("SimulationCounters"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("CapacityHighWaterMarks"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("StatisticsSnapshot"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("RecordingData"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("RecordingResult"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("ReplayHandle"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("ReplayInfo"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("ReplayBody"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("ReplayQuery"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("ReplayQueryHit"), behaviorContext.m_classes.end());

        const auto jointBus = behaviorContext.m_ebuses.find("Box3DJointRequestBus");
        ASSERT_NE(jointBus, behaviorContext.m_ebuses.end());
        EXPECT_NE(jointBus->second->m_events.find("GetWorldHandle"), jointBus->second->m_events.end());
        EXPECT_NE(jointBus->second->m_events.find("GetJointHandle"), jointBus->second->m_events.end());
        EXPECT_NE(jointBus->second->m_events.find("GetMeasurements"), jointBus->second->m_events.end());
        EXPECT_NE(jointBus->second->m_events.find("WakeBodies"), jointBus->second->m_events.end());
        EXPECT_NE(jointBus->second->m_events.find("GetDistanceConfiguration"), jointBus->second->m_events.end());
        EXPECT_NE(jointBus->second->m_events.find("UpdateDistanceConfiguration"), jointBus->second->m_events.end());
        EXPECT_NE(jointBus->second->m_events.find("GetWheelState"), jointBus->second->m_events.end());
        EXPECT_NE(behaviorContext.m_classes.find("JointThresholdEvent"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("JointCommonConfiguration"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("DistanceJointConfiguration"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("WheelJointConfiguration"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("JointMeasurements"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("DistanceJointState"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_ebuses.find("Box3DJointNotificationBus"), behaviorContext.m_ebuses.end());
        const auto explosionBus = behaviorContext.m_ebuses.find("Box3DExplosionRequestBus");
        ASSERT_NE(explosionBus, behaviorContext.m_ebuses.end());
        EXPECT_NE(explosionBus->second->m_events.find("UpdateConfiguration"), explosionBus->second->m_events.end());
        const auto windBus = behaviorContext.m_ebuses.find("Box3DWindRequestBus");
        ASSERT_NE(windBus, behaviorContext.m_ebuses.end());
        EXPECT_NE(windBus->second->m_events.find("UpdateConfiguration"), windBus->second->m_events.end());
        EXPECT_NE(behaviorContext.m_classes.find("ExplosionConfiguration"), behaviorContext.m_classes.end());
        EXPECT_NE(behaviorContext.m_classes.find("WindConfiguration"), behaviorContext.m_classes.end());
        EXPECT_EQ(behaviorContext.m_ebuses.find("RigidBodyRequestBus"), behaviorContext.m_ebuses.end());
    }

    TEST(
        Box3DSystemTests,
        RecycledSlotsInvalidateOldHandles)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const BodyHandle oldHandle = CreateBody(system, worldHandle, BodyType::Dynamic, AZ::Vector3::CreateZero());
        ASSERT_TRUE(oldHandle.IsValid());
        EXPECT_TRUE(system.DestroyBody(worldHandle, oldHandle));
        const BodyHandle newHandle = CreateBody(system, worldHandle, BodyType::Dynamic, AZ::Vector3::CreateZero());
        ASSERT_TRUE(newHandle.IsValid());
        EXPECT_NE(oldHandle, newHandle);
        BodyState state;
        EXPECT_FALSE(system.GetBodyState(worldHandle, oldHandle, state));
        EXPECT_TRUE(system.GetBodyState(worldHandle, newHandle, state));
    }

    TEST(
        Box3DSystemTests,
        WorldQueryViewsUseValidatedWorldLifetime)
    {
        System system;
        WorldConfiguration configuration;
        configuration.m_name = AZ_NAME_LITERAL("QueryView");
        const WorldHandle worldHandle = system.CreateWorld(configuration);
        const IWorldQueries* worldQueries = system.GetWorldQueries(worldHandle);
        ASSERT_NE(worldQueries, nullptr);

        const BodyHandle bodyHandle = CreateBody(system, worldHandle, BodyType::Static, 3.0f * AZ::Vector3::CreateAxisX());
        ASSERT_TRUE(CreateSphere(system, worldHandle, bodyHandle).IsValid());
        RaycastRequest request;
        request.m_distance = 10.0f;
        QueryHit hit;
        ASSERT_TRUE(worldQueries->RaycastClosest(request, hit));
        EXPECT_EQ(hit.m_bodyHandle, bodyHandle);

        EXPECT_TRUE(system.DestroyWorld(worldHandle));
        EXPECT_EQ(system.GetWorldQueries(worldHandle), nullptr);
    }

    TEST(
        Box3DSystemTests,
        JointScriptAdaptersPreserveConcreteConfigurationTypes)
    {
        DistanceJointConfiguration distance;
        distance.m_length = 3.5f;
        JointComponent component(distance);
        EXPECT_FLOAT_EQ(component.GetDistanceConfiguration().m_length, 3.5f);
        EXPECT_FLOAT_EQ(component.GetWheelConfiguration().m_spinSpeed, 0.0f);

        WheelJointConfiguration wheel;
        wheel.m_spinSpeed = 4.25f;
        EXPECT_TRUE(component.UpdateWheelConfiguration(wheel));
        EXPECT_FLOAT_EQ(component.GetWheelConfiguration().m_spinSpeed, 4.25f);
        EXPECT_FLOAT_EQ(component.GetDistanceConfiguration().m_length, 1.0f);
    }

    TEST(
        Box3DSystemTests,
        WorldsOwnIndependentGravityEnablementAndNames)
    {
        System system;
        WorldConfiguration configuration;
        configuration.m_name = AZ_NAME_LITERAL("LowGravity");
        configuration.m_gravity = AZ::Vector3(0.0f, 0.0f, -1.0f);
        const WorldHandle worldHandle = system.CreateWorld(configuration);
        ASSERT_TRUE(worldHandle.IsValid());
        EXPECT_EQ(system.FindWorld(configuration.m_name), worldHandle);

        AZ::Vector3 gravity;
        ASSERT_TRUE(system.GetWorldGravity(worldHandle, gravity));
        EXPECT_TRUE(gravity.IsClose(configuration.m_gravity));
        EXPECT_TRUE(system.SetWorldGravity(worldHandle, AZ::Vector3(0.0f, 0.0f, -2.0f)));
        ASSERT_TRUE(system.GetWorldGravity(worldHandle, gravity));
        EXPECT_TRUE(gravity.IsClose(AZ::Vector3(0.0f, 0.0f, -2.0f)));

        const AZ::Vector3 invalidGravity((AZStd::numeric_limits<float>::quiet_NaN)(), 0.0f, 0.0f);
        EXPECT_FALSE(system.SetWorldGravity(worldHandle, invalidGravity));
        ASSERT_TRUE(system.GetWorldGravity(worldHandle, gravity));
        EXPECT_TRUE(gravity.IsClose(AZ::Vector3(0.0f, 0.0f, -2.0f)));

        EXPECT_TRUE(system.SetWorldEnabled(worldHandle, false));
        EXPECT_FALSE(system.IsWorldEnabled(worldHandle));
        WorldConfiguration retainedConfiguration;
        ASSERT_TRUE(system.GetWorldConfiguration(worldHandle, retainedConfiguration));
        EXPECT_EQ(retainedConfiguration.m_name, configuration.m_name);
        EXPECT_TRUE(retainedConfiguration.m_gravity.IsClose(AZ::Vector3(0.0f, 0.0f, -2.0f)));
        EXPECT_FALSE(retainedConfiguration.m_enabled);

        const BodyHandle boundsBody = CreateBody(system, worldHandle, BodyType::Static, AZ::Vector3::CreateAxisX(3.0f));
        ASSERT_TRUE(CreateSphere(system, worldHandle, boundsBody).IsValid());
        EXPECT_TRUE(system.GetWorldAabb(worldHandle).Contains(AZ::Vector3::CreateAxisX(3.0f)));
        EXPECT_FALSE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        EXPECT_TRUE(system.DestroyWorld(worldHandle));
        EXPECT_FALSE(system.GetWorldGravity(worldHandle, gravity));
        EXPECT_FALSE(system.GetWorldConfiguration(worldHandle, retainedConfiguration));
        EXPECT_FALSE(system.GetWorldAabb(worldHandle).IsValid());
        EXPECT_FALSE(system.FindWorld(configuration.m_name).IsValid());
    }

    TEST(
        Box3DSystemTests,
        InvalidWorldAndBodyConfigurationsFailBeforeCreatingObjects)
    {
        System system;
        WorldConfiguration worldConfiguration;
        worldConfiguration.m_name = AZ_NAME_LITERAL("InvalidWorld");
        worldConfiguration.m_fixedTimeStep = (AZStd::numeric_limits<float>::quiet_NaN)();
        AZ_TEST_START_TRACE_SUPPRESSION;
        EXPECT_FALSE(system.CreateWorld(worldConfiguration).IsValid());
        AZ_TEST_STOP_TRACE_SUPPRESSION(1);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        RigidBodyConfiguration bodyConfiguration;
        bodyConfiguration.m_transform = AZ::Transform::CreateFromQuaternion(AZ::Quaternion(0.0f, 0.0f, 0.0f, 2.0f));
        EXPECT_FALSE(system.CreateBody(worldHandle, bodyConfiguration).IsValid());

        bodyConfiguration = {};
        bodyConfiguration.m_linearDamping = -1.0f;
        EXPECT_FALSE(system.CreateBody(worldHandle, bodyConfiguration).IsValid());

        bodyConfiguration = {};
        bodyConfiguration.m_bodyType = static_cast<BodyType>(255);
        EXPECT_FALSE(system.CreateBody(worldHandle, bodyConfiguration).IsValid());

        bodyConfiguration = {};
        bodyConfiguration.m_transform.SetUniformScale(2.0f);
        EXPECT_FALSE(system.CreateBody(worldHandle, bodyConfiguration).IsValid());
    }

    TEST(
        Box3DSystemTests,
        InvalidBodyUpdatesAreTransactional)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const BodyHandle bodyHandle = CreateBody(system, worldHandle, BodyType::Dynamic, AZ::Vector3(1.0f, 2.0f, 3.0f));
        ASSERT_TRUE(bodyHandle.IsValid());

        BodyState initialState;
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, initialState));
        const AZ::Transform invalidTransform =
            AZ::Transform::CreateFromQuaternionAndTranslation(AZ::Quaternion(0.0f, 0.0f, 0.0f, 2.0f), AZ::Vector3(8.0f, 9.0f, 10.0f));
        EXPECT_FALSE(system.SetBodyTransform(worldHandle, bodyHandle, invalidTransform));
        AZ::Transform scaledTransform = initialState.m_transform;
        scaledTransform.SetUniformScale(2.0f);
        EXPECT_FALSE(system.SetBodyTransform(worldHandle, bodyHandle, scaledTransform));

        BodyState stateAfterInvalidUpdate;
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, stateAfterInvalidUpdate));
        EXPECT_TRUE(stateAfterInvalidUpdate.m_transform.IsClose(initialState.m_transform));

        BodyProperties properties;
        ASSERT_TRUE(system.GetBodyProperties(worldHandle, bodyHandle, properties));
        properties.m_bodyType = static_cast<BodyType>(255);
        EXPECT_FALSE(system.SetBodyProperties(worldHandle, bodyHandle, properties));

        MassProperties massProperties;
        massProperties.m_mass = 0.0f;
        EXPECT_TRUE(system.SetMassProperties(worldHandle, bodyHandle, massProperties));
    }

    TEST(
        Box3DSystemTests,
        MaterialLifetimeTracksShapeReferences)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        MaterialConfiguration materialConfiguration;
        materialConfiguration.m_friction = 0.25f;
        const MaterialHandle materialHandle = system.CreateMaterial(materialConfiguration);
        ASSERT_TRUE(materialHandle.IsValid());

        const BodyHandle bodyHandle = CreateBody(system, worldHandle, BodyType::Static, AZ::Vector3::CreateZero());
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{};
        shapeConfiguration.m_properties.m_materials.push_back(materialHandle);
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, bodyHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle.IsValid());
        EXPECT_FALSE(system.DestroyMaterial(materialHandle));

        materialConfiguration.m_friction = 0.8f;
        EXPECT_TRUE(system.UpdateMaterial(materialHandle, materialConfiguration));
        MaterialConfiguration updatedConfiguration;
        ASSERT_TRUE(system.GetMaterial(materialHandle, updatedConfiguration));
        EXPECT_FLOAT_EQ(updatedConfiguration.m_friction, 0.8f);

        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
        EXPECT_TRUE(system.DestroyMaterial(materialHandle));

        materialConfiguration.m_friction = 0.4f;
        const MaterialHandle replacementHandle = system.CreateMaterial(materialConfiguration);
        ASSERT_TRUE(replacementHandle.IsValid());
        EXPECT_NE(replacementHandle, materialHandle);
        EXPECT_FALSE(system.GetMaterial(materialHandle, updatedConfiguration));
        ASSERT_TRUE(system.GetMaterial(replacementHandle, updatedConfiguration));
        EXPECT_FLOAT_EQ(updatedConfiguration.m_friction, 0.4f);
    }

    TEST(
        Box3DSystemTests,
        MaterialMixingUsesSurfaceTypesAndDebugAppearanceIsOptIn)
    {
        s_materialMixState = {};
        SystemConfiguration systemConfiguration;
        systemConfiguration.m_workerCount = 1;
        systemConfiguration.m_frictionCallback = MixMaterial;
        System system(systemConfiguration);
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();

        MaterialConfiguration groundMaterialConfiguration;
        groundMaterialConfiguration.m_surfaceTypeId = 101;
        groundMaterialConfiguration.m_debugColor = AZ::Colors::Red;
        const MaterialHandle groundMaterial = system.CreateMaterial(groundMaterialConfiguration);
        ASSERT_TRUE(groundMaterial.IsValid());
        EXPECT_EQ(system.ResolveMaterial(groundMaterial).m_debugColor, 0);

        groundMaterialConfiguration.m_debugAppearanceEnabled = true;
        ASSERT_TRUE(system.UpdateMaterial(groundMaterial, groundMaterialConfiguration));
        EXPECT_NE(system.ResolveMaterial(groundMaterial).m_debugColor, 0);

        MaterialConfiguration bodyMaterialConfiguration;
        bodyMaterialConfiguration.m_surfaceTypeId = 202;
        const MaterialHandle bodyMaterial = system.CreateMaterial(bodyMaterialConfiguration);
        ASSERT_TRUE(bodyMaterial.IsValid());

        const BodyHandle groundBody = CreateBody(system, worldHandle, BodyType::Static, AZ::Vector3(0.0f, 0.0f, -0.5f));
        ShapeConfiguration groundShapeConfiguration;
        groundShapeConfiguration.m_geometry = BoxShapeConfiguration{AZ::Vector3(5.0f, 5.0f, 0.5f)};
        groundShapeConfiguration.m_properties.m_materials = {groundMaterial};
        ASSERT_TRUE(system.CreateShape(worldHandle, groundBody, groundShapeConfiguration).IsValid());

        const BodyHandle dynamicBody = CreateBody(system, worldHandle, BodyType::Dynamic, AZ::Vector3(0.0f, 0.0f, 0.45f));
        ShapeConfiguration bodyShapeConfiguration;
        bodyShapeConfiguration.m_geometry = SphereShapeConfiguration{0.5f};
        bodyShapeConfiguration.m_properties.m_materials = {bodyMaterial};
        ASSERT_TRUE(system.CreateShape(worldHandle, dynamicBody, bodyShapeConfiguration).IsValid());
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));

        EXPECT_GT(s_materialMixState.m_callCount, 0);
        const bool expectedOrder = s_materialMixState.m_surfaceTypeA == 101 && s_materialMixState.m_surfaceTypeB == 202;
        const bool reverseOrder = s_materialMixState.m_surfaceTypeA == 202 && s_materialMixState.m_surfaceTypeB == 101;
        EXPECT_TRUE(expectedOrder || reverseOrder);
    }

    TEST(
        Box3DSystemTests,
        InvalidMaterialAndShapeUpdatesPreserveLiveState)
    {
        System system;
        MaterialConfiguration materialConfiguration;
        materialConfiguration.m_friction = -1.0f;
        EXPECT_FALSE(system.CreateMaterial(materialConfiguration).IsValid());

        materialConfiguration = {};
        materialConfiguration.m_friction = 0.25f;
        const MaterialHandle materialHandle = system.CreateMaterial(materialConfiguration);
        ASSERT_TRUE(materialHandle.IsValid());
        MaterialConfiguration invalidMaterial = materialConfiguration;
        invalidMaterial.m_rollingResistance = -1.0f;
        EXPECT_FALSE(system.UpdateMaterial(materialHandle, invalidMaterial));
        MaterialConfiguration retainedMaterial;
        ASSERT_TRUE(system.GetMaterial(materialHandle, retainedMaterial));
        EXPECT_FLOAT_EQ(retainedMaterial.m_friction, materialConfiguration.m_friction);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const BodyHandle bodyHandle = CreateBody(system, worldHandle, BodyType::Static, AZ::Vector3::CreateZero());
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{1.0f};
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, bodyHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle.IsValid());
        const AZ::Aabb initialAabb = system.GetShapeAabb(worldHandle, shapeHandle);

        shapeConfiguration.m_geometry = SphereShapeConfiguration{-1.0f};
        EXPECT_FALSE(system.UpdateShape(worldHandle, shapeHandle, shapeConfiguration));
        EXPECT_TRUE(system.GetShapeAabb(worldHandle, shapeHandle).IsClose(initialAabb));
    }

    TEST(
        Box3DSystemTests,
        InvalidJointAndCharacterUpdatesPreserveLiveObjects)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const BodyHandle parent = CreateBody(system, worldHandle, BodyType::Dynamic, AZ::Vector3::CreateZero());
        const BodyHandle child = CreateBody(system, worldHandle, BodyType::Dynamic, AZ::Vector3::CreateAxisX());

        DistanceJointConfiguration jointConfiguration;
        jointConfiguration.m_common.m_parentBody = parent;
        jointConfiguration.m_common.m_childBody = child;
        const JointHandle jointHandle = system.CreateJoint(worldHandle, jointConfiguration);
        ASSERT_TRUE(jointHandle.IsValid());
        jointConfiguration.m_length = -1.0f;
        EXPECT_FALSE(system.UpdateJoint(worldHandle, jointHandle, jointConfiguration));
        JointMeasurements measurements;
        EXPECT_TRUE(system.GetJointMeasurements(worldHandle, jointHandle, measurements));

        CharacterConfiguration characterConfiguration;
        const CharacterHandle characterHandle = system.CreateCharacter(worldHandle, characterConfiguration);
        ASSERT_TRUE(characterHandle.IsValid());
        CharacterState initialState;
        ASSERT_TRUE(system.GetCharacterState(worldHandle, characterHandle, initialState));
        characterConfiguration.m_maximumSpeed = -1.0f;
        EXPECT_FALSE(system.UpdateCharacter(worldHandle, characterHandle, characterConfiguration));
        CharacterState retainedState;
        ASSERT_TRUE(system.GetCharacterState(worldHandle, characterHandle, retainedState));
        EXPECT_TRUE(retainedState.m_basePosition.IsClose(initialState.m_basePosition));
    }

    TEST(
        Box3DSystemTests,
        ShapeUniformScaleIsPartOfLocalTransform)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const BodyHandle bodyHandle = CreateBody(system, worldHandle, BodyType::Static, AZ::Vector3::CreateZero());

        ShapeConfiguration box;
        box.m_geometry = BoxShapeConfiguration{AZ::Vector3::CreateOne() * 0.5f};
        box.m_properties.m_localTransform = AZ::Transform::CreateFromQuaternion(AZ::Quaternion::CreateRotationZ(AZ::DegToRad(45.0f)));
        box.m_properties.m_localTransform.SetUniformScale(2.0f);
        const ShapeHandle boxHandle = system.CreateShape(worldHandle, bodyHandle, box);
        ASSERT_TRUE(boxHandle.IsValid());
        EXPECT_GT(system.GetShapeAabb(worldHandle, boxHandle).GetExtents().GetX(), 2.0f);

        ShapeConfiguration sphere;
        sphere.m_geometry = SphereShapeConfiguration{0.5f};
        sphere.m_properties.m_localTransform.SetUniformScale(2.0f);
        EXPECT_TRUE(system.CreateShape(worldHandle, bodyHandle, sphere).IsValid());

        box.m_properties.m_localTransform.SetUniformScale(0.001f);
        EXPECT_FALSE(system.CreateShape(worldHandle, bodyHandle, box).IsValid());

        CompoundShapeConfiguration compound;
        CompoundChildShapeConfiguration child;
        child.m_geometry = SphereShapeConfiguration{0.5f};
        child.m_localTransform.SetUniformScale(2.0f);
        compound.m_children.push_back(AZStd::move(child));
        ShapeConfiguration compoundShape;
        compoundShape.m_geometry = AZStd::move(compound);
        EXPECT_TRUE(system.CreateShape(worldHandle, bodyHandle, compoundShape).IsValid());
    }

    TEST(
        Box3DSystemTests,
        FixedStepSimulationIsRepeatable)
    {
        const ReferenceSimulation first = SimulateReferenceStack(1);
        const ReferenceSimulation second = SimulateReferenceStack(1);
        EXPECT_NE(first.m_digest, 0);
        EXPECT_EQ(first.m_digest, second.m_digest);
    }

    TEST(
        Box3DSystemTests,
        SimulationAndRecordingAreDeterministicAcrossWorkerCounts)
    {
        const ReferenceSimulation serial = SimulateReferenceStack(1);
        const ReferenceSimulation nativeParallel = SimulateReferenceStack(4);

        AZ::JobManagerDesc jobManagerDescriptor;
        jobManagerDescriptor.m_workerThreads.resize(4);
        AZ::JobManager jobManager(jobManagerDescriptor);
        AZ::JobContext jobContext(jobManager);
        const ReferenceSimulation externalSerial = SimulateReferenceStack(1, &jobContext);
        const ReferenceSimulation externalParallel = SimulateReferenceStack(4, &jobContext);

        EXPECT_EQ(serial.m_digest, nativeParallel.m_digest);
        EXPECT_EQ(serial.m_digest, externalSerial.m_digest);
        EXPECT_EQ(serial.m_digest, externalParallel.m_digest);
        ASSERT_FALSE(serial.m_recording.empty());

        System validationSystem;
        EXPECT_TRUE(validationSystem.ValidateRecording(serial.m_recording, 1));
        EXPECT_TRUE(validationSystem.ValidateRecording(serial.m_recording, 4));
    }

    TEST(
        Box3DSystemTests,
        SimulationRestoresCallingThreadFloatEnvironment)
    {
        const int originalRoundingMode = std::fegetround();
        ASSERT_NE(originalRoundingMode, -1);
        ASSERT_EQ(std::fesetround(FE_DOWNWARD), 0);

        System system;
        const bool stepSucceeded = system.StepWorld(system.GetDefaultWorldHandle(), 1.0f / 60.0f);
        const int restoredRoundingMode = std::fegetround();

        ASSERT_EQ(std::fesetround(originalRoundingMode), 0);
        EXPECT_TRUE(stepSucceeded);
        EXPECT_EQ(restoredRoundingMode, FE_DOWNWARD);
    }

#if defined(_M_ARM64) || defined(_M_ARM64EC) || defined(__aarch64__)
    TEST(
        Box3DSystemTests,
        DeterministicScopeCanonicalizesAndRestoresArm64FloatControl)
    {
        const AZ::u64 originalControl = ReadArm64FloatControl();
        WriteArm64FloatControl(originalControl | Arm64FloatControlPoison);
        const AZ::u64 poisonedControl = ReadArm64FloatControl();

        {
            const DeterministicFloatScope floatScope;
            EXPECT_EQ(ReadArm64FloatControl() & Arm64FloatControlModeMask, 0);
        }

        EXPECT_EQ(ReadArm64FloatControl(), poisonedControl);
        WriteArm64FloatControl(originalControl);
    }
#endif

    TEST(
        Box3DSystemTests,
        RecordedMutationsAreIndependentOfCallerFloatEnvironment)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const int originalRoundingMode = std::fegetround();
        ASSERT_NE(originalRoundingMode, -1);
        ASSERT_EQ(std::fesetround(FE_UPWARD), 0);

        const bool recordingStarted = system.StartRecording(worldHandle, 0);
        RigidBodyConfiguration bodyConfiguration;
        bodyConfiguration.m_transform = AZ::Transform::CreateTranslation(AZ::Vector3(0.1f, 0.2f, 0.3f));
        bodyConfiguration.m_linearVelocity = AZ::Vector3(0.4f, 0.5f, 0.6f);
        const BodyHandle bodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{0.7f};
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, bodyHandle, shapeConfiguration);
        const bool impulseApplied = system.ApplyLinearImpulse(worldHandle, bodyHandle, AZ::Vector3(0.8f, 0.9f, 1.1f), true);
        const bool stepSucceeded = system.StepWorld(worldHandle, 1.0f / 60.0f);
        AZStd::vector<AZ::u8> recording;
        const bool recordingStopped = system.StopRecording(worldHandle, recording);
        const int restoredRoundingMode = std::fegetround();

        ASSERT_EQ(std::fesetround(originalRoundingMode), 0);
        ASSERT_TRUE(recordingStarted);
        ASSERT_TRUE(bodyHandle.IsValid());
        ASSERT_TRUE(shapeHandle.IsValid());
        ASSERT_TRUE(impulseApplied);
        ASSERT_TRUE(stepSucceeded);
        ASSERT_TRUE(recordingStopped);
        EXPECT_EQ(restoredRoundingMode, FE_UPWARD);
        EXPECT_TRUE(system.ValidateRecording(recording, 1));
        EXPECT_TRUE(system.ValidateRecording(recording, 4));
    }

    TEST(
        Box3DSystemTests,
        RecordingRejectsCallbacksThatCannotBeReplayed)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ContactCallbackState callbackState;

        ASSERT_TRUE(system.SetContactCallbacks(worldHandle, FilterContact, FilterPreSolve, &callbackState));
        EXPECT_FALSE(system.StartRecording(worldHandle, 0));
        ASSERT_TRUE(system.SetContactCallbacks(worldHandle, nullptr, nullptr, nullptr));
        ASSERT_TRUE(system.StartRecording(worldHandle, 0));
        EXPECT_FALSE(system.SetContactCallbacks(worldHandle, FilterContact, FilterPreSolve, &callbackState));

        SystemConfiguration configuration = system.GetConfiguration();
        configuration.m_frictionCallback = MixMaterials;
        configuration.m_workerCount = 1;
        configuration.m_enableContinuous = false;
        system.UpdateConfiguration(configuration);
        EXPECT_EQ(system.GetConfiguration(), SystemConfiguration{});

        AZStd::vector<AZ::u8> recording;
        EXPECT_TRUE(system.StopRecording(worldHandle, recording));
    }

    TEST(
        Box3DSystemTests,
        ConfigurationChangesApplyToExistingWorlds)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const BodyHandle bodyHandle = CreateBody(system, worldHandle, BodyType::Dynamic, AZ::Vector3::CreateZero());
        ASSERT_TRUE(CreateSphere(system, worldHandle, bodyHandle).IsValid());

        SystemConfiguration configuration = system.GetConfiguration();
        configuration.m_workerCount = 2;
        configuration.m_enableContinuous = false;
        configuration.m_enableWarmStarting = false;
        configuration.m_enableSpeculative = false;
        configuration.m_contactHertz = 42.0f;
        configuration.m_restitutionThreshold = 2.0f;
        configuration.m_stallWarningThresholdSeconds = 0.0f;
        configuration.m_lengthUnitsPerMeter = 0.0f;
        system.UpdateConfiguration(configuration);
        EXPECT_FLOAT_EQ(system.GetConfiguration().m_restitutionThreshold, 2.0f);
        EXPECT_FLOAT_EQ(system.GetConfiguration().m_lengthUnitsPerMeter, 1.0f);
        EXPECT_FLOAT_EQ(system.GetConfiguration().m_stallWarningThresholdSeconds, (AZStd::numeric_limits<float>::max)());
        EXPECT_NE(system.GetCompatibilityFingerprint().find("restitution=2"), AZStd::string_view::npos);
        EXPECT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));

        WorldStatistics statistics;
        ASSERT_TRUE(system.GetWorldStatistics(worldHandle, StatisticsFlags::Counters, statistics));
        EXPECT_EQ(statistics.m_counters.m_workerCount, 2);
        EXPECT_GT(statistics.m_counters.m_taskCount, 0);
    }

    TEST(
        Box3DSystemTests,
        MaximumLinearSpeedChangesApplyToExistingWorlds)
    {
        SystemConfiguration configuration;
        configuration.m_maximumLinearSpeed = 5.0f;
        System system(configuration);
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();

        RigidBodyConfiguration bodyConfiguration;
        bodyConfiguration.m_gravityScale = 0.0f;
        bodyConfiguration.m_linearVelocity = 100.0f * AZ::Vector3::CreateAxisX();
        const BodyHandle bodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(CreateSphere(system, worldHandle, bodyHandle).IsValid());
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));

        BodyState bodyState;
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, bodyState));
        EXPECT_LE(bodyState.m_linearVelocity.GetLength(), 5.001f);

        configuration.m_maximumLinearSpeed = 2.0f;
        system.UpdateConfiguration(configuration);
        ASSERT_TRUE(system.SetLinearVelocity(worldHandle, bodyHandle, 100.0f * AZ::Vector3::CreateAxisX()));
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, bodyState));
        EXPECT_LE(bodyState.m_linearVelocity.GetLength(), 2.001f);
    }

    TEST(
        Box3DSystemTests,
        DisablingSleepWakesExistingBodies)
    {
        SystemConfiguration configuration;
        configuration.m_enableSleep = true;
        System system(configuration);
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();

        RigidBodyConfiguration bodyConfiguration;
        bodyConfiguration.m_gravityScale = 0.0f;
        const BodyHandle bodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(CreateSphere(system, worldHandle, bodyHandle).IsValid());
        for (AZ::u32 step = 0; step < 120; ++step)
        {
            ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        }

        BodyState bodyState;
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, bodyState));
        ASSERT_FALSE(bodyState.m_isAwake);

        configuration.m_enableSleep = false;
        system.UpdateConfiguration(configuration);
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, bodyState));
        EXPECT_TRUE(bodyState.m_isAwake);
    }

    TEST(
        Box3DSystemTests,
        ContinuousCollisionPreventsFastBodyTunneling)
    {
        const auto simulateFastBody = [](bool enableContinuous)
        {
            SystemConfiguration configuration;
            configuration.m_subStepCount = 1;
            configuration.m_workerCount = 1;
            configuration.m_enableContinuous = enableContinuous;
            configuration.m_enableSpeculative = false;
            configuration.m_enableSleep = false;
            System system(configuration);
            const WorldHandle worldHandle = system.GetDefaultWorldHandle();

            const BodyHandle wall = CreateBody(system, worldHandle, BodyType::Static, AZ::Vector3::CreateZero());
            ShapeConfiguration wallShape;
            wallShape.m_geometry = BoxShapeConfiguration{AZ::Vector3(0.05f, 5.0f, 5.0f)};
            EXPECT_TRUE(system.CreateShape(worldHandle, wall, wallShape).IsValid());

            RigidBodyConfiguration bodyConfiguration;
            bodyConfiguration.m_transform = AZ::Transform::CreateTranslation(-2.0f * AZ::Vector3::CreateAxisX());
            bodyConfiguration.m_linearVelocity = 300.0f * AZ::Vector3::CreateAxisX();
            bodyConfiguration.m_gravityScale = 0.0f;
            bodyConfiguration.m_enableSleep = false;
            bodyConfiguration.m_isBullet = true;
            const BodyHandle fastBody = system.CreateBody(worldHandle, bodyConfiguration);
            EXPECT_TRUE(CreateSphere(system, worldHandle, fastBody, 0.25f).IsValid());
            EXPECT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));

            BodyState bodyState;
            EXPECT_TRUE(system.GetBodyState(worldHandle, fastBody, bodyState));
            return bodyState.m_transform.GetTranslation().GetX();
        };

        EXPECT_GT(simulateFastBody(false), 1.0f);
        EXPECT_LT(simulateFastBody(true), 0.0f);
    }

    TEST(
        Box3DSystemTests,
        HitEventThresholdChangesApplyToExistingWorlds)
    {
        SystemConfiguration configuration;
        configuration.m_hitEventThreshold = 1000.0f;
        System system(configuration);
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();

        const BodyHandle ground = CreateBody(system, worldHandle, BodyType::Static, AZ::Vector3(0.0f, 0.0f, -0.5f));
        ShapeConfiguration groundShape;
        groundShape.m_geometry = BoxShapeConfiguration{AZ::Vector3(4.0f, 4.0f, 0.5f)};
        ASSERT_TRUE(system.CreateShape(worldHandle, ground, groundShape).IsValid());

        RigidBodyConfiguration bodyConfiguration;
        bodyConfiguration.m_transform = AZ::Transform::CreateTranslation(3.0f * AZ::Vector3::CreateAxisZ());
        bodyConfiguration.m_linearVelocity = -10.0f * AZ::Vector3::CreateAxisZ();
        bodyConfiguration.m_gravityScale = 0.0f;
        const BodyHandle fallingBody = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(CreateSphere(system, worldHandle, fallingBody).IsValid());
        for (AZ::u32 step = 0; step < 30; ++step)
        {
            ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
            EXPECT_TRUE(system.GetStepEvents(worldHandle).m_contactHits.empty());
        }

        configuration.m_hitEventThreshold = 0.0f;
        system.UpdateConfiguration(configuration);
        ASSERT_TRUE(system.SetBodyTransform(worldHandle, fallingBody, AZ::Transform::CreateTranslation(3.0f * AZ::Vector3::CreateAxisZ())));
        ASSERT_TRUE(system.SetLinearVelocity(worldHandle, fallingBody, -10.0f * AZ::Vector3::CreateAxisZ()));
        bool sawHit = false;
        for (AZ::u32 step = 0; step < 30 && !sawHit; ++step)
        {
            ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
            sawHit = !system.GetStepEvents(worldHandle).m_contactHits.empty();
        }
        EXPECT_TRUE(sawHit);
    }

    TEST(
        Box3DSystemTests,
        CreatesEveryShapeFamily)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const BodyHandle bodyHandle = CreateBody(system, worldHandle, BodyType::Static, AZ::Vector3::CreateZero());
        ASSERT_TRUE(bodyHandle.IsValid());

        AZStd::array<ShapeConfiguration, 8> configurations;
        configurations[0].m_geometry = SphereShapeConfiguration{};
        configurations[1].m_geometry = CapsuleShapeConfiguration{};
        configurations[2].m_geometry = BoxShapeConfiguration{};
        configurations[3].m_geometry = CylinderShapeConfiguration{};
        configurations[4].m_geometry = ConvexHullShapeConfiguration{
            {AZ::Vector3(0.0f, 0.0f, 0.0f), AZ::Vector3(1.0f, 0.0f, 0.0f), AZ::Vector3(0.0f, 1.0f, 0.0f), AZ::Vector3(0.0f, 0.0f, 1.0f)}};
        configurations[5].m_geometry = TriangleMeshShapeConfiguration{
            {AZ::Vector3(0.0f, 0.0f, 0.0f), AZ::Vector3(1.0f, 0.0f, 0.0f), AZ::Vector3(0.0f, 1.0f, 0.0f)}, {0, 1, 2}, {}};
        configurations[6].m_geometry =
            HeightfieldShapeConfiguration{{0.0f, 0.0f, 0.0f, 0.0f}, {}, 2, 2, AZ::Vector2::CreateOne(), 1.0f, false};
        CompoundShapeConfiguration compound;
        compound.m_children.push_back({SphereShapeConfiguration{}, AZ::Transform::CreateIdentity(), 0});
        configurations[7].m_geometry = AZStd::move(compound);

        for (size_t configurationIndex = 0; configurationIndex < configurations.size(); ++configurationIndex)
        {
            SCOPED_TRACE(configurationIndex);
            EXPECT_TRUE(system.CreateShape(worldHandle, bodyHandle, configurations[configurationIndex]).IsValid());
        }
    }

    TEST(
        Box3DSystemTests,
        CreatesEveryJointFamily)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const BodyHandle parent = CreateBody(system, worldHandle, BodyType::Dynamic, AZ::Vector3::CreateZero());
        const BodyHandle child = CreateBody(system, worldHandle, BodyType::Dynamic, AZ::Vector3::CreateAxisX());
        ASSERT_TRUE(parent.IsValid());
        ASSERT_TRUE(child.IsValid());

        AZStd::array<JointConfiguration, 9> configurations{ParallelJointConfiguration{},
                                                           DistanceJointConfiguration{},
                                                           FilterJointConfiguration{},
                                                           MotorJointConfiguration{},
                                                           PrismaticJointConfiguration{},
                                                           RevoluteJointConfiguration{},
                                                           SphericalJointConfiguration{},
                                                           WeldJointConfiguration{},
                                                           WheelJointConfiguration{}};
        AZStd::array<JointHandle, 9> jointHandles;
        for (size_t jointIndex = 0; jointIndex < configurations.size(); ++jointIndex)
        {
            JointConfiguration& configuration = configurations[jointIndex];
            SetJointBodies(configuration, parent, child);
            jointHandles[jointIndex] = system.CreateJoint(worldHandle, configuration);
            EXPECT_TRUE(jointHandles[jointIndex].IsValid());
        }

        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        constexpr AZStd::array<size_t, 9> expectedStateIndices{0, 1, 0, 0, 2, 3, 4, 0, 5};
        for (size_t jointIndex = 0; jointIndex < configurations.size(); ++jointIndex)
        {
            SCOPED_TRACE(jointIndex);
            JointConfiguration retainedConfiguration;
            ASSERT_TRUE(system.GetJointConfiguration(worldHandle, jointHandles[jointIndex], retainedConfiguration));
            EXPECT_EQ(retainedConfiguration.index(), jointIndex);

            JointMeasurements measurements;
            ASSERT_TRUE(system.GetJointMeasurements(worldHandle, jointHandles[jointIndex], measurements));
            EXPECT_EQ(measurements.m_state.index(), expectedStateIndices[jointIndex]);
            EXPECT_TRUE(measurements.m_constraintForce.IsFinite());
            EXPECT_TRUE(measurements.m_constraintTorque.IsFinite());

            AZStd::visit(
                [](auto& typedConfiguration)
                {
                    typedConfiguration.m_common.m_drawScale = 2.0f;
                },
                configurations[jointIndex]);
            ASSERT_TRUE(system.UpdateJoint(worldHandle, jointHandles[jointIndex], configurations[jointIndex]));
            ASSERT_TRUE(system.GetJointConfiguration(worldHandle, jointHandles[jointIndex], retainedConfiguration));
            AZStd::visit(
                [](const auto& typedConfiguration)
                {
                    EXPECT_FLOAT_EQ(typedConfiguration.m_common.m_drawScale, 2.0f);
                },
                retainedConfiguration);
        }
    }

    TEST(
        Box3DSystemTests,
        QueryReportsRequiredCapacityAndKeepsClosestHits)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        for (float x : {2.0f, 4.0f})
        {
            const BodyHandle bodyHandle = CreateBody(system, worldHandle, BodyType::Static, AZ::Vector3::CreateAxisX(x));
            ASSERT_TRUE(CreateSphere(system, worldHandle, bodyHandle).IsValid());
        }

        RaycastRequest request;
        request.m_start = AZ::Vector3::CreateZero();
        request.m_direction = AZ::Vector3::CreateAxisX();
        request.m_distance = 10.0f;
        AZStd::array<QueryHit, 1> hits;
        const QueryResult result = system.Raycast(worldHandle, request, hits);
        EXPECT_EQ(result.m_hitCount, 1);
        EXPECT_EQ(result.m_requiredHitCount, 2);
        EXPECT_TRUE(result.HasOverflow());
        EXPECT_NEAR(hits[0].m_distance, 1.5f, 0.01f);
    }

    TEST(
        Box3DSystemTests,
        ReportsContactPhasesHitsAndBodyMoves)
    {
        SystemConfiguration systemConfiguration;
        systemConfiguration.m_hitEventThreshold = 0.0f;
        System system(systemConfiguration);
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();

        const BodyHandle ground = CreateBody(system, worldHandle, BodyType::Static, AZ::Vector3(0.0f, 0.0f, -0.5f));
        ShapeConfiguration groundShape;
        groundShape.m_geometry = BoxShapeConfiguration{AZ::Vector3(4.0f, 4.0f, 0.5f)};
        ASSERT_TRUE(system.CreateShape(worldHandle, ground, groundShape).IsValid());

        const BodyHandle fallingBody = CreateBody(system, worldHandle, BodyType::Dynamic, AZ::Vector3(0.0f, 0.0f, 3.0f));
        ASSERT_TRUE(CreateSphere(system, worldHandle, fallingBody).IsValid());

        bool sawBegin = false;
        bool sawHit = false;
        for (AZ::u32 step = 0; step < 120 && !sawBegin; ++step)
        {
            ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
            const StepEvents events = system.GetStepEvents(worldHandle);
            EXPECT_TRUE(
                AZStd::is_sorted(
                    events.m_bodyMoves.begin(),
                    events.m_bodyMoves.end(),
                    [](const BodyMoveEvent& left, const BodyMoveEvent& right)
                    {
                        return left.m_bodyHandle < right.m_bodyHandle;
                    }));
            sawHit = sawHit || !events.m_contactHits.empty();
            for (const ContactEvent& event : events.m_contactEvents)
            {
                if (event.m_phase == EventPhase::Begin)
                {
                    sawBegin = true;
                    EXPECT_GT(event.m_pointCount, 0);
                    EXPECT_LE(event.m_firstPoint + event.m_pointCount, events.m_contactPoints.size());
                }
            }
        }
        ASSERT_TRUE(sawBegin);
        EXPECT_TRUE(sawHit);

        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        StepEvents events = system.GetStepEvents(worldHandle);
        EXPECT_NE(
            AZStd::find_if(
                events.m_contactEvents.begin(),
                events.m_contactEvents.end(),
                [](const ContactEvent& event)
                {
                    return event.m_phase == EventPhase::Persist;
                }),
            events.m_contactEvents.end());

        ASSERT_TRUE(system.SetBodyTransform(worldHandle, fallingBody, AZ::Transform::CreateTranslation(AZ::Vector3(0.0f, 0.0f, 5.0f))));
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        events = system.GetStepEvents(worldHandle);
        EXPECT_NE(
            AZStd::find_if(
                events.m_contactEvents.begin(),
                events.m_contactEvents.end(),
                [](const ContactEvent& event)
                {
                    return event.m_phase == EventPhase::End;
                }),
            events.m_contactEvents.end());
    }

    TEST(
        Box3DSystemTests,
        ReportsSensorTransitions)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const BodyHandle sensorBody = CreateBody(system, worldHandle, BodyType::Static, AZ::Vector3::CreateZero());
        ShapeConfiguration sensorConfiguration;
        sensorConfiguration.m_geometry = SphereShapeConfiguration{1.0f};
        sensorConfiguration.m_properties.m_isSensor = true;
        sensorConfiguration.m_properties.m_enableSensorEvents = true;
        ASSERT_TRUE(system.CreateShape(worldHandle, sensorBody, sensorConfiguration).IsValid());

        RigidBodyConfiguration visitorConfiguration;
        visitorConfiguration.m_bodyType = BodyType::Dynamic;
        visitorConfiguration.m_transform = AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisX(4.0f));
        visitorConfiguration.m_linearVelocity = -5.0f * AZ::Vector3::CreateAxisX();
        visitorConfiguration.m_gravityScale = 0.0f;
        visitorConfiguration.m_isBullet = true;
        const BodyHandle visitorBody = system.CreateBody(worldHandle, visitorConfiguration);
        ShapeConfiguration visitorShape;
        visitorShape.m_geometry = SphereShapeConfiguration{};
        visitorShape.m_properties.m_enableSensorEvents = true;
        ASSERT_TRUE(system.CreateShape(worldHandle, visitorBody, visitorShape).IsValid());
        bool sawBegin = false;
        bool sawEnd = false;
        for (AZ::u32 step = 0; step < 120 && !sawEnd; ++step)
        {
            ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
            for (const SensorEvent& event : system.GetStepEvents(worldHandle).m_sensorEvents)
            {
                sawBegin = sawBegin || event.m_phase == EventPhase::Begin;
                sawEnd = sawEnd || event.m_phase == EventPhase::End;
            }
        }
        EXPECT_TRUE(sawBegin);
        EXPECT_TRUE(sawEnd);
    }

    TEST(
        Box3DSystemTests,
        InvokesOptInCollisionAndPreSolveCallbacks)
    {
        System system;
        ContactCallbackState callbackState;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ASSERT_TRUE(system.SetContactCallbacks(worldHandle, FilterContact, FilterPreSolve, &callbackState));

        const BodyHandle bodyA = CreateBody(system, worldHandle, BodyType::Static, AZ::Vector3::CreateZero());
        const BodyHandle bodyB = CreateBody(system, worldHandle, BodyType::Dynamic, AZ::Vector3::CreateAxisZ(0.5f));
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{1.0f};
        shapeConfiguration.m_properties.m_enableCustomFiltering = true;
        shapeConfiguration.m_properties.m_enablePreSolveEvents = true;
        ASSERT_TRUE(system.CreateShape(worldHandle, bodyA, shapeConfiguration).IsValid());
        ASSERT_TRUE(system.CreateShape(worldHandle, bodyB, shapeConfiguration).IsValid());

        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        EXPECT_GT(callbackState.m_filterCalls, 0);
        EXPECT_GT(callbackState.m_preSolveCalls, 0);
    }

    TEST(
        Box3DSystemTests,
        HonorsCollisionGroupOverrides)
    {
        for (const AZ::s32 groupIndex : {-7, 7})
        {
            CollisionFilter filterA;
            CollisionFilter filterB;
            filterA.m_groupIndex = groupIndex;
            filterB.m_groupIndex = groupIndex;
            filterA.m_maskBits = AZStd::numeric_limits<AZ::u64>::max();
            if (groupIndex > 0)
            {
                filterA.m_maskBits = 0;
            }
            filterB.m_maskBits = filterA.m_maskBits;
            EXPECT_EQ(filterA.Allows(filterB), groupIndex > 0);

            System system;
            const WorldHandle worldHandle = system.GetDefaultWorldHandle();
            const BodyHandle staticBody = CreateBody(system, worldHandle, BodyType::Static, AZ::Vector3::CreateZero());
            const BodyHandle dynamicBody = CreateBody(system, worldHandle, BodyType::Dynamic, AZ::Vector3::CreateAxisZ(0.5f));
            ShapeConfiguration shapeConfiguration;
            shapeConfiguration.m_geometry = SphereShapeConfiguration{1.0f};
            shapeConfiguration.m_properties.m_collisionFilter = filterA;
            shapeConfiguration.m_properties.m_enableContactEvents = true;
            ASSERT_TRUE(system.CreateShape(worldHandle, staticBody, shapeConfiguration).IsValid());
            shapeConfiguration.m_properties.m_collisionFilter = filterB;
            ASSERT_TRUE(system.CreateShape(worldHandle, dynamicBody, shapeConfiguration).IsValid());

            ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
            EXPECT_EQ(!system.GetStepEvents(worldHandle).m_contactEvents.empty(), groupIndex > 0);
        }
    }

    TEST(
        Box3DSystemTests,
        CharacterMovesAndReportsState)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        CharacterConfiguration configuration;
        const CharacterHandle characterHandle = system.CreateCharacter(worldHandle, configuration);
        ASSERT_TRUE(characterHandle.IsValid());
        EXPECT_TRUE(system.MoveCharacter(worldHandle, characterHandle, AZ::Vector3::CreateAxisX(), 0.25f));
        EXPECT_TRUE(system.StepWorld(worldHandle, 0.25f));
        CharacterState state;
        ASSERT_TRUE(system.GetCharacterState(worldHandle, characterHandle, state));
        EXPECT_NEAR(state.m_basePosition.GetX(), 0.25f, 0.001f);
    }
} // namespace Box3D::Tests
