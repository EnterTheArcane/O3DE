/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AZTestShared/Utils/Utils.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/IO/ByteContainerStream.h>
#include <AzCore/Serialization/Json/JsonSerialization.h>
#include <AzCore/Serialization/Utils.h>
#include <AzFramework/Components/TransformComponent.h>
#include <PhysX/MathConversion.h>
#include <PhysX/SystemComponentBus.h>
#include <Source/ArticulationLinkComponent.h>
#include <Source/ArticulationUtils.h>
#include <Source/Common/PhysXSceneQueryHelpers.h>
#include <Source/System/PhysXSystem.h>
#include <Source/Utils.h>
#include <Tests/PhysXGenericTestFixture.h>
#include <Tests/PhysXTestCommon.h>
#include <limits>

namespace PhysX
{
    static_assert(GetPxArticulationJointType(ArticulationJointType::Fix) == physx::PxArticulationJointType::eFIX);
    static_assert(GetPxArticulationJointType(ArticulationJointType::Hinge) == physx::PxArticulationJointType::eREVOLUTE);
    static_assert(GetPxArticulationJointType(ArticulationJointType::Prismatic) == physx::PxArticulationJointType::ePRISMATIC);
    static_assert(GetArticulationJointType(physx::PxArticulationJointType::eFIX) == ArticulationJointType::Fix);
    static_assert(GetArticulationJointType(physx::PxArticulationJointType::eREVOLUTE) == ArticulationJointType::Hinge);
    static_assert(GetArticulationJointType(physx::PxArticulationJointType::ePRISMATIC) == ArticulationJointType::Prismatic);

    static_assert(GetPxArticulationAxis(ArticulationJointAxis::Twist) == physx::PxArticulationAxis::eTWIST);
    static_assert(GetPxArticulationAxis(ArticulationJointAxis::SwingY) == physx::PxArticulationAxis::eSWING1);
    static_assert(GetPxArticulationAxis(ArticulationJointAxis::SwingZ) == physx::PxArticulationAxis::eSWING2);
    static_assert(GetPxArticulationAxis(ArticulationJointAxis::X) == physx::PxArticulationAxis::eX);
    static_assert(GetPxArticulationAxis(ArticulationJointAxis::Y) == physx::PxArticulationAxis::eY);
    static_assert(GetPxArticulationAxis(ArticulationJointAxis::Z) == physx::PxArticulationAxis::eZ);
    static_assert(GetArticulationJointAxis(physx::PxArticulationAxis::eTWIST) == ArticulationJointAxis::Twist);
    static_assert(GetArticulationJointAxis(physx::PxArticulationAxis::eSWING1) == ArticulationJointAxis::SwingY);
    static_assert(GetArticulationJointAxis(physx::PxArticulationAxis::eSWING2) == ArticulationJointAxis::SwingZ);
    static_assert(GetArticulationJointAxis(physx::PxArticulationAxis::eX) == ArticulationJointAxis::X);
    static_assert(GetArticulationJointAxis(physx::PxArticulationAxis::eY) == ArticulationJointAxis::Y);
    static_assert(GetArticulationJointAxis(physx::PxArticulationAxis::eZ) == ArticulationJointAxis::Z);

    static_assert(GetPxArticulationMotion(ArticulationJointMotionType::Free) == physx::PxArticulationMotion::eFREE);
    static_assert(GetPxArticulationMotion(ArticulationJointMotionType::Limited) == physx::PxArticulationMotion::eLIMITED);
    static_assert(GetPxArticulationMotion(ArticulationJointMotionType::Locked) == physx::PxArticulationMotion::eLOCKED);
    static_assert(GetArticulationJointMotionType(physx::PxArticulationMotion::eFREE) == ArticulationJointMotionType::Free);
    static_assert(GetArticulationJointMotionType(physx::PxArticulationMotion::eLIMITED) == ArticulationJointMotionType::Limited);
    static_assert(GetArticulationJointMotionType(physx::PxArticulationMotion::eLOCKED) == ArticulationJointMotionType::Locked);

    static_assert(SceneQueryHelpers::GetPxHitType(AzPhysics::SceneQuery::QueryHitType::None) == physx::PxQueryHitType::eNONE);
    static_assert(SceneQueryHelpers::GetPxHitType(AzPhysics::SceneQuery::QueryHitType::Touch) == physx::PxQueryHitType::eTOUCH);
    static_assert(SceneQueryHelpers::GetPxHitType(AzPhysics::SceneQuery::QueryHitType::Block) == physx::PxQueryHitType::eBLOCK);

