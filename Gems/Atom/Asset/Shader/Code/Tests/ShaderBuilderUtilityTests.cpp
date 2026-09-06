/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Atom/RHI.Edit/Utils.h>
#include <AzCore/IO/Path/Path.h>
#include <AzCore/JSON/document.h>
#include <AzCore/JSON/stringbuffer.h>
#include <AzCore/JSON/writer.h>
#include <AzCore/Platform.h>
#include <AzCore/Serialization/Json/JsonSystemComponent.h>
#include <AzCore/Serialization/Json/JsonUtils.h>
#include <AzCore/Serialization/Json/RegistrationContext.h>
#include <AzCore/UnitTest/MockComponentApplication.h>
#include <AzCore/Utils/Utils.h>
#include <AzFramework/IO/LocalFileIO.h>
#include <AzFramework/Process/ProcessWatcher.h>
#include <AzTest/AzTest.h>
#include <AzTest/Utils.h>
#include <AzslCompiler.h>
#include <ShaderBuilderUtility.h>
#include "Common/ShaderBuilderTestFixture.h"

namespace UnitTest
{
    class ShaderBuilderUtilityTests : public ShaderBuilderTestFixture
    {
    };

    TEST_F(ShaderBuilderUtilityTests, PrepareSourceInputReturnsFailureForMissingDescriptor)
    {
        AZ::Test::ScopedAutoTempDirectory directory;
        const AZStd::string path = (directory.GetDirectoryAsPath() / "missing.shader").Native();
        AZ::RPI::ShaderSourceData descriptor;
        descriptor.m_source = "unchanged.azsl";

        const auto result = AZ::ShaderBuilder::ShaderBuilderUtility::PrepareSourceInput(path, descriptor);

        ASSERT_FALSE(result.IsSuccess());
        EXPECT_FALSE(result.GetError().empty());
        EXPECT_EQ(descriptor.m_source, "unchanged.azsl");
    }

    TEST_F(ShaderBuilderUtilityTests, PrepareSourceInputReturnsFailureForMalformedDescriptor)
    {
        AZ::Test::ScopedAutoTempDirectory directory;
        const auto file = AZ::Test::CreateTestFile(directory, "malformed.shader", "{");
        ASSERT_TRUE(file);
        AZ::RPI::ShaderSourceData descriptor;
        descriptor.m_source = "unchanged.azsl";

        const auto result = AZ::ShaderBuilder::ShaderBuilderUtility::PrepareSourceInput(AZStd::string(file->c_str()), descriptor);

        ASSERT_FALSE(result.IsSuccess());
        EXPECT_FALSE(result.GetError().empty());
        EXPECT_EQ(descriptor.m_source, "unchanged.azsl");
    }

