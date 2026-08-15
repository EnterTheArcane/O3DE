/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/Editor/SceneBuilder.h>
#include <Jolt/Editor/SkeletonBuilder.h>
#include <Jolt/NativeRuntime.h>
#include <Jolt/SceneAsset.h>
#include <Jolt/SkeletonAsset.h>
#include <Jolt/System.h>
#include <Jolt/SystemComponent.h>

#include <AzTest/AzTest.h>
#include <AzTest/Utils.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/Name/Name.h>
#include <AzCore/Name/NameDictionary.h>
#include <AzCore/Serialization/Json/JsonSystemComponent.h>
#include <AzCore/Serialization/Json/JsonSerializationSettings.h>
#include <AzCore/Serialization/Json/JsonUtils.h>
#include <AzCore/Serialization/Json/RegistrationContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/Utils.h>
#include <AzCore/UnitTest/MockComponentApplication.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/vector.h>
#include <AzFramework/IO/LocalFileIO.h>
#include <AzToolsFramework/UnitTest/AzToolsFrameworkTestHelpers.h>

AZ_TOOLS_UNIT_TEST_HOOK(DEFAULT_UNIT_TEST_ENV);

namespace Jolt::Editor
{
    namespace
    {
        class NameDictionaryScope final
        {
        public:
            NameDictionaryScope()
            {
                if (!AZ::Interface<AZ::NameDictionary>::Get())
                {
                    AZ::NameDictionary::Create();
                    m_created = true;
                }
            }

            ~NameDictionaryScope()
            {
                if (m_created)
                {
                    AZ::NameDictionary::Destroy();
                }
            }

            AZ_DISABLE_COPY_MOVE(NameDictionaryScope);

        private:
            bool m_created = false;
        };

        class FileIoScope final
        {
        public:
            FileIoScope()
            {
                AZ_Assert(!AZ::IO::FileIOBase::GetInstance(), "The test FileIO instance is already registered.");
                AZ::IO::FileIOBase::SetInstance(&m_fileIo);
            }

            ~FileIoScope()
            {
                AZ::IO::FileIOBase::SetInstance(nullptr);
            }

            AZ_DISABLE_COPY_MOVE(FileIoScope);

        private:
            AZ::IO::LocalFileIO m_fileIo;
        };

        class JsonRegistrationScope final
        {
        public:
            JsonRegistrationScope()
            {
                AZ::JsonSystemComponent::Reflect(&m_context);
                AZ::Name::Reflect(&m_context);
                SystemComponent::Reflect(&m_context);
            }

            ~JsonRegistrationScope()
            {
                m_context.EnableRemoveReflection();
                SystemComponent::Reflect(&m_context);
                AZ::Name::Reflect(&m_context);
                AZ::JsonSystemComponent::Reflect(&m_context);
                m_context.DisableRemoveReflection();
            }

            AZ_DISABLE_COPY_MOVE(JsonRegistrationScope);

            [[nodiscard]]
            AZ::JsonRegistrationContext* Get()
            {
                return &m_context;
            }

        private:
            AZ::JsonRegistrationContext m_context;
        };

        class SerializeContextScope final
        {
        public:
            SerializeContextScope()
            {
                AZ::Data::AssetData::Reflect(&m_context);
                AZ::Entity::Reflect(&m_context);
                AZ::Name::Reflect(&m_context);
                SystemComponent::Reflect(&m_context);
            }

            ~SerializeContextScope()
            {
                m_context.EnableRemoveReflection();
                SystemComponent::Reflect(&m_context);
                AZ::Name::Reflect(&m_context);
                AZ::Entity::Reflect(&m_context);
                AZ::Data::AssetData::Reflect(&m_context);
                m_context.DisableRemoveReflection();
                AZ::GetGlobalSerializeContextModule().Cleanup();
            }

            AZ_DISABLE_COPY_MOVE(SerializeContextScope);

            [[nodiscard]]
            AZ::SerializeContext* Get()
            {
                return &m_context;
            }

        private:
            AZ::SerializeContext m_context;
        };

        class SystemComponentScope final
        {
        public:
            SystemComponentScope()
            {
                m_component.Activate();
            }