    class PhysXUpgradeTest : public GenericPhysicsInterfaceTest
    {
    protected:
        void SetUp() override
        {
            GenericPhysicsInterfaceTest::SetUp();
            m_cookingParams = *GetPhysXSystem()->GetPxCookingParams();
            m_defaultScene->SetGravity(AZ::Vector3(0.0f, 0.0f, -9.81f));
        }

        void TearDown() override
        {
            if (m_child)
            {
                m_child->Deactivate();
            }
            if (m_root)
            {
                m_root->Deactivate();
            }
            m_child.reset();
            m_root.reset();
            *GetPhysXSystem()->GetPxCookingParams() = m_cookingParams;
            GenericPhysicsInterfaceTest::TearDown();
        }

        void CreateArticulation(
            ArticulationJointType jointType = ArticulationJointType::Fix, const ArticulationJointMotorProperties& motor = {})
        {
            m_root = AZStd::make_unique<AZ::Entity>("Articulation root");
            m_root->CreateComponent<AzFramework::TransformComponent>();
            m_rootLink = m_root->CreateComponent<ArticulationLinkComponent>();
            m_rootLink->m_config.m_entityId = m_root->GetId();
            m_rootLink->m_config.m_isFixedBase = true;
            m_rootLink->m_articulationLinkData = AZStd::make_shared<ArticulationLinkData>();
            m_rootLink->m_articulationLinkData->m_articulationLinkConfiguration = m_rootLink->m_config;

            m_child = AZStd::make_unique<AZ::Entity>("Articulation child");
            AZ::TransformConfig transform;
            transform.m_parentId = m_root->GetId();
            transform.m_localTransform = AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisX());
            transform.m_worldTransform = transform.m_localTransform;
            m_child->CreateComponent<AzFramework::TransformComponent>()->SetConfiguration(transform);
            m_childLink = m_child->CreateComponent<ArticulationLinkComponent>();
            m_childLink->m_config.m_entityId = m_child->GetId();
            m_childLink->m_config.m_mass = 2.0f;
            m_childLink->m_config.m_linearDamping = 0.0f;
            m_childLink->m_config.m_angularDamping = 0.0f;
            m_childLink->m_config.m_articulationJointType = jointType;
            m_childLink->m_config.m_motorConfiguration = motor;

            auto childData = AZStd::make_shared<ArticulationLinkData>();
            childData->m_articulationLinkConfiguration = m_childLink->m_config;
            childData->m_localTransform = transform.m_localTransform;
            childData->m_jointFollowerLocalFrame = AZ::Transform::CreateRotationY(AZ::Constants::HalfPi);
            childData->m_jointFollowerLocalFrame.SetTranslation(AZ::Vector3::CreateAxisY());
            childData->m_jointLeadLocalFrame = childData->m_localTransform * childData->m_jointFollowerLocalFrame;
            m_rootLink->m_articulationLinkData->m_childLinks.push_back(childData);

            m_root->Init();
            m_child->Init();
            m_root->Activate();
            if (m_child->GetState() != AZ::Entity::State::Active)
            {
                m_child->Activate();
            }
        }

        void CheckMotorEffortLimit(float timestep)
        {
            m_defaultScene->SetGravity(AZ::Vector3::CreateZero());
            ArticulationJointMotorProperties motor;
            motor.m_useMotor = true;
            motor.m_driveForceLimit = 1.0f;
            motor.m_driveStiffness = 0.0f;
            motor.m_driveDamping = 1000000.0f;
            CreateArticulation(ArticulationJointType::Prismatic, motor);
            m_childLink->SetDriveTargetVelocity(ArticulationJointAxis::X, 100.0f);
            TestUtils::UpdateScene(m_testSceneHandle, timestep, 1);
            EXPECT_NEAR(m_childLink->GetJointVelocity(ArticulationJointAxis::X), timestep / 2.0f, 1e-5f);
            m_childLink->SetMaxForce(ArticulationJointAxis::X, 2.0f);
            TestUtils::UpdateScene(m_testSceneHandle, timestep, 1);
            EXPECT_NEAR(m_childLink->GetJointVelocity(ArticulationJointAxis::X), 3.0f * timestep / 2.0f, 1e-5f);
        }

