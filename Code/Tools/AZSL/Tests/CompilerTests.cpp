/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AZSL/Compiler.h>

#include <AzTest/AzTest.h>

#include <algorithm>
#include <future>
#include <string>
#include <utility>
#include <vector>

AZ_UNIT_TEST_HOOK(DEFAULT_UNIT_TEST_ENV);

namespace AZ::ShaderCompiler
{
    namespace
    {
        constexpr std::string_view SimpleShader = R"(
            float4 MainPS() : SV_Target0
            {
                return float4(1.0, 0.0, 0.0, 1.0);
            }
        )";

        constexpr std::string_view ReflectionShader = R"(
            ShaderResourceGroupSemantic SceneSemantic { FrequencyId = 0; };
            ShaderResourceGroup SceneSrg : SceneSemantic
            {
                float4 m_color;
            };

            [numthreads(1, 1, 1)]
            void MainCS(uint3 dispatchId : SV_DispatchThreadID)
            {
            }
        )";

        CompileResult Compile(
            std::string_view source,
            CompilationPhase phase = CompilationPhase::Emit,
            std::vector<ArtifactType> artifacts = {ArtifactType::Shader},
            std::string_view sourcePath = "memory.azsl")
        {
            CompileOptions options;
            options.m_phase = phase;
            options.m_artifacts = std::move(artifacts);
            return Compiler{}.Compile({source, sourcePath, std::move(options)});
        }
    } // namespace

    TEST(AZSLCompilerTests, CompilesSourceEntirelyInMemory)
    {
        const CompileResult result = Compile(SimpleShader);

        ASSERT_TRUE(result.m_succeeded) << result.m_legacyDiagnostics;
        ASSERT_EQ(result.m_artifacts.size(), 1);
        EXPECT_EQ(result.m_artifacts.front().m_type, ArtifactType::Shader);
        EXPECT_NE(result.m_artifacts.front().m_content.find("MainPS"), std::string::npos);
    }

    TEST(AZSLCompilerTests, ProducesOwnedReflectionArtifacts)
    {
        const CompileResult result = Compile(
            ReflectionShader,
            CompilationPhase::Emit,
            {ArtifactType::InputAssembler, ArtifactType::ShaderResourceGroup, ArtifactType::BindingDependencies});

        ASSERT_TRUE(result.m_succeeded) << result.m_legacyDiagnostics;
        ASSERT_EQ(result.m_artifacts.size(), 3);
        EXPECT_NE(result.m_artifacts[1].m_content.find("SceneSrg"), std::string::npos);
        EXPECT_FALSE(result.m_artifacts[0].m_content.empty());
        EXPECT_FALSE(result.m_artifacts[2].m_content.empty());
    }

    TEST(AZSLCompilerTests, FullCompilationReturnsEveryRequestedArtifactInOrder)
    {
        const std::vector<ArtifactType> requestedArtifacts{
            ArtifactType::Shader,
            ArtifactType::InputAssembler,
            ArtifactType::OutputMerger,
            ArtifactType::ShaderResourceGroup,
            ArtifactType::Options,
            ArtifactType::BindingDependencies,
        };
        const CompileResult result = Compile(ReflectionShader, CompilationPhase::Emit, requestedArtifacts);

        ASSERT_TRUE(result.m_succeeded) << result.m_legacyDiagnostics;
        ASSERT_EQ(result.m_artifacts.size(), requestedArtifacts.size());
        for (size_t index = 0; index < requestedArtifacts.size(); ++index)
        {
            EXPECT_EQ(result.m_artifacts[index].m_type, requestedArtifacts[index]);
        }
    }

    TEST(AZSLCompilerTests, ReturnsStructuredAndLegacyDiagnosticsForFailure)
    {
        const CompileResult result = Compile("void Broken(", CompilationPhase::Syntax, {}, "broken.azsl");

        EXPECT_FALSE(result.m_succeeded);
        ASSERT_FALSE(result.m_diagnostics.empty());
        EXPECT_EQ(result.m_diagnostics.front().m_severity, DiagnosticSeverity::Error);
        EXPECT_EQ(result.m_diagnostics.front().m_sourcePath, "broken.azsl");
        EXPECT_FALSE(result.m_legacyDiagnostics.empty());
    }

    TEST(AZSLCompilerTests, ReturnsWarningsAndSupportsWarningsAsErrors)
    {
        constexpr std::string_view source = R"(
            ShaderResourceGroupSemantic SceneSemantic { FrequencyId = 0; ShaderVariantFallback = 128; };
            ShaderResourceGroup SceneSrg : SceneSemantic {};
            rootconstant const int Value;
        )";

        CompileOptions warningOptions;
        warningOptions.m_phase = CompilationPhase::Semantic;
        warningOptions.m_artifacts.clear();
        CompileResult warningResult = Compiler{}.Compile({source, "warning.azsl", warningOptions});
        ASSERT_TRUE(warningResult.m_succeeded) << warningResult.m_legacyDiagnostics;
        ASSERT_FALSE(warningResult.m_diagnostics.empty());
        EXPECT_EQ(warningResult.m_diagnostics.front().m_severity, DiagnosticSeverity::Warning);

        warningOptions.m_warningsAsErrors = WarningsAsErrors::EnabledWarnings;
        const CompileResult errorResult = Compiler{}.Compile({source, "warning-error.azsl", warningOptions});
        EXPECT_FALSE(errorResult.m_succeeded);
        EXPECT_FALSE(errorResult.m_legacyDiagnostics.empty());
    }

    TEST(AZSLCompilerTests, SupportsDeveloperArtifacts)
    {
        const CompileResult syntaxTree = Compile(
            "void Function() {}",
            CompilationPhase::Syntax,
            {ArtifactType::SyntaxTree});
        ASSERT_TRUE(syntaxTree.m_succeeded) << syntaxTree.m_legacyDiagnostics;
        ASSERT_EQ(syntaxTree.m_artifacts.size(), 1);
        EXPECT_NE(syntaxTree.m_artifacts.front().m_content.find("compilationUnit"), std::string::npos);

        const CompileResult symbols = Compile(
            "static const int Value = 1;",
            CompilationPhase::Semantic,
            {ArtifactType::Symbols});
        ASSERT_TRUE(symbols.m_succeeded) << symbols.m_legacyDiagnostics;
        ASSERT_EQ(symbols.m_artifacts.size(), 1);
        EXPECT_NE(symbols.m_artifacts.front().m_content.find("Value"), std::string::npos);
    }

    TEST(AZSLCompilerTests, SequentialCompilationsKeepLineMappingsAndDiagnosticsIsolated)
    {
        const CompileResult first = Compile(
            "#line 42 \"virtual.azsl\"\nvoid Broken(",
            CompilationPhase::Syntax,
            {},
            "first.azsl");
        ASSERT_FALSE(first.m_succeeded);
        ASSERT_FALSE(first.m_diagnostics.empty());
        EXPECT_EQ(first.m_diagnostics.front().m_sourcePath, "virtual.azsl");

        const CompileResult second = Compile("", CompilationPhase::Syntax, {}, "second.azsl");
        EXPECT_TRUE(second.m_succeeded) << second.m_legacyDiagnostics;
        EXPECT_TRUE(second.m_diagnostics.empty());
        EXPECT_TRUE(second.m_legacyDiagnostics.empty());
    }

    TEST(AZSLCompilerTests, IndependentCompilationsCanRunConcurrently)
    {
        std::vector<std::future<CompileResult>> compilations;
        for (size_t index = 0; index < 8; ++index)
        {
            compilations.push_back(std::async(
                std::launch::async,
                [index]
                {
                    return Compile(
                        SimpleShader,
                        CompilationPhase::Emit,
                        {ArtifactType::Shader},
                        "thread-" + std::to_string(index) + ".azsl");
                }));
        }

        for (std::future<CompileResult>& compilation : compilations)
        {
            const CompileResult result = compilation.get();
            EXPECT_TRUE(result.m_succeeded) << result.m_legacyDiagnostics;
            EXPECT_EQ(result.m_artifacts.size(), 1);
        }
    }

    TEST(AZSLCompilerTests, BuiltInPlatformEmittersAreExplicitlyRetainedAndRegistered)
    {
        const std::vector<std::string> emitters = Compiler::GetRegisteredPlatformEmitters();

        EXPECT_NE(std::find(emitters.begin(), emitters.end(), "dx"), emitters.end());
        EXPECT_NE(std::find(emitters.begin(), emitters.end(), "vk"), emitters.end());
        EXPECT_NE(std::find(emitters.begin(), emitters.end(), "mt"), emitters.end());
    }
} // namespace AZ::ShaderCompiler
