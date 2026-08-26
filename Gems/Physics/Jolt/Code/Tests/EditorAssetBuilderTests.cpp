/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/Editor/AssetBuilder.h>
#include <Jolt/Editor/SourceDependencyAnalyzer.h>
#include <Jolt/AssetProduct.h>
#include <Jolt/Capabilities.h>
#include <Jolt/CustomShapeDependencyUtils.h>
#include <Jolt/NativeRuntime.h>
#include <Jolt/SceneAsset.h>
#include <Jolt/SkeletonAsset.h>
#include <Jolt/SoftBodyComponentConfiguration.h>
#include <Jolt/System.h>
#include <Jolt/SystemComponent.h>

#include <AzTest/AzTest.h>
#include <AzTest/Utils.h>

#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/IO/ByteContainerStream.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/Name/Name.h>
#include <AzCore/Name/NameDictionary.h>
#include <AzCore/Serialization/Json/JsonSystemComponent.h>
#include <AzCore/Serialization/Json/JsonSerializationSettings.h>
#include <AzCore/Serialization/Json/JsonUtils.h>
#include <AzCore/Serialization/Json/RegistrationContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/UnitTest/MockComponentApplication.h>
#include <AzCore/Utils/TypeHash.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/vector.h>
#include <AzFramework/IO/LocalFileIO.h>
#include <AzToolsFramework/UnitTest/AzToolsFrameworkTestHelpers.h>

namespace Jolt::Editor
{
    TEST(EditorAssetBuilderTests, BuilderComponentActivatesAfterRuntimeServiceWhenPresent)
    {
        AZ::ComponentDescriptor::DependencyArrayType dependentServices;
        BuilderComponent::GetDependentServices(dependentServices);

        ASSERT_EQ(dependentServices.size(), 1);
        EXPECT_EQ(dependentServices[0], AZ_CRC_CE("JoltService"));
    }

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