        physx::PxCookingParams m_cookingParams{ physx::PxTolerancesScale() };
        AZStd::unique_ptr<AZ::Entity> m_root;
        AZStd::unique_ptr<AZ::Entity> m_child;
        ArticulationLinkComponent* m_rootLink = nullptr;
        ArticulationLinkComponent* m_childLink = nullptr;
    };

    TEST_F(PhysXUpgradeTest, CookingParameters_AreMutableThroughTheBusAndUsedForMeshRoundTrips)
    {
        physx::PxCookingParams* params = nullptr;
        SystemRequestsBus::BroadcastResult(params, &SystemRequests::GetCookingParams);
        ASSERT_NE(params, nullptr);
        EXPECT_EQ(params, GetPhysXSystem()->GetPxCookingParams());
        params->meshWeldTolerance = 0.0125f;
        params->meshPreprocessParams = physx::PxMeshPreprocessingFlag::eWELD_VERTICES;
        params->suppressTriangleMeshRemapTable = true;

        EXPECT_FLOAT_EQ(GetPhysXSystem()->GetPxCookingParams()->meshWeldTolerance, 0.0125f);
        auto snapshot = *params;
        snapshot.meshWeldTolerance = 0.5f;
        EXPECT_FLOAT_EQ(params->meshWeldTolerance, 0.0125f);

        const auto [vertices, indices] = TestUtils::GenerateCubeMeshData(1.0f);
        physx::PxDefaultMemoryOutputStream convexStream;
        ASSERT_TRUE(Utils::CookConvexToPxOutputStream(vertices.data(), aznumeric_cast<AZ::u32>(vertices.size()), convexStream));
        physx::PxDefaultMemoryInputData convexInput(convexStream.getData(), convexStream.getSize());
        auto* convex = GetPhysXSystem()->GetPxPhysics()->createConvexMesh(convexInput);
        ASSERT_NE(convex, nullptr);
        EXPECT_EQ(convex->getNbVertices(), 8);
        convex->release();

        physx::PxDefaultMemoryOutputStream triangleStream;
        ASSERT_TRUE(
            Utils::CookTriangleMeshToToPxOutputStream(
                vertices.data(),
                aznumeric_cast<AZ::u32>(vertices.size()),
                indices.data(),
                aznumeric_cast<AZ::u32>(indices.size()),
                triangleStream));
        physx::PxDefaultMemoryInputData triangleInput(triangleStream.getData(), triangleStream.getSize());
        auto* triangle = GetPhysXSystem()->GetPxPhysics()->createTriangleMesh(triangleInput);
        ASSERT_NE(triangle, nullptr);
        EXPECT_EQ(triangle->getNbTriangles(), indices.size() / 3);
        EXPECT_EQ(triangle->getTrianglesRemap(), nullptr);
        triangle->release();
    }

    TEST_F(PhysXUpgradeTest, CookingParameters_WhenUnavailable_UseSdkDefaults)
    {
        auto* handler = static_cast<SystemRequestsBus::Handler*>(SystemRequestsBus::FindFirstHandler());
        ASSERT_NE(handler, nullptr);
        handler->BusDisconnect();

        const auto [vertices, indices] = TestUtils::GenerateCubeMeshData(1.0f);
        physx::PxDefaultMemoryOutputStream convexStream;
        EXPECT_TRUE(Utils::CookConvexToPxOutputStream(vertices.data(), aznumeric_cast<AZ::u32>(vertices.size()), convexStream));
        EXPECT_GT(convexStream.getSize(), 0);

        physx::PxDefaultMemoryOutputStream triangleStream;
        EXPECT_TRUE(
            Utils::CookTriangleMeshToToPxOutputStream(
                vertices.data(),
                aznumeric_cast<AZ::u32>(vertices.size()),
                indices.data(),
                aznumeric_cast<AZ::u32>(indices.size()),
                triangleStream));
        EXPECT_GT(triangleStream.getSize(), 0);

        handler->BusConnect();
    }

    TEST_F(PhysXUpgradeTest, InvalidConvexMesh_ReportsFailureWithoutProducingData)
    {
        const AZ::Vector3 vertices[] = { AZ::Vector3::CreateZero(), AZ::Vector3::CreateAxisX() };
        physx::PxDefaultMemoryOutputStream stream;
        UnitTest::ErrorHandler errors("");
        EXPECT_FALSE(Utils::CookConvexToPxOutputStream(vertices, 2, stream));
        EXPECT_EQ(errors.GetErrorCount(), 2);
        EXPECT_EQ(stream.getSize(), 0);
    }

    TEST_F(PhysXUpgradeTest, Heightfield_CooksAndLoadsWithTheNewSdk)
    {
        physx::PxHeightFieldSample samples[9]{};
        samples[1].height = 2;
        samples[4].height = 5;
        physx::PxHeightField* heightfield = nullptr;
        SystemRequestsBus::BroadcastResult(heightfield, &SystemRequests::CreateHeightField, samples, size_t{ 3 }, size_t{ 3 });
        ASSERT_NE(heightfield, nullptr);
        EXPECT_FLOAT_EQ(heightfield->getHeight(1.0f, 1.0f), 5.0f);
        heightfield->release();
    }

    TEST_F(PhysXUpgradeTest, IncomingJointForces_UseChildJointFrameAndSurviveReactivation)
    {
        CreateArticulation();
        EXPECT_TRUE(m_rootLink->GetIncomingJointForce().IsZero());
        TestUtils::UpdateScene(m_testSceneHandle, 1.0f / 60.0f, 2);
        const AZ::Vector3 expected =
            AZ::Transform::CreateRotationY(-AZ::Constants::HalfPi).TransformVector(AZ::Vector3(0.0f, 0.0f, 19.62f));
        auto force = m_childLink->GetIncomingJointForce();
        EXPECT_TRUE(force.IsClose(expected, 0.01f)) << force.GetX() << ", " << force.GetY() << ", " << force.GetZ();
        const AZ::Vector3 expectedTorque =
            AZ::Transform::CreateRotationY(-AZ::Constants::HalfPi).TransformVector(AZ::Vector3(-19.62f, 0.0f, 0.0f));
        const auto torque = m_childLink->GetIncomingJointTorque();
        EXPECT_TRUE(torque.IsClose(expectedTorque, 0.01f)) << torque.GetX() << ", " << torque.GetY() << ", " << torque.GetZ();
        EXPECT_TRUE(m_rootLink->GetIncomingJointForce().IsZero());
        EXPECT_TRUE(m_rootLink->GetIncomingJointTorque().IsZero());

        m_child->Deactivate();
        m_root->Deactivate();
        EXPECT_TRUE(m_childLink->GetIncomingJointForce().IsZero());
        m_root->Activate();
        if (m_child->GetState() != AZ::Entity::State::Active)
        {
            m_child->Activate();
        }
        TestUtils::UpdateScene(m_testSceneHandle, 1.0f / 60.0f, 2);
        force = m_childLink->GetIncomingJointForce();
        EXPECT_TRUE(force.IsClose(expected, 0.01f)) << force.GetX() << ", " << force.GetY() << ", " << force.GetZ();

        DestroyTestScene();
        EXPECT_TRUE(m_childLink->GetIncomingJointForce().IsZero());
        EXPECT_TRUE(m_childLink->GetIncomingJointTorque().IsZero());
    }

    TEST_F(PhysXUpgradeTest, RemovingAllScenes_ReleasesTheArticulationForceCache)
    {
        CreateArticulation();
        TestUtils::UpdateScene(m_testSceneHandle, 1.0f / 60.0f, 2);
        ASSERT_FALSE(m_childLink->GetIncomingJointForce().IsZero());
        GetPhysXSystem()->RemoveAllScenes();
        EXPECT_TRUE(m_childLink->GetIncomingJointForce().IsZero());
        EXPECT_TRUE(m_childLink->GetIncomingJointTorque().IsZero());
    }

    TEST_F(PhysXUpgradeTest, ArticulationParameters_AreIndependentPerAxis)
    {
        CreateArticulation(ArticulationJointType::Hinge);
        const ArticulationJointFrictionProperties friction{ 3.0f, 2.0f, 0.25f };
        m_childLink->SetFrictionParameters(ArticulationJointAxis::Twist, friction);
        const auto actual = m_childLink->GetFrictionParameters(ArticulationJointAxis::Twist);
        EXPECT_FLOAT_EQ(actual.m_staticFrictionEffort, 3.0f);
        EXPECT_FLOAT_EQ(actual.m_dynamicFrictionEffort, 2.0f);
        EXPECT_FLOAT_EQ(actual.m_viscousFrictionCoefficient, 0.25f);
        EXPECT_FLOAT_EQ(m_childLink->GetFrictionParameters(ArticulationJointAxis::SwingY).m_staticFrictionEffort, 0.0f);

        m_childLink->SetMaxJointVelocity(ArticulationJointAxis::Twist, 3.0f);
        m_childLink->SetMaxJointVelocity(ArticulationJointAxis::SwingY, 7.0f);
        EXPECT_FLOAT_EQ(m_childLink->GetMaxJointVelocity(ArticulationJointAxis::Twist), 3.0f);
        EXPECT_FLOAT_EQ(m_childLink->GetMaxJointVelocity(ArticulationJointAxis::SwingY), 7.0f);
        m_childLink->SetMaxForce(ArticulationJointAxis::Twist, 12.0f);
        EXPECT_FLOAT_EQ(m_childLink->GetMaxForce(ArticulationJointAxis::Twist), 12.0f);

        UnitTest::ErrorHandler errors("Articulation friction parameters");
        m_childLink->SetFrictionParameters(ArticulationJointAxis::Twist, { 1.0f, 2.0f, 0.0f });
        EXPECT_EQ(errors.GetExpectedErrorCount(), 1);
        EXPECT_FLOAT_EQ(m_childLink->GetFrictionParameters(ArticulationJointAxis::Twist).m_staticFrictionEffort, 3.0f);
    }

    TEST_F(PhysXUpgradeTest, FrictionParameters_RejectNonFiniteAndNegativeValues)
    {
        EXPECT_TRUE(ArticulationJointFrictionProperties{}.IsValid());
        ArticulationJointFrictionProperties friction;
        friction.m_staticFrictionEffort = std::numeric_limits<float>::infinity();
        EXPECT_FALSE(friction.IsValid());
        friction.m_staticFrictionEffort = 1.0f;
        friction.m_dynamicFrictionEffort = -1.0f;
        EXPECT_FALSE(friction.IsValid());
        friction.m_dynamicFrictionEffort = 0.5f;
        friction.m_viscousFrictionCoefficient = std::numeric_limits<float>::quiet_NaN();
        EXPECT_FALSE(friction.IsValid());
    }

    TEST_F(PhysXUpgradeTest, MotorEffortLimit_UsesForceUnitsAt60Hz)
    {
        CheckMotorEffortLimit(1.0f / 60.0f);
    }

    TEST_F(PhysXUpgradeTest, MotorEffortLimit_UsesForceUnitsAt120Hz)
    {
        CheckMotorEffortLimit(1.0f / 120.0f);
    }

    TEST_F(PhysXUpgradeTest, LegacyArticulationVersion1_LoadsAndRetainsRetuningData)
    {
        AZ::SerializeContext legacyContext;
        AZ::Entity::Reflect(&legacyContext);
        AzPhysics::SimulatedBodyConfiguration::Reflect(&legacyContext);
        ArticulationSensorConfiguration::Reflect(&legacyContext);
        // Freeze the pre-5.12 field layout instead of serializing with the current reflection.
        legacyContext.Class<ArticulationJointMotorProperties>()->Version(1)
            ->Field("UseMotor", &ArticulationJointMotorProperties::m_useMotor)
            ->Field("ForceLimit", &ArticulationJointMotorProperties::m_driveForceLimit)
            ->Field("Stiffness", &ArticulationJointMotorProperties::m_driveStiffness)
            ->Field("Damping", &ArticulationJointMotorProperties::m_driveDamping);
        legacyContext.Class<ArticulationLinkConfiguration, AzPhysics::SimulatedBodyConfiguration>()->Version(1)
            ->Field("Friction", &ArticulationLinkConfiguration::m_jointFriction)
            ->Field("Sensor Configuration", &ArticulationLinkConfiguration::m_sensorConfigs)
            ->Field("Motor configuration", &ArticulationLinkConfiguration::m_motorConfiguration);
        ArticulationLinkConfiguration original;
        original.m_jointFriction = 0.3f;
        original.m_sensorConfigs.emplace_back();
        original.m_sensorConfigs[0].m_localPosition = AZ::Vector3(1.0f, 2.0f, 3.0f);
        original.m_sensorConfigs[0].m_localRotation = AZ::Vector3(10.0f, 20.0f, 30.0f);
        original.m_sensorConfigs[0].m_includeForwardDynamicsForces = false;
        original.m_sensorConfigs[0].m_includeConstraintSolverForces = false;
        original.m_sensorConfigs[0].m_useWorldFrame = true;
        original.m_motorConfiguration.m_useMotor = true;
        original.m_motorConfiguration.m_driveForceLimit = 2.5f;
        AZStd::vector<AZ::u8> buffer;
        AZ::IO::ByteContainerStream stream(&buffer);
        ASSERT_TRUE(AZ::Utils::SaveObjectToStream(stream, AZ::DataStream::ST_XML, &original, &legacyContext));
        stream.Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);
        AZStd::unique_ptr<ArticulationLinkConfiguration> restored(AZ::Utils::LoadObjectFromStream<ArticulationLinkConfiguration>(stream));
        ASSERT_NE(restored, nullptr);
        EXPECT_FLOAT_EQ(restored->m_jointFriction, 0.3f);
        EXPECT_FLOAT_EQ(restored->m_jointFrictionParameters.m_staticFrictionEffort, 0.0f);
        EXPECT_FLOAT_EQ(restored->m_jointFrictionParameters.m_dynamicFrictionEffort, 0.0f);
        EXPECT_FLOAT_EQ(restored->m_jointFrictionParameters.m_viscousFrictionCoefficient, 0.0f);
        ASSERT_EQ(restored->m_sensorConfigs.size(), 1);
        EXPECT_TRUE(restored->m_sensorConfigs[0].m_localPosition.IsClose(original.m_sensorConfigs[0].m_localPosition));
        EXPECT_TRUE(restored->m_sensorConfigs[0].m_localRotation.IsClose(original.m_sensorConfigs[0].m_localRotation));
        EXPECT_FALSE(restored->m_sensorConfigs[0].m_includeForwardDynamicsForces);
        EXPECT_FALSE(restored->m_sensorConfigs[0].m_includeConstraintSolverForces);
        EXPECT_TRUE(restored->m_sensorConfigs[0].m_useWorldFrame);
        ASSERT_TRUE(restored->m_motorConfiguration.m_legacyDriveImpulseLimit.has_value());
        EXPECT_TRUE(restored->m_motorConfiguration.m_useMotor);
        EXPECT_FLOAT_EQ(*restored->m_motorConfiguration.m_legacyDriveImpulseLimit, 2.5f);
        EXPECT_FLOAT_EQ(restored->m_motorConfiguration.m_driveForceLimit, 2.5f);
        buffer.clear();
        stream.Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);
        ASSERT_TRUE(AZ::Utils::SaveObjectToStream(stream, AZ::DataStream::ST_XML, restored.get()));
        stream.Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);
        AZStd::unique_ptr<ArticulationLinkConfiguration> roundTrip(AZ::Utils::LoadObjectFromStream<ArticulationLinkConfiguration>(stream));
        ASSERT_NE(roundTrip, nullptr);
        EXPECT_TRUE(roundTrip->HasLegacySettings());
        ASSERT_TRUE(roundTrip->m_motorConfiguration.m_legacyDriveImpulseLimit.has_value());
        EXPECT_FLOAT_EQ(*roundTrip->m_motorConfiguration.m_legacyDriveImpulseLimit, 2.5f);
        EXPECT_FLOAT_EQ(roundTrip->m_jointFriction, 0.3f);
        EXPECT_EQ(roundTrip->m_sensorConfigs.size(), 1);
    }

    TEST_F(PhysXUpgradeTest, LegacyMotorJson_IsDetectedAndNewDefaultsRemainCurrent)
    {
        rapidjson::Document legacy;
        legacy.Parse(R"({"UseMotor":true,"ForceLimit":2.5,"Stiffness":3.0,"Damping":4.0})");
        ArticulationJointMotorProperties motor;
        auto result = AZ::JsonSerialization::Load(motor, legacy);
        ASSERT_EQ(result.GetProcessing(), AZ::JsonSerializationResult::Processing::Completed);
        ASSERT_TRUE(motor.m_legacyDriveImpulseLimit.has_value());
        EXPECT_FLOAT_EQ(*motor.m_legacyDriveImpulseLimit, 2.5f);
        EXPECT_TRUE(motor.m_useMotor);
        EXPECT_FLOAT_EQ(motor.m_driveStiffness, 3.0f);
        EXPECT_FLOAT_EQ(motor.m_driveDamping, 4.0f);
        motor.m_driveForceLimit = 150.0f;
        rapidjson::Document stored;
        result = AZ::JsonSerialization::Store(stored, stored.GetAllocator(), motor);
        ASSERT_EQ(result.GetProcessing(), AZ::JsonSerializationResult::Processing::Completed);
        ArticulationJointMotorProperties roundTrip;
        result = AZ::JsonSerialization::Load(roundTrip, stored);
        ASSERT_EQ(result.GetProcessing(), AZ::JsonSerializationResult::Processing::Completed);
        ASSERT_TRUE(roundTrip.m_legacyDriveImpulseLimit.has_value());
        EXPECT_FLOAT_EQ(*roundTrip.m_legacyDriveImpulseLimit, 2.5f);
        EXPECT_FLOAT_EQ(roundTrip.m_driveForceLimit, 150.0f);
        EXPECT_TRUE(roundTrip.m_useMotor);
        EXPECT_FLOAT_EQ(roundTrip.m_driveStiffness, 3.0f);
        EXPECT_FLOAT_EQ(roundTrip.m_driveDamping, 4.0f);
        ArticulationJointMotorProperties defaults;
        result = AZ::JsonSerialization::Store(stored, stored.GetAllocator(), defaults);
        ASSERT_EQ(result.GetProcessing(), AZ::JsonSerializationResult::Processing::Completed);
        ASSERT_TRUE(stored.HasMember("LegacyImpulseLimit"));
        EXPECT_TRUE(stored["LegacyImpulseLimit"].IsNull());
        result = AZ::JsonSerialization::Load(roundTrip, stored);
        ASSERT_EQ(result.GetProcessing(), AZ::JsonSerializationResult::Processing::Completed);
        EXPECT_FALSE(roundTrip.HasLegacyDriveImpulseLimit());

        ArticulationLinkConfiguration config;
        config.m_motorConfiguration.m_useMotor = true;
        result = AZ::JsonSerialization::Store(stored, stored.GetAllocator(), config);
        ASSERT_EQ(result.GetProcessing(), AZ::JsonSerializationResult::Processing::Completed);
        ASSERT_TRUE(stored.HasMember("Motor configuration"));
        ASSERT_TRUE(stored["Motor configuration"].HasMember("LegacyImpulseLimit"));
        EXPECT_TRUE(stored["Motor configuration"]["LegacyImpulseLimit"].IsNull());
        ArticulationLinkConfiguration restored;
        result = AZ::JsonSerialization::Load(restored, stored);
        ASSERT_EQ(result.GetProcessing(), AZ::JsonSerializationResult::Processing::Completed);
        EXPECT_TRUE(restored.m_motorConfiguration.m_useMotor);
        EXPECT_FALSE(restored.m_motorConfiguration.HasLegacyDriveImpulseLimit());
    }

    TEST_F(PhysXUpgradeTest, LegacyPrefabMotorOverrideRemainsMarkedAfterParentRetuning)
    {
        rapidjson::Document legacyParent;
        legacyParent.Parse(R"({"Motor configuration":{"UseMotor":true,"ForceLimit":2.5}})");
        rapidjson::Document overridePatch;
        overridePatch.Parse(R"([{"op":"replace","path":"/Motor configuration/ForceLimit","value":7.0}])");

        ArticulationLinkConfiguration parent;
        ASSERT_EQ(AZ::JsonSerialization::Load(parent, legacyParent).GetProcessing(), AZ::JsonSerializationResult::Processing::Completed);
        parent.m_motorConfiguration.m_driveForceLimit = 150.0f;
        parent.ClearLegacySettings();

        rapidjson::Document savedParent;
        ASSERT_EQ(
            AZ::JsonSerialization::Store(savedParent, savedParent.GetAllocator(), parent).GetProcessing(),
            AZ::JsonSerializationResult::Processing::Completed);
        ASSERT_EQ(
            AZ::JsonSerialization::ApplyPatch(savedParent, savedParent.GetAllocator(), overridePatch, AZ::JsonMergeApproach::JsonPatch).GetProcessing(),
            AZ::JsonSerializationResult::Processing::Completed);

        ArticulationLinkConfiguration variant;
        ASSERT_EQ(AZ::JsonSerialization::Load(variant, savedParent).GetProcessing(), AZ::JsonSerializationResult::Processing::Completed);
        EXPECT_FLOAT_EQ(variant.m_motorConfiguration.m_driveForceLimit, 150.0f);
        ASSERT_TRUE(variant.m_motorConfiguration.m_legacyDriveImpulseLimit.has_value());
        EXPECT_FLOAT_EQ(*variant.m_motorConfiguration.m_legacyDriveImpulseLimit, 7.0f);
    }

    TEST_F(PhysXUpgradeTest, MotorJson_PreservesZeroAndRejectsInvalidLegacyValues)
    {
        ArticulationJointMotorProperties motor;
        motor.m_legacyDriveImpulseLimit = 0.0f;
        rapidjson::Document stored;
        auto result = AZ::JsonSerialization::Store(stored, stored.GetAllocator(), motor);
        ASSERT_EQ(result.GetProcessing(), AZ::JsonSerializationResult::Processing::Completed);
        ArticulationJointMotorProperties restored;
        result = AZ::JsonSerialization::Load(restored, stored);
        ASSERT_EQ(result.GetProcessing(), AZ::JsonSerializationResult::Processing::Completed);
        ASSERT_TRUE(restored.m_legacyDriveImpulseLimit.has_value());
        EXPECT_FLOAT_EQ(*restored.m_legacyDriveImpulseLimit, 0.0f);

        rapidjson::Document invalid;
        invalid.Parse(R"({"LegacyImpulseLimit":[]})");
        AZ::JsonDeserializerSettings settings;
        settings.m_reporting = [](AZStd::string_view, AZ::JsonSerializationResult::ResultCode code, AZStd::string_view)
        {
            return code;
        };
        ArticulationJointMotorProperties rejected;
        result = AZ::JsonSerialization::Load(rejected, invalid, settings);
        EXPECT_NE(result.GetProcessing(), AZ::JsonSerializationResult::Processing::Completed);
        EXPECT_FALSE(rejected.HasLegacyDriveImpulseLimit());
        invalid.Parse(R"({"ForceLimit":[]})");
        result = AZ::JsonSerialization::Load(rejected, invalid, settings);
        EXPECT_NE(result.GetProcessing(), AZ::JsonSerializationResult::Processing::Completed);
        EXPECT_FALSE(rejected.HasLegacyDriveImpulseLimit());
    }

    TEST_F(PhysXUpgradeTest, ClearingLegacySettings_PreservesCurrentSdkParameters)
    {
        ArticulationLinkConfiguration config;
        config.m_jointFriction = 0.3f;
        config.m_sensorConfigs.emplace_back();
        config.m_motorConfiguration.m_legacyDriveImpulseLimit = 2.5f;
        config.m_motorConfiguration.m_driveForceLimit = 150.0f;
        config.m_jointFrictionParameters = { 4.0f, 2.0f, 0.5f };
        ASSERT_TRUE(config.HasLegacySettings());
        config.ClearLegacySettings();
        EXPECT_FALSE(config.HasLegacySettings());
        EXPECT_FLOAT_EQ(config.m_motorConfiguration.m_driveForceLimit, 150.0f);
        EXPECT_FLOAT_EQ(config.m_jointFrictionParameters.m_staticFrictionEffort, 4.0f);
        EXPECT_FLOAT_EQ(config.m_jointFrictionParameters.m_dynamicFrictionEffort, 2.0f);
        EXPECT_FLOAT_EQ(config.m_jointFrictionParameters.m_viscousFrictionCoefficient, 0.5f);
    }

    TEST_F(PhysXUpgradeTest, LegacyArticulationSettings_RoundTripWithoutChangingUnits)
    {
        AZ::SerializeContext* context = nullptr;
        AZ::ComponentApplicationBus::BroadcastResult(context, &AZ::ComponentApplicationRequests::GetSerializeContext);
        ASSERT_NE(context, nullptr);
        ArticulationLinkConfiguration original;
        original.m_jointFriction = 0.3f;
        original.m_sensorConfigs.emplace_back();
        original.m_sensorConfigs[0].m_localPosition = AZ::Vector3(1.0f, 2.0f, 3.0f);
        original.m_sensorConfigs[0].m_includeForwardDynamicsForces = false;
        original.m_jointFrictionParameters = { 4.0f, 2.0f, 0.5f };
        AZStd::vector<AZ::u8> buffer;
        AZ::IO::ByteContainerStream stream(&buffer);
        ASSERT_TRUE(AZ::Utils::SaveObjectToStream(stream, AZ::DataStream::ST_XML, &original, context));
        stream.Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);
        AZStd::unique_ptr<ArticulationLinkConfiguration> restored(
            AZ::Utils::LoadObjectFromStream<ArticulationLinkConfiguration>(stream, context));
        ASSERT_NE(restored, nullptr);
        EXPECT_FLOAT_EQ(restored->m_jointFriction, 0.3f);
        ASSERT_EQ(restored->m_sensorConfigs.size(), 1);
        EXPECT_TRUE(restored->m_sensorConfigs[0].m_localPosition.IsClose(AZ::Vector3(1.0f, 2.0f, 3.0f)));
        EXPECT_FALSE(restored->m_sensorConfigs[0].m_includeForwardDynamicsForces);
        EXPECT_FLOAT_EQ(restored->m_jointFrictionParameters.m_staticFrictionEffort, 4.0f);
        EXPECT_FLOAT_EQ(restored->m_jointFrictionParameters.m_dynamicFrictionEffort, 2.0f);
        EXPECT_FLOAT_EQ(restored->m_jointFrictionParameters.m_viscousFrictionCoefficient, 0.5f);
    }
} // namespace PhysX