            ~SystemComponentScope()
            {
                m_component.Deactivate();
            }

            AZ_DISABLE_COPY_MOVE(SystemComponentScope);

        private:
            SystemComponent m_component;
        };

        class BuilderRegistrationCapture final
            : public AssetBuilderSDK::AssetBuilderBus::Handler
        {
        public:
            BuilderRegistrationCapture()
            {
                BusConnect();
            }

            ~BuilderRegistrationCapture() override
            {
                BusDisconnect();
            }

            AZ_DISABLE_COPY_MOVE(BuilderRegistrationCapture);

            void RegisterBuilderInformation(
                const AssetBuilderSDK::AssetBuilderDesc& descriptor) override
            {
                m_descriptors.push_back(descriptor);
            }

            AZStd::vector<AssetBuilderSDK::AssetBuilderDesc> m_descriptors;
        };
    } // namespace

    TEST(EditorSceneBuilderTests, RegistrationFingerprintsNativeBuildIdentity)
    {
        NameDictionaryScope nameDictionaryScope;
        SystemComponentScope systemComponentScope;
        BuilderRegistrationCapture registrationCapture;

        SceneBuilder sceneBuilder;
        sceneBuilder.Register();
        sceneBuilder.BusDisconnect();

        SkeletonBuilder skeletonBuilder;
        skeletonBuilder.Register();
        skeletonBuilder.BusDisconnect();

        ASSERT_EQ(registrationCapture.m_descriptors.size(), 2);
        const AZStd::string fingerprint = AZStd::string::format(
            "%016llx",
            static_cast<unsigned long long>(GetNativeBuildFingerprint()));
        EXPECT_EQ(
            registrationCapture.m_descriptors[0].m_analysisFingerprint,
            AZStd::string::format("JoltScene:%s", fingerprint.c_str()));
        EXPECT_EQ(registrationCapture.m_descriptors[0].m_version, 2);

        EXPECT_EQ(
            registrationCapture.m_descriptors[1].m_analysisFingerprint,
            AZStd::string::format("JoltSkeleton:%s", fingerprint.c_str()));
        EXPECT_EQ(registrationCapture.m_descriptors[1].m_version, 2);
    }

    TEST(EditorSceneBuilderTests, CreatesOneCriticalJobPerPlatformAndHonorsShutdown)
    {
        SceneBuilder builder;
        AssetBuilderSDK::CreateJobsRequest request;
        request.m_enabledPlatforms.emplace_back("pc", AZStd::unordered_set<AZStd::string>{});
        request.m_enabledPlatforms.emplace_back("linux", AZStd::unordered_set<AZStd::string>{});

        AssetBuilderSDK::CreateJobsResponse response;
        builder.CreateJobs(request, response);
        EXPECT_EQ(response.m_result, AssetBuilderSDK::CreateJobsResultCode::Success);
        ASSERT_EQ(response.m_createJobOutputs.size(), 2);
        EXPECT_TRUE(response.m_createJobOutputs[0].m_critical);
        EXPECT_TRUE(response.m_createJobOutputs[1].m_critical);
        EXPECT_EQ(response.m_createJobOutputs[0].m_jobKey, "Jolt Scene");

        builder.ShutDown();
        AssetBuilderSDK::CreateJobsResponse shutdownResponse;
        builder.CreateJobs(request, shutdownResponse);
        EXPECT_EQ(shutdownResponse.m_result, AssetBuilderSDK::CreateJobsResultCode::ShuttingDown);
        EXPECT_TRUE(shutdownResponse.m_createJobOutputs.empty());

        SkeletonBuilder skeletonBuilder;
        AssetBuilderSDK::CreateJobsResponse skeletonResponse;
        skeletonBuilder.CreateJobs(request, skeletonResponse);
        EXPECT_EQ(skeletonResponse.m_result, AssetBuilderSDK::CreateJobsResultCode::Success);
        ASSERT_EQ(skeletonResponse.m_createJobOutputs.size(), 2);
        EXPECT_TRUE(skeletonResponse.m_createJobOutputs[0].m_critical);
        EXPECT_TRUE(skeletonResponse.m_createJobOutputs[1].m_critical);
        EXPECT_EQ(skeletonResponse.m_createJobOutputs[0].m_jobKey, "Jolt Skeleton");

        skeletonBuilder.ShutDown();
        AssetBuilderSDK::CreateJobsResponse skeletonShutdownResponse;
        skeletonBuilder.CreateJobs(request, skeletonShutdownResponse);
        EXPECT_EQ(skeletonShutdownResponse.m_result, AssetBuilderSDK::CreateJobsResultCode::ShuttingDown);
        EXPECT_TRUE(skeletonShutdownResponse.m_createJobOutputs.empty());
    }