    TEST_F(ShaderBuilderUtilityTests, CompilerOverloadsPreserveArgumentsAndProfiling)
    {
        AZ::Test::ScopedAutoTempDirectory directory;
        const auto source = AZ::Test::CreateTestFile(directory, "shader with spaces.azsl", "VALUE\n");
        ASSERT_TRUE(source);
        const AZStd::string sourcePath(source->c_str());
        AZ::IO::Path compiler = AZ::IO::Path(AZ::Test::GetCurrentExecutablePath()) / "azslc";
#if defined(AZ_PLATFORM_WINDOWS)
        compiler.ReplaceExtension(".exe");
#endif
        AZ::SerializeContext serializeContext;
        AZ::JsonRegistrationContext jsonContext;
        AZ::JsonSystemComponent::Reflect(&jsonContext);
        AZ::RHI::ShaderCompilerProfiling::Reflect(&serializeContext);
        testing::NiceMock<MockComponentApplication> application;
        ON_CALL(application, GetSerializeContext()).WillByDefault(testing::Return(&serializeContext));
        ON_CALL(application, GetJsonRegistrationContext()).WillByDefault(testing::Return(&jsonContext));
        AZ::IO::LocalFileIO fileIO;
        auto* previousFileIO = AZ::IO::FileIOBase::GetInstance();
        AZ::IO::FileIOBase::SetInstance(nullptr);
        AZ::IO::FileIOBase::SetInstance(&fileIO);

        for (int invocation = 0; invocation < 2; ++invocation)
        {
            const AZStd::string outputPath = (directory.GetDirectoryAsPath() / AZStd::string::format("output \xc3\xa9 %d.preprocessed", invocation)).Native();
            const AZStd::vector<AZStd::string> arguments{ sourcePath, "-E", "-DVALUE=\"quoted text\"", "-o", outputPath };
            AzFramework::ProcessLauncher::ProcessLaunchInfo command;
            command.m_commandlineParameters = arguments;
            const AZStd::string parameters = command.GetCommandLineParametersAsString();
            AZStd::string expectedParameters = parameters;
            bool succeeded = false;
            if (invocation == 0)
            {
                succeeded = AZ::RHI::ExecuteShaderCompiler(compiler.Native(), parameters, sourcePath, directory.GetDirectory(), "AZSL string test");
            }
            else
            {
                succeeded = AZ::RHI::ExecuteShaderCompiler(compiler.Native(), arguments, sourcePath, directory.GetDirectory(), "AZSL argv test");
                AZStd::vector<AZStd::string> profiledArguments{ compiler.Native() };
                profiledArguments.insert(profiledArguments.end(), arguments.begin(), arguments.end());
                command.m_commandlineParameters = AZStd::move(profiledArguments);
                expectedParameters = command.GetCommandLineParametersAsString();
            }
            EXPECT_TRUE(succeeded);
            const auto output = AZ::Utils::ReadFile<AZStd::string>(outputPath);
            EXPECT_TRUE(output.IsSuccess());
            if (output.IsSuccess())
            {
                EXPECT_TRUE(output.GetValue().contains("\"quoted text\""));
            }
            const AZStd::string profilingPath = (directory.GetDirectoryAsPath() / AZStd::string::format("shader with spaces.%d.profiling", AZ::Platform::GetCurrentProcessId())).Native();
            AZ::RHI::ShaderCompilerProfiling profiling;
            const auto loaded = AZ::JsonSerializationUtils::LoadObjectFromFile(profiling, profilingPath);
            EXPECT_TRUE(loaded.IsSuccess());
            EXPECT_EQ(profiling.m_entries.size(), invocation + 1);
            if (!profiling.m_entries.empty())
            {
                EXPECT_EQ(profiling.m_entries.back().m_executablePath, compiler.Native());
                EXPECT_EQ(profiling.m_entries.back().m_parameters, expectedParameters);
                EXPECT_GE(profiling.m_entries.back().m_elapsedTimeSeconds, 0.0f);
            }
        }
        AZ::IO::FileIOBase::SetInstance(nullptr);
        AZ::IO::FileIOBase::SetInstance(previousFileIO);
        jsonContext.EnableRemoveReflection();
        AZ::JsonSystemComponent::Reflect(&jsonContext);
    }

    TEST_F(ShaderBuilderUtilityTests, DependencyProcessCapturesBothPipesAndExitStatus)
    {
        AZ::Test::ScopedAutoTempDirectory directory;
        AZStd::string source = "#warning " + AZStd::string(80000, 'x') + "\n#include <missing.azsli>\n";
        const auto file = AZ::Test::CreateTestFile(directory, "shader \xc3\xa9\xf0\x9f\x98\x80 with spaces.azsl", source);
        ASSERT_TRUE(file);
        const AZStd::string sourcePath(file->c_str());
        AZ::IO::Path compiler = AZ::IO::Path(AZ::Test::GetCurrentExecutablePath()) / "azslc";
#if defined(AZ_PLATFORM_WINDOWS)
        compiler.ReplaceExtension(".exe");
#endif
        AZStd::vector<AZStd::string> arguments{ sourcePath, "--dependencies", "--W1", "-o", "-" };
        for (size_t i = 0; i < 200; ++i)
        {
            arguments.push_back("-I" + (directory.GetDirectoryAsPath() / AZStd::string::format("include%zu", i)).Native());
        }
        const auto process = AZ::RHI::RunShaderCompiler(compiler.Native(), arguments, sourcePath, "Dependency test");
        ASSERT_TRUE(process.m_launched);
        EXPECT_FALSE(process.m_timedOut);
        EXPECT_EQ(process.m_exitCode, 1);
        EXPECT_GT(process.m_stdout.size(), 8000);
        EXPECT_GT(process.m_stderr.size(), 80000);
        EXPECT_TRUE(process.m_stderr.contains("warning #601:"));
        EXPECT_TRUE(process.m_stderr.contains("error #600:"));
        const auto report = AZ::ShaderBuilder::AzslCompiler::ParseDependencyReport(process.m_stdout, process.m_exitCode);
        ASSERT_TRUE(report.IsSuccess()) << report.GetError().c_str();
        EXPECT_EQ(report.GetValue().size(), 201);
    }