        class BuilderCustomShapeProvider final
            : public ICustomShapeProvider
        {
        public:
            [[nodiscard]]
            AZ::TypeId GetId() const override
            {
                return AZ::TypeId("{83086C50-834C-4789-AD1F-8AC45C1E70E1}");
            }

            [[nodiscard]]
            AZ::u64 GetVersion() const override
            {
                return m_version;
            }

            [[nodiscard]]
            bool Cook(
                const AZStd::span<const AZ::u8> input,
                CustomShapeData& output) const override
            {
                if (input.size() != m_expectedInput.size()
                    || input[0] != m_expectedInput[0]
                    || input[1] != m_expectedInput[1])
                {
                    return false;
                }

                output.m_vertices = {
                    {-1.0f, -1.0f, -1.0f},
                    {1.0f, -1.0f, -1.0f},
                    {-1.0f, 1.0f, -1.0f},
                    {1.0f, 1.0f, -1.0f},
                    {-1.0f, -1.0f, 1.0f},
                    {1.0f, -1.0f, 1.0f},
                    {-1.0f, 1.0f, 1.0f},
                    {1.0f, 1.0f, 1.0f},
                };
                output.m_dependencies = {
                    {"Objects/Jolt/./BuilderCustomShape.source", m_dependencyContentHash},
                    {"Objects/Jolt/BuilderCustomShape.source", m_dependencyContentHash},
                };
                output.m_geometryKind = CustomShapeGeometryKind::Convex;
                output.m_runtimeData.assign(input.begin(), input.end());
                return true;
            }

            AZ::u64 m_dependencyContentHash = 0x0123456789abcdef;
            AZ::u64 m_version = 1;

        private:
            inline static constexpr AZStd::array<AZ::u8, 2> m_expectedInput = {4, 2};
        };
    } // namespace

    TEST(EditorAssetBuilderTests, RegistrationFingerprintsNativeBuildIdentity)
    {
        NameDictionaryScope nameDictionaryScope;
        SystemComponentScope systemComponentScope;
        BuilderRegistrationCapture registrationCapture;

        AssetBuilder builder;
        builder.Register();
        builder.BusDisconnect();

        ASSERT_EQ(registrationCapture.m_descriptors.size(), 1);
        const AZStd::string fingerprint = AZStd::string::format(
            "%016llx",
            static_cast<unsigned long long>(GetNativeBuildFingerprint()));
        EXPECT_EQ(
            registrationCapture.m_descriptors[0].m_analysisFingerprint,
            AZStd::string::format("JoltAssets:Portable3:%s", fingerprint.c_str()));
        EXPECT_EQ(registrationCapture.m_descriptors[0].m_version, 9);
        EXPECT_EQ(
            registrationCapture.m_descriptors[0].m_flags,
            AssetBuilderSDK::AssetBuilderDesc::BF_None);
        ASSERT_EQ(registrationCapture.m_descriptors[0].m_patterns.size(), 1);
        EXPECT_EQ(registrationCapture.m_descriptors[0].m_patterns[0].m_pattern, "*.jolt.json");
    }

    TEST(EditorAssetBuilderTests, CreatesOneCriticalJobPerPlatformAndHonorsShutdown)
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

        AZ::Test::ScopedAutoTempDirectory temporaryDirectory;
        const AZStd::optional<AZ::IO::FixedMaxPath> scenePath = AZ::Test::CreateTestFile(
            temporaryDirectory,
            "scene.jolt.json",
            R"({"Type":"JsonSerialization","Version":1,"ClassName":"SceneSourceData","ClassData":{}})");
        ASSERT_TRUE(scenePath);
        const AZStd::optional<AZ::IO::FixedMaxPath> skeletonPath = AZ::Test::CreateTestFile(
            temporaryDirectory,
            "skeleton.jolt.json",
            R"({"Type":"JsonSerialization","Version":1,"ClassName":"SkeletonSourceData","ClassData":{}})");
        ASSERT_TRUE(skeletonPath);

        AssetBuilder builder;
        AssetBuilderSDK::CreateJobsRequest request;
        request.m_watchFolder = temporaryDirectory.GetDirectory();
        request.m_sourceFile = "scene.jolt.json";
        request.m_enabledPlatforms.emplace_back("pc", AZStd::unordered_set<AZStd::string>{});
        request.m_enabledPlatforms.emplace_back("linux", AZStd::unordered_set<AZStd::string>{});

        AssetBuilderSDK::CreateJobsResponse response;
        builder.CreateJobs(request, response);
        EXPECT_EQ(response.m_result, AssetBuilderSDK::CreateJobsResultCode::Success);
        ASSERT_EQ(response.m_createJobOutputs.size(), 2);
        EXPECT_TRUE(response.m_createJobOutputs[0].m_critical);
        EXPECT_TRUE(response.m_createJobOutputs[1].m_critical);
        EXPECT_EQ(response.m_createJobOutputs[0].m_jobKey, "Jolt Scene");

        request.m_sourceFile = "skeleton.jolt.json";
        AssetBuilderSDK::CreateJobsResponse skeletonResponse;
        builder.CreateJobs(request, skeletonResponse);
        EXPECT_EQ(skeletonResponse.m_result, AssetBuilderSDK::CreateJobsResultCode::Success);
        ASSERT_EQ(skeletonResponse.m_createJobOutputs.size(), 2);
        EXPECT_TRUE(skeletonResponse.m_createJobOutputs[0].m_critical);
        EXPECT_TRUE(skeletonResponse.m_createJobOutputs[1].m_critical);
        EXPECT_EQ(skeletonResponse.m_createJobOutputs[0].m_jobKey, "Jolt Skeleton");

        builder.ShutDown();
        AssetBuilderSDK::CreateJobsResponse shutdownResponse;
        builder.CreateJobs(request, shutdownResponse);
        EXPECT_EQ(shutdownResponse.m_result, AssetBuilderSDK::CreateJobsResultCode::ShuttingDown);
        EXPECT_TRUE(shutdownResponse.m_createJobOutputs.empty());
    }

    TEST(EditorAssetBuilderTests, TracksCustomShapeDependencyEditsDeletionAndRecovery)
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

        SystemComponentScope systemComponentScope;
        BuilderCustomShapeProvider customShapeProvider;
        Extensions* extensions = Extensions::Get();
        ASSERT_TRUE(extensions);
        ExtensionRegistrationResult providerRegistration = extensions->RegisterExtension(&customShapeProvider, {});
        ASSERT_TRUE(providerRegistration);
        ExtensionInformation providerInformation;
        ASSERT_TRUE(extensions->FindExtensionInformation(
            ExtensionKind::CustomShapeProvider,
            customShapeProvider.GetId(),
            providerInformation));
        EXPECT_EQ(providerInformation.m_version, customShapeProvider.GetVersion());

        SceneSourceData sourceData;
        sourceData.m_shapes.push_back(SceneSourceShapeData{
            .m_geometry = CustomShapeConfiguration{
                .m_data = {4, 2},
                .m_providerId = customShapeProvider.GetId(),
            },
            .m_sourceDependencies = {
                "Objects/Jolt/./BuilderCustomShape.source",
                "Objects/Jolt/BuilderCustomShape.source",
            },
        });

        AZ::Test::ScopedAutoTempDirectory temporaryDirectory;
        constexpr AZStd::string_view SourceRelativePath = "source/dependency_scene.jolt.json";
        constexpr AZStd::string_view DependencyRelativePath = "Objects/Jolt/BuilderCustomShape.source";
        const AZStd::optional<AZ::IO::FixedMaxPath> sourcePath = AZ::Test::CreateTestFile(
            temporaryDirectory,
            SourceRelativePath,
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

        SceneSourceData loadedSourceData;
        const AZ::Outcome<void, AZStd::string> sourceLoadResult =
            AZ::JsonSerializationUtils::LoadObjectFromFile(loadedSourceData, sourcePath->String());
        ASSERT_TRUE(sourceLoadResult.IsSuccess()) << sourceLoadResult.GetError().c_str();
        ASSERT_EQ(loadedSourceData.m_shapes.size(), 1);
        const auto* loadedSourceShape = AZStd::get_if<SceneSourceShapeData>(&loadedSourceData.m_shapes[0]);
        ASSERT_TRUE(loadedSourceShape);
        EXPECT_EQ(
            loadedSourceShape->m_sourceDependencies,
            AZStd::vector<AZStd::string>({
                "Objects/Jolt/./BuilderCustomShape.source",
                "Objects/Jolt/BuilderCustomShape.source",
            }));

        AssetBuilder builder;
        AssetBuilderSDK::CreateJobsRequest createRequest;
        createRequest.m_watchFolder = temporaryDirectory.GetDirectory();
        createRequest.m_sourceFile = SourceRelativePath;
        createRequest.m_enabledPlatforms.emplace_back("pc", AZStd::unordered_set<AZStd::string>{});

        const auto analyze = [&builder, &createRequest]()
        {
            AssetBuilderSDK::CreateJobsResponse response;
            builder.CreateJobs(createRequest, response);
            return response;
        };
        const auto process =
            [&builder, &createRequest, &sourcePath, &temporaryDirectory](const AssetBuilderSDK::CreateJobsResponse& createResponse)
            {
                AssetBuilderSDK::ProcessJobRequest request;
                request.m_fullPath = sourcePath->String();
                request.m_sourceFile = createRequest.m_sourceFile;
                request.m_watchFolder = createRequest.m_watchFolder;
                request.m_tempDirPath = temporaryDirectory.GetDirectory();
                request.m_platformInfo.m_identifier = "pc";
                request.m_jobDescription = createResponse.m_createJobOutputs.front();
                request.m_sourceFileDependencyList = createResponse.m_sourceFileDependencyList;

                AssetBuilderSDK::ProcessJobResponse response;
                builder.ProcessJob(request, response);
                return response;
            };

        AZStd::optional<AZ::IO::FixedMaxPath> dependencyPath = AZ::Test::CreateTestFile(
            temporaryDirectory,
            DependencyRelativePath,
            "dependency revision one");
        ASSERT_TRUE(dependencyPath);
        customShapeProvider.m_dependencyContentHash = AssetBuilderSDK::GetFileHash(dependencyPath->c_str());
        ASSERT_NE(customShapeProvider.m_dependencyContentHash, 0);

        const AssetBuilderSDK::CreateJobsResponse firstAnalysis = analyze();
        ASSERT_EQ(firstAnalysis.m_result, AssetBuilderSDK::CreateJobsResultCode::Success);
        ASSERT_EQ(firstAnalysis.m_createJobOutputs.size(), 1);
        ASSERT_EQ(firstAnalysis.m_sourceFileDependencyList.size(), 1);
        EXPECT_EQ(
            AZ::IO::Path(firstAnalysis.m_sourceFileDependencyList[0].m_sourceFileDependencyPath).LexicallyNormal(),
            AZ::IO::Path(*dependencyPath).LexicallyNormal());
        const AZStd::string firstFingerprint = firstAnalysis.m_createJobOutputs[0].m_additionalFingerprintInfo;
        EXPECT_FALSE(firstFingerprint.empty());
        const AssetBuilderSDK::ProcessJobResponse firstProcess = process(firstAnalysis);
        EXPECT_EQ(firstProcess.m_resultCode, AssetBuilderSDK::ProcessJobResult_Success);
        EXPECT_EQ(firstProcess.m_outputProducts.size(), 1);

        dependencyPath = AZ::Test::CreateTestFile(
            temporaryDirectory,
            DependencyRelativePath,
            "dependency revision two is different");
        ASSERT_TRUE(dependencyPath);
        const AssetBuilderSDK::CreateJobsResponse editedAnalysis = analyze();
        ASSERT_EQ(editedAnalysis.m_result, AssetBuilderSDK::CreateJobsResultCode::Success);
        ASSERT_EQ(editedAnalysis.m_createJobOutputs.size(), 1);
        EXPECT_NE(editedAnalysis.m_createJobOutputs[0].m_additionalFingerprintInfo, firstFingerprint);

        AZ_TEST_START_TRACE_SUPPRESSION;
        const AssetBuilderSDK::ProcessJobResponse staleProviderProcess = process(editedAnalysis);
        AZ_TEST_STOP_TRACE_SUPPRESSION(1);
        EXPECT_EQ(staleProviderProcess.m_resultCode, AssetBuilderSDK::ProcessJobResult_Failed);
        EXPECT_TRUE(staleProviderProcess.m_outputProducts.empty());

        customShapeProvider.m_dependencyContentHash = AssetBuilderSDK::GetFileHash(dependencyPath->c_str());
        ASSERT_NE(customShapeProvider.m_dependencyContentHash, 0);
        const AssetBuilderSDK::ProcessJobResponse editedProcess = process(editedAnalysis);
        EXPECT_EQ(editedProcess.m_resultCode, AssetBuilderSDK::ProcessJobResult_Success);
        EXPECT_EQ(editedProcess.m_outputProducts.size(), 1);

        ASSERT_TRUE(AZ::IO::SystemFile::Delete(dependencyPath->c_str()));
        const AssetBuilderSDK::CreateJobsResponse missingAnalysis = analyze();
        ASSERT_EQ(missingAnalysis.m_result, AssetBuilderSDK::CreateJobsResultCode::Success);
        ASSERT_EQ(missingAnalysis.m_createJobOutputs.size(), 1);
        ASSERT_EQ(missingAnalysis.m_sourceFileDependencyList.size(), 1);
        EXPECT_NE(
            missingAnalysis.m_createJobOutputs[0].m_additionalFingerprintInfo,
            editedAnalysis.m_createJobOutputs[0].m_additionalFingerprintInfo);
        AZ_TEST_START_TRACE_SUPPRESSION;
        const AssetBuilderSDK::ProcessJobResponse missingProcess = process(missingAnalysis);
        AZ_TEST_STOP_TRACE_SUPPRESSION(1);
        EXPECT_EQ(missingProcess.m_resultCode, AssetBuilderSDK::ProcessJobResult_Failed);
        EXPECT_TRUE(missingProcess.m_outputProducts.empty());

        dependencyPath = AZ::Test::CreateTestFile(
            temporaryDirectory,
            DependencyRelativePath,
            "dependency revision three restores the source");
        ASSERT_TRUE(dependencyPath);
        customShapeProvider.m_dependencyContentHash = AssetBuilderSDK::GetFileHash(dependencyPath->c_str());
        ASSERT_NE(customShapeProvider.m_dependencyContentHash, 0);
        const AssetBuilderSDK::CreateJobsResponse recoveredAnalysis = analyze();
        ASSERT_EQ(recoveredAnalysis.m_result, AssetBuilderSDK::CreateJobsResultCode::Success);
        ASSERT_EQ(recoveredAnalysis.m_createJobOutputs.size(), 1);
        EXPECT_NE(
            recoveredAnalysis.m_createJobOutputs[0].m_additionalFingerprintInfo,
            missingAnalysis.m_createJobOutputs[0].m_additionalFingerprintInfo);
        const AssetBuilderSDK::ProcessJobResponse recoveredProcess = process(recoveredAnalysis);
        EXPECT_EQ(recoveredProcess.m_resultCode, AssetBuilderSDK::ProcessJobResult_Success);
        EXPECT_EQ(recoveredProcess.m_outputProducts.size(), 1);

        EXPECT_EQ(extensions->UnregisterExtension(providerRegistration.m_handle), ExtensionRegistrationStatus::Success);
        customShapeProvider.m_version = 2;
        providerRegistration = extensions->RegisterExtension(&customShapeProvider, {});
        ASSERT_TRUE(providerRegistration);
        const AssetBuilderSDK::CreateJobsResponse versionedAnalysis = analyze();
        ASSERT_EQ(versionedAnalysis.m_result, AssetBuilderSDK::CreateJobsResultCode::Success);
        ASSERT_EQ(versionedAnalysis.m_createJobOutputs.size(), 1);
        EXPECT_NE(
            versionedAnalysis.m_createJobOutputs[0].m_additionalFingerprintInfo,
            recoveredAnalysis.m_createJobOutputs[0].m_additionalFingerprintInfo);
        const AssetBuilderSDK::ProcessJobResponse versionedProcess = process(versionedAnalysis);
        EXPECT_EQ(versionedProcess.m_resultCode, AssetBuilderSDK::ProcessJobResult_Success);
        EXPECT_EQ(versionedProcess.m_outputProducts.size(), 1);
        EXPECT_EQ(extensions->UnregisterExtension(providerRegistration.m_handle), ExtensionRegistrationStatus::Success);
    }

    TEST(EditorAssetBuilderTests, RejectsInvalidCustomShapeSourceDependencyContracts)
    {
        NameDictionaryScope nameDictionaryScope;
        FileIoScope fileIoScope;
        SystemComponentScope systemComponentScope;
        AZ::Test::ScopedAutoTempDirectory temporaryDirectory;

        AZStd::vector<CustomShapeDependency> conflictingDependencies = {
            {"Objects/Jolt/./Conflict.source", 1},
            {"Objects/Jolt/Conflict.source", 2},
        };
        AZStd::string dependencyError;
        EXPECT_FALSE(CanonicalizeCustomShapeDependencies(conflictingDependencies, dependencyError));
        EXPECT_FALSE(dependencyError.empty());

        SceneSourceData sourceData;
        sourceData.m_shapes.push_back(SceneSourceShapeData{
            .m_geometry = BoxShapeConfiguration{},
            .m_sourceDependencies = {"Objects/Jolt/Invalid.source"},
        });
        EXPECT_FALSE(AnalyzeSceneSource(sourceData, temporaryDirectory.GetDirectory()).IsSuccess());

        sourceData.m_shapes[0] = SceneSourceShapeData{
            .m_geometry = CustomConvexShapeConfiguration{
                .m_providerId = AZ::TypeId::CreateRandom(),
            },
            .m_sourceDependencies = {"Objects/Jolt/Invalid.source"},
        };
        EXPECT_FALSE(AnalyzeSceneSource(sourceData, temporaryDirectory.GetDirectory()).IsSuccess());

        BuilderCustomShapeProvider customShapeProvider;
        Extensions* extensions = Extensions::Get();
        ASSERT_TRUE(extensions);
        const ExtensionRegistrationResult providerRegistration = extensions->RegisterExtension(&customShapeProvider, {});
        ASSERT_TRUE(providerRegistration);

        sourceData.m_shapes[0] = SceneSourceShapeData{
            .m_geometry = CustomShapeConfiguration{
                .m_providerId = customShapeProvider.GetId(),
            },
            .m_sourceDependencies = {"../Invalid.source"},
        };
        EXPECT_FALSE(AnalyzeSceneSource(sourceData, temporaryDirectory.GetDirectory()).IsSuccess());

        sourceData.m_shapes[0] = SceneSourceShapeData{
            .m_geometry = CustomShapeConfiguration{
                .m_providerId = AZ::TypeId::CreateRandom(),
            },
        };
        EXPECT_FALSE(AnalyzeSceneSource(sourceData, temporaryDirectory.GetDirectory()).IsSuccess());

        EXPECT_EQ(extensions->UnregisterExtension(providerRegistration.m_handle), ExtensionRegistrationStatus::Success);
    }

    TEST(EditorAssetBuilderTests, RejectsUnsupportedDocumentClassesWithoutPublishingProducts)
    {
        FileIoScope fileIoScope;
        AZ::Test::ScopedAutoTempDirectory temporaryDirectory;
        const AZStd::optional<AZ::IO::FixedMaxPath> sourcePath = AZ::Test::CreateTestFile(
            temporaryDirectory,
            "unsupported.jolt.json",
            R"({"Type":"JsonSerialization","Version":1,"ClassName":"UnknownSourceData","ClassData":{}})");
        ASSERT_TRUE(sourcePath);

        AssetBuilder builder;
        AssetBuilderSDK::CreateJobsRequest createRequest;
        createRequest.m_watchFolder = temporaryDirectory.GetDirectory();
        createRequest.m_sourceFile = "unsupported.jolt.json";
        createRequest.m_enabledPlatforms.emplace_back("pc", AZStd::unordered_set<AZStd::string>{});
        AssetBuilderSDK::CreateJobsResponse createResponse;
        AZ_TEST_START_TRACE_SUPPRESSION;
        builder.CreateJobs(createRequest, createResponse);
        EXPECT_EQ(createResponse.m_result, AssetBuilderSDK::CreateJobsResultCode::Failed);
        EXPECT_TRUE(createResponse.m_createJobOutputs.empty());

        AssetBuilderSDK::ProcessJobRequest processRequest;
        processRequest.m_fullPath = sourcePath->String();
        processRequest.m_sourceFile = "unsupported.jolt.json";
        processRequest.m_tempDirPath = temporaryDirectory.GetDirectory();
        processRequest.m_platformInfo.m_identifier = "pc";
        AssetBuilderSDK::ProcessJobResponse processResponse;
        builder.ProcessJob(processRequest, processResponse);
        EXPECT_EQ(processResponse.m_resultCode, AssetBuilderSDK::ProcessJobResult_Failed);
        EXPECT_TRUE(processResponse.m_outputProducts.empty());
        AZ_TEST_STOP_TRACE_SUPPRESSION(2);
    }

    TEST(EditorAssetBuilderTests, RejectsInvalidDocumentEnvelopes)
    {
        FileIoScope fileIoScope;
        AZ::Test::ScopedAutoTempDirectory temporaryDirectory;
        constexpr AZStd::array InvalidDocuments = {
            R"({"Version":1,"ClassName":"SceneSourceData","ClassData":{}})",
            R"({"Type":"JsonSerialization","Version":2,"ClassName":"SceneSourceData","ClassData":{}})",
            R"({"Type":"JsonSerialization","Version":1,"ClassData":{}})",
        };

        AssetBuilder builder;
        AZ_TEST_START_TRACE_SUPPRESSION;
        for (size_t documentIndex = 0; documentIndex < InvalidDocuments.size(); ++documentIndex)
        {
            const AZStd::string sourceFile = AZStd::string::format(
                "invalid_%zu.jolt.json",
                documentIndex);
            const AZStd::optional<AZ::IO::FixedMaxPath> sourcePath = AZ::Test::CreateTestFile(
                temporaryDirectory,
                AZ::IO::PathView(sourceFile),
                InvalidDocuments[documentIndex]);
            ASSERT_TRUE(sourcePath);

            AssetBuilderSDK::CreateJobsRequest request;
            request.m_watchFolder = temporaryDirectory.GetDirectory();
            request.m_sourceFile = sourceFile;
            request.m_enabledPlatforms.emplace_back("pc", AZStd::unordered_set<AZStd::string>{});
            AssetBuilderSDK::CreateJobsResponse response;
            builder.CreateJobs(request, response);
            EXPECT_EQ(response.m_result, AssetBuilderSDK::CreateJobsResultCode::Failed);
            EXPECT_TRUE(response.m_createJobOutputs.empty());
        }
        AZ_TEST_STOP_TRACE_SUPPRESSION(3);
    }

    TEST(EditorAssetBuilderTests, RejectsMalformedAndTruncatedJsonWithoutPublishingProducts)
    {
        FileIoScope fileIoScope;
        AZ::Test::ScopedAutoTempDirectory temporaryDirectory;
        constexpr AZStd::array InvalidDocuments = {
            R"({"Type":"JsonSerialization","Version":1,"ClassName":"SceneSourceData","ClassData":)",
            R"({"Type":"JsonSerialization")",
        };

        AssetBuilder builder;
        AZ_TEST_START_TRACE_SUPPRESSION;
        for (size_t documentIndex = 0; documentIndex < InvalidDocuments.size(); ++documentIndex)
        {
            const AZStd::string sourceFile = AZStd::string::format(
                "malformed_%zu.jolt.json",
                documentIndex);
            const AZStd::optional<AZ::IO::FixedMaxPath> sourcePath = AZ::Test::CreateTestFile(
                temporaryDirectory,
                AZ::IO::PathView(sourceFile),
                InvalidDocuments[documentIndex]);
            ASSERT_TRUE(sourcePath);

            AssetBuilderSDK::ProcessJobRequest request;
            request.m_fullPath = sourcePath->String();
            request.m_sourceFile = sourceFile;
            request.m_tempDirPath = temporaryDirectory.GetDirectory();
            request.m_platformInfo.m_identifier = "pc";
            AssetBuilderSDK::ProcessJobResponse response;
            builder.ProcessJob(request, response);
            EXPECT_EQ(response.m_resultCode, AssetBuilderSDK::ProcessJobResult_Failed);
            EXPECT_TRUE(response.m_outputProducts.empty());
        }
        AZ_TEST_STOP_TRACE_SUPPRESSION(2);
    }

    TEST(EditorAssetBuilderTests, RejectsProcessJobWithoutTargetPlatform)
    {
        FileIoScope fileIoScope;
        AZ::Test::ScopedAutoTempDirectory temporaryDirectory;
        const AZStd::optional<AZ::IO::FixedMaxPath> sourcePath = AZ::Test::CreateTestFile(
            temporaryDirectory,
            "scene.jolt.json",
            R"({"Type":"JsonSerialization","Version":1,"ClassName":"SceneSourceData","ClassData":{}})");
        ASSERT_TRUE(sourcePath);

        AssetBuilderSDK::ProcessJobRequest request;
        request.m_fullPath = sourcePath->String();
        request.m_sourceFile = "scene.jolt.json";
        request.m_tempDirPath = temporaryDirectory.GetDirectory();
        AssetBuilderSDK::ProcessJobResponse response;
        AssetBuilder builder;
        AZ_TEST_START_TRACE_SUPPRESSION;
        builder.ProcessJob(request, response);
        AZ_TEST_STOP_TRACE_SUPPRESSION(1);
        EXPECT_EQ(response.m_resultCode, AssetBuilderSDK::ProcessJobResult_Failed);
        EXPECT_TRUE(response.m_outputProducts.empty());
    }

    TEST(EditorAssetBuilderTests, ProcessesJsonSourceIntoLoadableBinaryAsset)
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

        SystemComponentScope systemComponentScope;

        SceneSourceData sourceData;
        sourceData.m_name = AZ::Name("BuilderScene");
        BuilderCustomShapeProvider customShapeProvider;
        Cooking* cooking = Cooking::Get();
        ASSERT_TRUE(cooking);
        Extensions* extensions = Extensions::Get();
        ASSERT_TRUE(extensions);
        Scenes* scenes = Scenes::Get();
        ASSERT_TRUE(scenes);
        SoftBodies* softBodies = SoftBodies::Get();
        ASSERT_TRUE(softBodies);
        Worlds* worlds = Worlds::Get();
        ASSERT_TRUE(worlds);
        ExtensionRegistrationResult customShapeRegistration =
            extensions->RegisterExtension(&customShapeProvider, {});
        ASSERT_TRUE(customShapeRegistration);

        sourceData.m_shapes.push_back(SceneSourceShapeData{
            .m_geometry = CustomShapeConfiguration{
                .m_data = {4, 2},
                .m_providerId = customShapeProvider.GetId(),
            },
            .m_sourceDependencies = {"Objects/Jolt/BuilderCustomShape.source"},
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

        SoftBodyComponentConfiguration softBodyConfiguration =
            SoftBodyComponentConfiguration::CreateDefault();
        softBodyConfiguration.m_definition.m_inverseBinds = {
            {
                .m_transform = AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisX(0.25f)),
                .m_jointIndex = 0,
            },
        };
        SoftBodySkinConstraint& skinConstraint =
            softBodyConfiguration.m_definition.m_skinConstraints.emplace_back();
        skinConstraint.m_weights[0] = {
            .m_inverseBindIndex = 0,
            .m_weight = 1.0f,
        };
        skinConstraint.m_vertex = 0;
        skinConstraint.m_backstopDistance = 0.1f;
        skinConstraint.m_backstopRadius = 0.2f;
        skinConstraint.m_maximumDistance = 0.3f;
        const SoftBodyDefinitionConfiguration& definition = softBodyConfiguration.m_definition;
        sourceData.m_softBodyDefinitions.push_back(SceneSourceSoftBodyDefinition{
            .m_vertices = definition.m_vertices,
            .m_faces = definition.m_faces,
            .m_materialIndices = {},
            .m_vertexAttributes = definition.m_vertexAttributes,
            .m_edgeConstraints = definition.m_edgeConstraints,
            .m_dihedralBendConstraints = definition.m_dihedralBendConstraints,
            .m_longRangeConstraints = definition.m_longRangeConstraints,
            .m_rodStretchShearConstraints = definition.m_rodStretchShearConstraints,
            .m_rodBendTwistConstraints = definition.m_rodBendTwistConstraints,
            .m_volumeConstraints = definition.m_volumeConstraints,
            .m_inverseBinds = definition.m_inverseBinds,
            .m_skinConstraints = definition.m_skinConstraints,
            .m_shearAngleTolerance = definition.m_shearAngleTolerance,
            .m_bendType = definition.m_bendType,
            .m_createFaceConstraints = definition.m_createFaceConstraints,
            .m_optimize = definition.m_optimize,
        });
        sourceData.m_bodies.push_back(SceneAssetSoftBody{
            .m_transform = {.m_position = {.m_x = 4.0, .m_z = 2.0}},
            .m_name = AZ::Name("Cloth"),
            .m_definitionIndex = 0,
        });

        AZ::Test::ScopedAutoTempDirectory temporaryDirectory;
        const AZStd::optional<AZ::IO::FixedMaxPath> dependencyPath = AZ::Test::CreateTestFile(
            temporaryDirectory,
            "Objects/Jolt/BuilderCustomShape.source",
            "builder custom shape dependency");
        ASSERT_TRUE(dependencyPath);
        customShapeProvider.m_dependencyContentHash = AssetBuilderSDK::GetFileHash(dependencyPath->c_str());
        ASSERT_NE(customShapeProvider.m_dependencyContentHash, 0);
        const AZStd::optional<AZ::IO::FixedMaxPath> sourcePath = AZ::Test::CreateTestFile(
            temporaryDirectory,
            "source/test_scene.jolt.json",
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

        AssetBuilder builder;
        AssetBuilderSDK::ProcessJobRequest request;
        request.m_fullPath = sourcePath->String();
        request.m_sourceFile = "source/test_scene.jolt.json";
        request.m_watchFolder = temporaryDirectory.GetDirectory();
        request.m_tempDirPath = temporaryDirectory.GetDirectory();
        request.m_platformInfo.m_identifier = GetNativeAssetPlatform();
        AssetBuilderSDK::ProcessJobResponse response;
        builder.ProcessJob(request, response);
        ASSERT_EQ(response.m_resultCode, AssetBuilderSDK::ProcessJobResult_Success);
        ASSERT_EQ(response.m_outputProducts.size(), 1);
        EXPECT_EQ(response.m_outputProducts.front().m_productAssetType, SceneAssetTypeId);
        EXPECT_EQ(response.m_outputProducts.front().m_productSubID, 1);
        ASSERT_EQ(response.m_outputProducts.front().m_pathDependencies.size(), 1);
        const AssetBuilderSDK::ProductPathDependency& productDependency =
            *response.m_outputProducts.front().m_pathDependencies.begin();
        EXPECT_EQ(productDependency.m_dependencyPath, "Objects/Jolt/BuilderCustomShape.source");
        EXPECT_EQ(
            productDependency.m_dependencyType,
            AssetBuilderSDK::ProductPathDependencyType::SourceFile);
        EXPECT_TRUE(
            AZStd::string_view(response.m_outputProducts.front().m_productFileName).ends_with("test_scene.jolt"));

        AZ::IO::SystemFile productFile;
        ASSERT_TRUE(productFile.Open(
            response.m_outputProducts.front().m_productFileName.c_str(),
            AZ::IO::SystemFile::SF_OPEN_READ_ONLY));
        const AZ::IO::SystemFile::SizeType productSize = productFile.Length();
        ASSERT_GT(productSize, 1);
        AZStd::vector<AZ::u8> productBytes(aznumeric_cast<size_t>(productSize));
        EXPECT_EQ(productFile.Read(productSize, productBytes.data()), productSize);
        productFile.Close();

        AZStd::vector<AZ::u8> truncatedProduct(
            productBytes.begin(),
            productBytes.begin() + productBytes.size() / 2);
        AZ::IO::ByteContainerStream truncatedProductStream(&truncatedProduct);
        SceneAsset truncatedAsset;
        truncatedAsset.m_data.m_name = AZ::Name("Unchanged");
        EXPECT_FALSE(LoadAssetProduct(
            truncatedProductStream,
            &truncatedAsset,
            SceneAssetTypeId,
            serializeContextScope.Get()));
        EXPECT_EQ(truncatedAsset.m_data.m_name, AZ::Name("Unchanged"));

        AZStd::vector<AZ::u8> corruptProduct = productBytes;
        corruptProduct.back() ^= 1;
        AZ::IO::ByteContainerStream corruptProductStream(&corruptProduct);
        SceneAsset corruptAsset;
        corruptAsset.m_data.m_name = AZ::Name("Unchanged");
        EXPECT_FALSE(LoadAssetProduct(
            corruptProductStream,
            &corruptAsset,
            SceneAssetTypeId,
            serializeContextScope.Get()));
        EXPECT_EQ(corruptAsset.m_data.m_name, AZ::Name("Unchanged"));

        constexpr size_t ProductVersionOffset = 8;
        AZStd::vector<AZ::u8> unsupportedProduct = productBytes;
        unsupportedProduct[ProductVersionOffset] = 3;
        AZ::IO::ByteContainerStream unsupportedProductStream(&unsupportedProduct);
        SceneAsset unsupportedAsset;
        unsupportedAsset.m_data.m_name = AZ::Name("Unchanged");
        EXPECT_FALSE(LoadAssetProduct(
            unsupportedProductStream,
            &unsupportedAsset,
            SceneAssetTypeId,
            serializeContextScope.Get()));
        EXPECT_EQ(unsupportedAsset.m_data.m_name, AZ::Name("Unchanged"));

        SceneAsset loadedAsset;
        AZ::IO::ByteContainerStream productStream(&productBytes);
        EXPECT_TRUE(LoadAssetProduct(
            productStream,
            &loadedAsset,
            SceneAssetTypeId,
            serializeContextScope.Get()));
        EXPECT_EQ(loadedAsset.m_data.m_nativeCachePlatform, GetNativeAssetPlatform());
        EXPECT_EQ(loadedAsset.m_data.m_nativeCacheBuildFingerprint, GetNativeBuildFingerprint());
        EXPECT_EQ(loadedAsset.m_data.m_name, sourceData.m_name);
        ASSERT_EQ(loadedAsset.m_data.m_shapes.size(), 1);
        const auto* customShapeSource = AZStd::get_if<SceneSourceShapeData>(&loadedAsset.m_data.m_shapes[0].m_source);
        ASSERT_TRUE(customShapeSource);
        const auto* customShapeConfiguration = AZStd::get_if<CustomShapeConfiguration>(&customShapeSource->m_geometry);
        ASSERT_TRUE(customShapeConfiguration);
        EXPECT_EQ(customShapeConfiguration->m_data, AZStd::vector<AZ::u8>({4, 2}));
        EXPECT_EQ(customShapeConfiguration->m_providerId, customShapeProvider.GetId());
        EXPECT_TRUE(customShapeSource->m_sourceDependencies.empty());
        EXPECT_EQ(loadedAsset.m_data.m_shapes[0].m_providerId, customShapeProvider.GetId());
        EXPECT_EQ(loadedAsset.m_data.m_shapes[0].m_providerVersion, customShapeProvider.GetVersion());
        EXPECT_FALSE(loadedAsset.m_data.m_shapes[0].m_archive.m_binaryState.empty());
        EXPECT_NE(loadedAsset.m_data.m_shapes[0].m_archive.m_buildFingerprint, 0);
        ASSERT_EQ(loadedAsset.m_data.m_shapes[0].m_archive.m_dependencies.size(), 1);
        EXPECT_EQ(
            loadedAsset.m_data.m_shapes[0].m_archive.m_dependencies[0].m_path,
            "Objects/Jolt/BuilderCustomShape.source");
        EXPECT_NE(loadedAsset.m_data.m_shapes[0].m_archive.m_dependencies[0].m_contentHash, 0);
        EXPECT_NE(loadedAsset.m_data.m_shapes[0].m_archive.m_contentHash, 0);
        EXPECT_NE(loadedAsset.m_data.m_shapes[0].m_archive.m_formatVersion, 0);
        EXPECT_EQ(
            loadedAsset.m_data.m_shapes[0].m_archive.m_buildFingerprint,
            GetNativeBuildFingerprint());
        EXPECT_EQ(loadedAsset.m_data.m_shapes[0].m_archive.m_materialCount, 1);
        EXPECT_EQ(loadedAsset.m_data.m_shapes[0].m_archive.m_childShapeCount, 0);
        AZ::HashValue64 shapeArchiveHash = AZ::TypeHash64(
            loadedAsset.m_data.m_shapes[0].m_archive.m_binaryState.data(),
            loadedAsset.m_data.m_shapes[0].m_archive.m_binaryState.size());
        shapeArchiveHash = AZ::TypeHash64(
            loadedAsset.m_data.m_shapes[0].m_archive.m_providerId,
            shapeArchiveHash);
        shapeArchiveHash = AZ::TypeHash64(
            loadedAsset.m_data.m_shapes[0].m_archive.m_providerVersion,
            shapeArchiveHash);
        shapeArchiveHash = AZ::TypeHash64(
            loadedAsset.m_data.m_shapes[0].m_archive.m_dependencies.size(),
            shapeArchiveHash);
        for (const CustomShapeDependency& dependency : loadedAsset.m_data.m_shapes[0].m_archive.m_dependencies)
        {
            shapeArchiveHash = AZ::TypeHash64(
                reinterpret_cast<const AZ::u8*>(dependency.m_path.data()),
                dependency.m_path.size(),
                shapeArchiveHash);
            shapeArchiveHash = AZ::TypeHash64(dependency.m_contentHash, shapeArchiveHash);
        }
        EXPECT_EQ(
            loadedAsset.m_data.m_shapes[0].m_archive.m_contentHash,
            static_cast<AZ::u64>(shapeArchiveHash));
        ASSERT_EQ(loadedAsset.m_data.m_softBodyDefinitions.size(), 1);
        EXPECT_FALSE(loadedAsset.m_data.m_softBodyDefinitions[0].m_archive.m_binaryState.empty());
        EXPECT_NE(loadedAsset.m_data.m_softBodyDefinitions[0].m_archive.m_buildFingerprint, 0);
        EXPECT_EQ(loadedAsset.m_data.m_bodies.size(), 3);
        EXPECT_EQ(loadedAsset.m_data.m_constraints.size(), 1);

        const AZStd::array<MaterialHandle, 1> shapeMaterials = {MaterialHandle::Invalid};
        const CookedShapeHandle importedShapeHandle = cooking->ImportShape(
            loadedAsset.m_data.m_shapes[0].m_archive,
            shapeMaterials,
            {});
        ASSERT_TRUE(importedShapeHandle);
        EXPECT_TRUE(cooking->DestroyCookedShape(importedShapeHandle));

        const SoftBodyDefinitionHandle importedSoftBodyHandle = softBodies->ImportSoftBodyDefinition(
            loadedAsset.m_data.m_softBodyDefinitions[0].m_archive,
            {});
        ASSERT_TRUE(importedSoftBodyHandle);
        EXPECT_TRUE(softBodies->DestroySoftBodyDefinition(importedSoftBodyHandle));

        const SceneDefinitionHandle baselineDefinitionHandle =
            scenes->CreateSceneDefinition(loadedAsset.m_data);
        ASSERT_TRUE(baselineDefinitionHandle);
        EXPECT_TRUE(scenes->DestroySceneDefinition(baselineDefinitionHandle));

        EXPECT_EQ(
            extensions->UnregisterExtension(customShapeRegistration.m_handle),
            ExtensionRegistrationStatus::Success);
        AZ_TEST_START_TRACE_SUPPRESSION;
        EXPECT_FALSE(scenes->CreateSceneDefinition(loadedAsset.m_data));
        AZ_TEST_STOP_TRACE_SUPPRESSION(1);
        customShapeRegistration = extensions->RegisterExtension(&customShapeProvider, {});
        ASSERT_TRUE(customShapeRegistration);

        const AZ::u32 validArchiveVersion = loadedAsset.m_data.m_shapes[0].m_archive.m_formatVersion;
        loadedAsset.m_data.m_shapes[0].m_archive.m_formatVersion = validArchiveVersion + 1;
        const SceneDefinitionHandle fallbackDefinitionHandle = scenes->CreateSceneDefinition(loadedAsset.m_data);
        ASSERT_TRUE(fallbackDefinitionHandle);
        EXPECT_TRUE(scenes->DestroySceneDefinition(fallbackDefinitionHandle));
        loadedAsset.m_data.m_shapes[0].m_archive.m_formatVersion = validArchiveVersion;

        const SceneDefinitionHandle definitionHandle = scenes->CreateSceneDefinition(loadedAsset.m_data);
        ASSERT_TRUE(definitionHandle);
        const WorldHandle worldHandle = worlds->GetDefaultWorldHandle();
        const SceneInstanceHandle instanceHandle = scenes->InstantiateScene(
            worldHandle,
            definitionHandle);
        ASSERT_TRUE(instanceHandle);
        AZStd::array<BodyHandle, 3> bodies;
        AZStd::array<ConstraintHandle, 1> constraints;
        EXPECT_TRUE(scenes->GetSceneBodies(worldHandle, instanceHandle, bodies).IsComplete());
        EXPECT_TRUE(scenes->GetSceneConstraints(worldHandle, instanceHandle, constraints).IsComplete());
        EXPECT_TRUE(bodies[0]);
        EXPECT_TRUE(bodies[1]);
        EXPECT_TRUE(bodies[2]);
        EXPECT_TRUE(constraints[0]);
        EXPECT_TRUE(scenes->DestroySceneInstance(worldHandle, instanceHandle));
        EXPECT_TRUE(scenes->DestroySceneDefinition(definitionHandle));

        request.m_platformInfo.m_identifier = "non-native-test";
        AssetBuilderSDK::ProcessJobResponse portableResponse;
        builder.ProcessJob(request, portableResponse);
        ASSERT_EQ(portableResponse.m_resultCode, AssetBuilderSDK::ProcessJobResult_Success);
        ASSERT_EQ(portableResponse.m_outputProducts.size(), 1);

        SceneAsset portableAsset;
        ASSERT_TRUE(LoadAssetProductFile(
            portableResponse.m_outputProducts.front().m_productFileName,
            &portableAsset,
            SceneAssetTypeId,
            serializeContextScope.Get()));
        EXPECT_TRUE(portableAsset.m_data.m_nativeCachePlatform.empty());
        EXPECT_EQ(portableAsset.m_data.m_nativeCacheBuildFingerprint, 0);
        ASSERT_EQ(portableAsset.m_data.m_shapes.size(), 1);
        EXPECT_TRUE(portableAsset.m_data.m_shapes[0].m_archive.m_binaryState.empty());
        ASSERT_EQ(portableAsset.m_data.m_shapes[0].m_dependencies.size(), 1);
        EXPECT_EQ(portableAsset.m_data.m_shapes[0].m_providerId, customShapeProvider.GetId());
        EXPECT_EQ(portableAsset.m_data.m_shapes[0].m_providerVersion, customShapeProvider.GetVersion());
        EXPECT_EQ(
            portableAsset.m_data.m_shapes[0].m_dependencies[0].m_path,
            "Objects/Jolt/BuilderCustomShape.source");
        const SceneDefinitionHandle portableDefinitionHandle = scenes->CreateSceneDefinition(portableAsset.m_data);
        ASSERT_TRUE(portableDefinitionHandle);
        EXPECT_TRUE(scenes->DestroySceneDefinition(portableDefinitionHandle));

        const AZ::u64 providerVersion = portableAsset.m_data.m_shapes[0].m_providerVersion;
        ++portableAsset.m_data.m_shapes[0].m_providerVersion;
        EXPECT_FALSE(scenes->CreateSceneDefinition(portableAsset.m_data));
        portableAsset.m_data.m_shapes[0].m_providerVersion = providerVersion;

        EXPECT_EQ(
            extensions->UnregisterExtension(customShapeRegistration.m_handle),
            ExtensionRegistrationStatus::Success);
    }

    TEST(EditorAssetBuilderTests, ProcessesSkeletonAndAnimationsIntoValidatedNativeArchives)
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
            "source/test_skeleton.jolt.json",
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
        request.m_sourceFile = "test_skeleton.jolt.json";
        request.m_tempDirPath = temporaryDirectory.GetDirectory();
        request.m_platformInfo.m_identifier = GetNativeAssetPlatform();
        AssetBuilderSDK::ProcessJobResponse response;
        AssetBuilder builder;
        EXPECT_FALSE(RuntimeConfiguration::Get());
        builder.Register();
        EXPECT_TRUE(RuntimeConfiguration::Get());
        builder.ProcessJob(request, response);
        ASSERT_EQ(response.m_resultCode, AssetBuilderSDK::ProcessJobResult_Success);
        ASSERT_EQ(response.m_outputProducts.size(), 1);
        EXPECT_EQ(response.m_outputProducts.front().m_productAssetType, SkeletonAssetTypeId);
        EXPECT_EQ(response.m_outputProducts.front().m_productSubID, 1);
        EXPECT_TRUE(
            AZStd::string_view(response.m_outputProducts.front().m_productFileName).ends_with("test_skeleton.jolt"));

        SkeletonAsset loadedAsset;
        ASSERT_TRUE(LoadAssetProductFile(
            response.m_outputProducts.front().m_productFileName,
            &loadedAsset,
            SkeletonAssetTypeId,
            serializeContextScope.Get()));
        EXPECT_EQ(loadedAsset.m_data.m_nativeCachePlatform, GetNativeAssetPlatform());
        EXPECT_EQ(loadedAsset.m_data.m_nativeCacheBuildFingerprint, GetNativeBuildFingerprint());
        EXPECT_EQ(loadedAsset.m_data.m_name.GetStringView(), sourceData.m_name);
        ASSERT_EQ(loadedAsset.m_data.m_sourceSkeleton.m_joints.size(), 2);
        ASSERT_EQ(loadedAsset.m_data.m_animations.size(), 1);
        EXPECT_EQ(loadedAsset.m_data.m_animations.front().m_name.GetStringView(), sourceAnimation.m_name);
        EXPECT_EQ(loadedAsset.m_data.m_animations.front().m_source.m_joints.size(), 1);

        Skeletons* skeletons = Skeletons::Get();
        ASSERT_TRUE(skeletons);
        const SkeletonDefinitionHandle skeletonHandle = skeletons->ImportSkeletonDefinition(loadedAsset.m_data.m_skeleton);
        ASSERT_TRUE(skeletonHandle);
        const SkeletalAnimationHandle animationHandle =
            skeletons->ImportSkeletalAnimation(loadedAsset.m_data.m_animations.front().m_archive);
        ASSERT_TRUE(animationHandle);

        SkeletonJoint joints[2];
        const QueryResult jointResult = skeletons->GetSkeletonJoints(skeletonHandle, joints);
        EXPECT_EQ(jointResult.m_hitCount, 2);
        EXPECT_EQ(joints[1].m_name.GetStringView(), "child");

        SkeletalAnimationState animationState;
        ASSERT_TRUE(skeletons->GetSkeletalAnimationState(animationHandle, animationState));
        EXPECT_EQ(animationState.m_jointCount, 1);
        EXPECT_FLOAT_EQ(animationState.m_duration, 1.0f);

        EXPECT_TRUE(skeletons->DestroySkeletalAnimation(animationHandle));
        EXPECT_TRUE(skeletons->DestroySkeletonDefinition(skeletonHandle));

        request.m_platformInfo.m_identifier = "non-native-test";
        AssetBuilderSDK::ProcessJobResponse portableResponse;
        builder.ProcessJob(request, portableResponse);
        ASSERT_EQ(portableResponse.m_resultCode, AssetBuilderSDK::ProcessJobResult_Success);
        ASSERT_EQ(portableResponse.m_outputProducts.size(), 1);

        SkeletonAsset portableAsset;
        ASSERT_TRUE(LoadAssetProductFile(
            portableResponse.m_outputProducts.front().m_productFileName,
            &portableAsset,
            SkeletonAssetTypeId,
            serializeContextScope.Get()));
        EXPECT_TRUE(portableAsset.m_data.m_nativeCachePlatform.empty());
        EXPECT_EQ(portableAsset.m_data.m_nativeCacheBuildFingerprint, 0);
        EXPECT_TRUE(portableAsset.m_data.m_skeleton.m_binaryState.empty());
        ASSERT_EQ(portableAsset.m_data.m_sourceSkeleton.m_joints.size(), 2);
        ASSERT_EQ(portableAsset.m_data.m_animations.size(), 1);
        EXPECT_TRUE(portableAsset.m_data.m_animations.front().m_archive.m_binaryState.empty());

        const SkeletonDefinitionHandle portableSkeletonHandle =
            skeletons->CreateSkeletonDefinition(portableAsset.m_data.m_sourceSkeleton);
        const SkeletalAnimationHandle portableAnimationHandle =
            skeletons->CreateSkeletalAnimation(portableAsset.m_data.m_animations.front().m_source);
        ASSERT_TRUE(portableSkeletonHandle);
        ASSERT_TRUE(portableAnimationHandle);
        EXPECT_TRUE(skeletons->DestroySkeletalAnimation(portableAnimationHandle));
        EXPECT_TRUE(skeletons->DestroySkeletonDefinition(portableSkeletonHandle));

        builder.ShutDown();
        builder.BusDisconnect();
    }
} // namespace Jolt::Editor