    TEST(EditorSceneBuilderTests, ProcessesJsonSourceIntoLoadableBinaryAsset)
    {
        NameDictionaryScope nameDictionaryScope;
        FileIoScope fileIoScope;
        SerializeContextScope serializeContextScope;
        JsonRegistrationScope jsonRegistrationScope;
        ::testing::NiceMock<UnitTest::MockComponentApplication> application;
        ON_CALL(application, GetSerializeContext())
            .WillByDefault(::testing::Return(serializeContextScope.Get()));
        ON_CALL(application, GetJsonRegistrationContext())
            .WillByDefault(::testing::Return(jsonRegistrationScope.Get()));

        SceneSourceData sourceData;
        sourceData.m_name = AZ::Name("BuilderScene");
        sourceData.m_shapes.push_back(SceneSourceShapeData{
            .m_geometry = BoxShapeConfiguration{},
        });
        sourceData.m_bodies.emplace_back(SceneAssetRigidBody{
            .m_name = AZ::Name("Floor"),
            .m_objectLayer = DefaultLayers::NonMoving,
            .m_motionType = MotionType::Static,
            .m_shapeIndex = 0,
        });
        sourceData.m_bodies.emplace_back(SceneAssetRigidBody{
            .m_transform = {.m_position = {.m_z = 2.0}},
            .m_name = AZ::Name("Box"),
            .m_shapeIndex = 0,
        });
        sourceData.m_constraints.push_back(SceneAssetConstraint{
            .m_geometry = DistanceConstraintConfiguration{},
            .m_name = AZ::Name("Distance"),
            .m_firstBodyIndex = 0,
            .m_secondBodyIndex = 1,
        });

        AZ::Test::ScopedAutoTempDirectory temporaryDirectory;
        const AZStd::optional<AZ::IO::FixedMaxPath> sourcePath = AZ::Test::CreateTestFile(
            temporaryDirectory,
            "source/test.joltscene",
            "{}");
        ASSERT_TRUE(sourcePath);
        AZStd::string serializationIssues;
        AZ::JsonSerializerSettings serializationSettings;
        serializationSettings.m_serializeContext = serializeContextScope.Get();
        serializationSettings.m_registrationContext = jsonRegistrationScope.Get();
        serializationSettings.m_reporting =
            [&serializationIssues](
                const AZStd::string_view message,
                const AZ::JsonSerializationResult::ResultCode result,
                const AZStd::string_view path)
            {
                if (result.GetOutcome() >= AZ::JsonSerializationResult::Outcomes::Unsupported)
                {
                    serializationIssues += AZStd::string::format(
                        "%.*s: %.*s\n",
                        AZ_STRING_ARG(path),
                        AZ_STRING_ARG(message));
                }
                return result;
            };
        const AZ::Outcome<void, AZStd::string> saveResult = AZ::JsonSerializationUtils::SaveObjectToFile(
            &sourceData,
            sourcePath->String(),
            static_cast<const SceneSourceData*>(nullptr),
            &serializationSettings);
        ASSERT_TRUE(saveResult.IsSuccess()) << serializationIssues.c_str() << saveResult.GetError().c_str();

        SystemComponentScope systemComponentScope;
        ASSERT_TRUE(AZ::Interface<ISystem>::Get());

        SceneBuilder builder;
        AssetBuilderSDK::ProcessJobRequest request;
        request.m_fullPath = sourcePath->String();
        request.m_sourceFile = "test.joltscene";
        request.m_tempDirPath = temporaryDirectory.GetDirectory();
        AssetBuilderSDK::ProcessJobResponse response;
        builder.ProcessJob(request, response);
        ASSERT_EQ(response.m_resultCode, AssetBuilderSDK::ProcessJobResult_Success);
        ASSERT_EQ(response.m_outputProducts.size(), 1);
        EXPECT_EQ(response.m_outputProducts.front().m_productAssetType, SceneAssetTypeId);

        SceneAsset loadedAsset;
        EXPECT_TRUE(AZ::Utils::LoadObjectFromFileInPlace(
            response.m_outputProducts.front().m_productFileName,
            loadedAsset,
            serializeContextScope.Get()));
        EXPECT_EQ(loadedAsset.m_data.m_name, sourceData.m_name);
        ASSERT_EQ(loadedAsset.m_data.m_shapes.size(), 1);
        EXPECT_FALSE(loadedAsset.m_data.m_shapes[0].m_archive.m_binaryState.empty());
        EXPECT_NE(loadedAsset.m_data.m_shapes[0].m_archive.m_buildFingerprint, 0);
        EXPECT_EQ(loadedAsset.m_data.m_bodies.size(), 2);
        EXPECT_EQ(loadedAsset.m_data.m_constraints.size(), 1);

        ISystem* system = AZ::Interface<ISystem>::Get();
        ASSERT_TRUE(system);
        const SceneDefinitionHandle definitionHandle = system->CreateSceneDefinition(loadedAsset.m_data);
        ASSERT_TRUE(definitionHandle);
        const WorldHandle worldHandle = system->GetDefaultWorldHandle();
        const SceneInstanceHandle instanceHandle = system->InstantiateScene(
            worldHandle,
            definitionHandle);
        ASSERT_TRUE(instanceHandle);
        AZStd::array<BodyHandle, 2> bodies;
        AZStd::array<ConstraintHandle, 1> constraints;
        EXPECT_TRUE(system->GetSceneBodies(worldHandle, instanceHandle, bodies).IsComplete());
        EXPECT_TRUE(system->GetSceneConstraints(worldHandle, instanceHandle, constraints).IsComplete());
        EXPECT_TRUE(bodies[0]);
        EXPECT_TRUE(bodies[1]);
        EXPECT_TRUE(constraints[0]);
        EXPECT_TRUE(system->DestroySceneInstance(worldHandle, instanceHandle));
        EXPECT_TRUE(system->DestroySceneDefinition(definitionHandle));
    }