    TEST_F(ShaderBuilderUtilityTests, DependencyReportPreservesPathsAndPartialResults)
    {
        rapidjson::Document document(rapidjson::kObjectType);
        auto& allocator = document.GetAllocator();
        document.AddMember("version", 1, allocator);
        document.AddMember("complete", false, allocator);
        rapidjson::Value paths(rapidjson::kArrayType);
#ifdef _WIN32
        const char* path = "C:/folder with spaces/\xc3\xa9.azsli";
#else
        const char* path = "/folder with spaces/\xc3\xa9.azsli";
#endif
        paths.PushBack(rapidjson::Value(path, allocator), allocator);
        document.AddMember("dependencies", AZStd::move(paths), allocator);
        rapidjson::StringBuffer json;
        rapidjson::Writer<rapidjson::StringBuffer> writer(json);
        document.Accept(writer);
        auto result = AZ::ShaderBuilder::AzslCompiler::ParseDependencyReport(json.GetString(), 1);
        ASSERT_TRUE(result.IsSuccess());
        ASSERT_EQ(result.GetValue().size(), 1);
        EXPECT_EQ(result.GetValue()[0], path);
        EXPECT_FALSE(AZ::ShaderBuilder::AzslCompiler::ParseDependencyReport(json.GetString(), 0).IsSuccess());
        EXPECT_FALSE(AZ::ShaderBuilder::AzslCompiler::ParseDependencyReport(json.GetString(), 3221226505u).IsSuccess());
        AZStd::string invalidUtf8(json.GetString());
        invalidUtf8.insert(invalidUtf8.find("folder"), 1, static_cast<char>(0xff));
        EXPECT_FALSE(AZ::ShaderBuilder::AzslCompiler::ParseDependencyReport(invalidUtf8, 1).IsSuccess());
    }

    TEST_F(ShaderBuilderUtilityTests, DependencyReportRejectsMalformedOrUnsupportedData)
    {
        EXPECT_FALSE(AZ::ShaderBuilder::AzslCompiler::ParseDependencyReport({}, 0).IsSuccess());
        const char* invalid[] = {
            "", "[]", "{", R"({"version":2,"complete":true,"dependencies":[]})",
            R"({"version":1,"complete":true})", R"({"version":1,"complete":1,"dependencies":[]})",
            R"({"version":1,"complete":true,"dependencies":[17]})",
            R"({"version":1,"complete":true,"dependencies":["relative.azsli"]})",
            R"({"version":1,"complete":true,"dependencies":[""]})",
            R"({"version":1,"complete":true,"dependencies":["/header\u0000suffix"]})",
        };
        for (const char* json : invalid)
        {
            EXPECT_FALSE(AZ::ShaderBuilder::AzslCompiler::ParseDependencyReport(json, 0).IsSuccess()) << json;
        }
        EXPECT_TRUE(AZ::ShaderBuilder::AzslCompiler::ParseDependencyReport(R"({"version":1,"complete":true,"dependencies":[]})", 0).IsSuccess());
        EXPECT_FALSE(AZ::ShaderBuilder::AzslCompiler::ParseDependencyReport(R"({"version":1,"complete":true,"dependencies":[]})", 1).IsSuccess());
    }
}