    TEST(EditorSceneBuilderTests, ProcessesSkeletonAndAnimationsIntoValidatedNativeArchives)
    {
        NameDictionaryScope nameDictionaryScope;
        FileIoScope fileIoScope;
        SerializeContextScope serializeContextScope;
        JsonRegistrationScope jsonRegistrationScope;
        ::testing::NiceMock<UnitTest::MockComponentApplication> application;
        ON_CALL(application, GetSerializeContext())
            .WillByDefault(::testing::Return(serializeContextScope.Get()));
        ON_CALL(application, GetJsonRegistrationContext())
            .WillByDefault(::testing::Return(jsonRegistrationScope.Get()));

        SkeletonSourceData sourceData;
        sourceData.m_name = "Humanoid";
        sourceData.m_skeleton.m_joints = {
            {
                .m_name = "root",
                .m_parentIndex = -1,
            },
            {
                .m_name = "child",
                .m_parentIndex = 0,
            },
        };
        NamedSkeletalAnimationSource& sourceAnimation = sourceData.m_animations.emplace_back();
        sourceAnimation.m_name = "Walk";
        SkeletalAnimatedJointSource& animatedJoint = sourceAnimation.m_configuration.m_joints.emplace_back();
        animatedJoint.m_name = "child";
        animatedJoint.m_keyframes = {
            {
                .m_rotation = AZ::Quaternion::CreateIdentity(),
                .m_translation = AZ::Vector3::CreateZero(),
                .m_time = 0.0f,
            },
            {
                .m_rotation = AZ::Quaternion::CreateIdentity(),
                .m_translation = AZ::Vector3::CreateAxisX(),
                .m_time = 1.0f,
            },
        };

        AZ::Test::ScopedAutoTempDirectory temporaryDirectory;
        const AZStd::optional<AZ::IO::FixedMaxPath> sourcePath = AZ::Test::CreateTestFile(
            temporaryDirectory,
            "source/test.joltskeleton",
            "{}");
        ASSERT_TRUE(sourcePath);
        const AZ::Outcome<void, AZStd::string> saveResult = AZ::JsonSerializationUtils::SaveObjectToFile(
            &sourceData,
            sourcePath->String());
        ASSERT_TRUE(saveResult.IsSuccess()) << saveResult.GetError().c_str();
        SkeletonSourceData loadedSourceData;
        const AZ::Outcome<void, AZStd::string> loadResult = AZ::JsonSerializationUtils::LoadObjectFromFile(
            loadedSourceData,
            sourcePath->String());
        ASSERT_TRUE(loadResult.IsSuccess()) << loadResult.GetError().c_str();
        ASSERT_EQ(loadedSourceData.m_skeleton.m_joints.size(), 2);
        ASSERT_EQ(loadedSourceData.m_animations.size(), 1);
        EXPECT_FALSE(loadedSourceData.m_skeleton.m_joints[0].m_name.empty());
        EXPECT_FALSE(loadedSourceData.m_skeleton.m_joints[1].m_name.empty());
        EXPECT_EQ(loadedSourceData.m_skeleton.m_joints[0].m_parentIndex, -1);
        EXPECT_EQ(loadedSourceData.m_skeleton.m_joints[1].m_parentIndex, 0);

        AssetBuilderSDK::ProcessJobRequest request;
        request.m_fullPath = sourcePath->String();
        request.m_sourceFile = "test.joltskeleton";
        request.m_tempDirPath = temporaryDirectory.GetDirectory();
        AssetBuilderSDK::ProcessJobResponse response;
        SkeletonBuilder builder;
        EXPECT_EQ(AZ::Interface<ISystem>::Get(), nullptr);
        builder.Register();
        EXPECT_EQ(AZ::Interface<ISystem>::Get(), nullptr);
        builder.ProcessJob(request, response);
        ASSERT_EQ(response.m_resultCode, AssetBuilderSDK::ProcessJobResult_Success);
        ASSERT_EQ(response.m_outputProducts.size(), 1);
        EXPECT_EQ(response.m_outputProducts.front().m_productAssetType, SkeletonAssetTypeId);

        SkeletonAsset loadedAsset;
        ASSERT_TRUE(AZ::Utils::LoadObjectFromFileInPlace(
            response.m_outputProducts.front().m_productFileName,
            loadedAsset,
            serializeContextScope.Get()));
        EXPECT_EQ(loadedAsset.m_data.m_name.GetStringView(), sourceData.m_name);
        ASSERT_EQ(loadedAsset.m_data.m_animations.size(), 1);
        EXPECT_EQ(loadedAsset.m_data.m_animations.front().m_name.GetStringView(), sourceAnimation.m_name);

        SystemComponentScope systemComponentScope;
        ISystem* system = AZ::Interface<ISystem>::Get();
        ASSERT_NE(system, nullptr);
        const SkeletonDefinitionHandle skeletonHandle = system->ImportSkeletonDefinition(loadedAsset.m_data.m_skeleton);
        ASSERT_TRUE(skeletonHandle);
        const SkeletalAnimationHandle animationHandle =
            system->ImportSkeletalAnimation(loadedAsset.m_data.m_animations.front().m_archive);
        ASSERT_TRUE(animationHandle);

        SkeletonJoint joints[2];
        const QueryResult jointResult = system->GetSkeletonJoints(skeletonHandle, joints);
        EXPECT_EQ(jointResult.m_hitCount, 2);
        EXPECT_EQ(joints[1].m_name.GetStringView(), "child");

        SkeletalAnimationState animationState;
        ASSERT_TRUE(system->GetSkeletalAnimationState(animationHandle, animationState));
        EXPECT_EQ(animationState.m_jointCount, 1);
        EXPECT_FLOAT_EQ(animationState.m_duration, 1.0f);

        EXPECT_TRUE(system->DestroySkeletalAnimation(animationHandle));
        EXPECT_TRUE(system->DestroySkeletonDefinition(skeletonHandle));
        builder.ShutDown();
        builder.BusDisconnect();
    }
} // namespace Jolt::Editor
