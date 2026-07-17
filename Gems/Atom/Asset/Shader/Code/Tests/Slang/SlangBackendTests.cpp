/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// M7 (SlangIntegrationPlan.md, Phase 1B): the Slang language backend.
//
// Verifies the frontend pipeline end-to-end against the real ShaderLib assets: a private-SRG
// shader authored in the ParameterBlock idiom, with the builder-injected preludes, compiles and
// links to bytecode for both PC targets (DXIL and SPIR-V), and the resolver-driven dependency
// scanner reports the prelude modules and transitive imports CreateJobs must register.

#include <AzTest/AzTest.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzCore/IO/Path/Path.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/Utils/Utils.h>

#include <Common/ShaderBuilderTestFixture.h>

#include <Slang/SlangBackend.h>
#include <Slang/SlangCompilerService.h>

namespace UnitTest
{
    using namespace AZ;
    using namespace AZ::ShaderBuilder;

    class SlangBackendTests : public ShaderBuilderTestFixture
    {
    public:
        void SetUp() override
        {
            ShaderBuilderTestFixture::SetUp();
            m_tempDirectory = AZStd::make_unique<AZ::Test::ScopedAutoTempDirectory>();
        }

        void TearDown() override
        {
            m_tempDirectory.reset();
            ShaderBuilderTestFixture::TearDown();
        }

        //! The test binary lives at <engine root>/build/<build dir>/bin/<config>; the ShaderLib
        //! assets the builder injects resolve from the engine source tree.
        static AZ::IO::FixedMaxPath GetEngineRoot()
        {
            AZ::IO::FixedMaxPath engineRoot = AZ::Utils::GetExecutableDirectory();
            for (int i = 0; i < 4; ++i)
            {
                engineRoot = engineRoot.ParentPath();
            }
            return engineRoot;
        }

        static AZStd::vector<AZStd::string> GetShaderLibIncludePaths()
        {
            const AZ::IO::FixedMaxPath engineRoot = GetEngineRoot();
            AZStd::vector<AZStd::string> includePaths;
            for (const char* shaderLibRelativePath : {
                "Gems/Atom/RPI/Assets/ShaderLib",
                "Gems/Atom/Feature/Common/Assets/ShaderLib",
            })
            {
                AZ::IO::FixedMaxPath shaderLibPath = engineRoot;
                shaderLibPath /= shaderLibRelativePath;
                includePaths.push_back(shaderLibPath.String());
            }
            return includePaths;
        }

        //! A private-SRG shader in the ParameterBlock authoring idiom, using the prelude aliases.
        //! The builder injects the Prelude and ShaderResourceGroup imports.
        static constexpr AZStd::string_view PrivateShaderResourceGroupSource = R"(
struct TestShaderResourceGroupLayout
{
    Texture2D<Vector4F> m_texture;
    SamplerState m_sampler;
    RWStructuredBuffer<Vector4F> m_output;
    Vector4F m_color;
    f32 m_scale;
};
ParameterBlock<TestShaderResourceGroupLayout> TestSrg;

void MainCS(u32 index : SV_DispatchThreadID)
{
    TestSrg.m_output[index] =
        TestSrg.m_texture.SampleLevel(TestSrg.m_sampler, Vector2F(0, 0), 0) * TestSrg.m_scale + TestSrg.m_color;
}
)";

        static RHI::ShaderTargetDescriptor MakeDxilTarget()
        {
            RHI::ShaderTargetDescriptor descriptor;
            descriptor.m_format = RHI::ShaderTargetFormat::Dxil;
            descriptor.m_conventions.m_enable16BitTypes = true;
            return descriptor;
        }

        static RHI::ShaderTargetDescriptor MakeSpirvTarget()
        {
            RHI::ShaderTargetDescriptor descriptor;
            descriptor.m_format = RHI::ShaderTargetFormat::Spirv;
            descriptor.m_conventions.m_invertY = true;
            descriptor.m_conventions.m_useDxPositionW = true;
            descriptor.m_conventions.m_uniqueBindingIndicesPerSet = true;
            return descriptor;
        }

        //! Writes @source to the temp directory and compiles it for @targetDescriptor through
        //! the full backend pipeline, returning the bytecode of the single compute entry.
        AZStd::vector<uint8_t> CompilePrivateShaderResourceGroupShader(const RHI::ShaderTargetDescriptor& targetDescriptor)
        {
            if (!AZ::Test::CreateTestFile(*m_tempDirectory, "TestShader.slang", PrivateShaderResourceGroupSource))
            {
                ADD_FAILURE() << "failed to write shader source";
                return {};
            }
            AZ::IO::FixedMaxPath sourcePath(m_tempDirectory->GetDirectory());
            sourcePath /= "TestShader.slang";

            const AZStd::vector<AZStd::string> includePaths = GetShaderLibIncludePaths();
            const MapOfStringToStageType entryPoints = {
                {"MainCS", RPI::ShaderStageType::Compute},
            };

            SlangBackend::ProgramCompileRequest request;
            request.m_sourcePath = sourcePath.Native();
            request.m_entryPoints = &entryPoints;
            request.m_includePaths = includePaths;

            SlangBackend backend;
            auto compilerLock = SlangCompilerService::Get().AcquireCompilerLock();
            auto compilationOutcome = backend.CompileProgram(targetDescriptor, request);
            if (!compilationOutcome.IsSuccess())
            {
                ADD_FAILURE() << compilationOutcome.GetError().c_str();
                return {};
            }
            const SlangBackend::ProgramCompilation compilation = compilationOutcome.TakeValue();

            Slang::ComPtr<slang::IBlob> bytecode;
            Slang::ComPtr<slang::IBlob> diagnostics;
            const SlangResult codeResult = compilation.m_linkedProgram->getEntryPointCode(0, 0, bytecode.writeRef(), diagnostics.writeRef());
            SlangCompilerService::ReportDiagnostics(request.m_sourcePath, diagnostics, SLANG_FAILED(codeResult));
            if (SLANG_FAILED(codeResult) || !bytecode)
            {
                ADD_FAILURE() << "failed to generate entry point code";
                return {};
            }

            const uint8_t* bytes = static_cast<const uint8_t*>(bytecode->getBufferPointer());
            return AZStd::vector<uint8_t>(bytes, bytes + bytecode->getBufferSize());
        }

        AZStd::unique_ptr<AZ::Test::ScopedAutoTempDirectory> m_tempDirectory;
    };

    TEST_F(SlangBackendTests, BackendIdentity_NameExtensionsAndTargets)
    {
        SlangBackend backend;
        EXPECT_EQ(backend.GetName(), "slang");
        EXPECT_THAT(backend.GetSourceExtensions(), ::testing::ElementsAre(".slang"));

        EXPECT_TRUE(backend.CanCompileTarget(MakeDxilTarget()));
        EXPECT_TRUE(backend.CanCompileTarget(MakeSpirvTarget()));

        RHI::ShaderTargetDescriptor noTarget;
        EXPECT_FALSE(backend.CanCompileTarget(noTarget));
        RHI::ShaderTargetDescriptor wgslTarget;
        wgslTarget.m_format = RHI::ShaderTargetFormat::Wgsl;
        EXPECT_FALSE(backend.CanCompileTarget(wgslTarget));
    }

    TEST_F(SlangBackendTests, CompileProgram_PrivateShaderResourceGroup_Dxil)
    {
        const AZStd::vector<uint8_t> bytecode = CompilePrivateShaderResourceGroupShader(MakeDxilTarget());
        ASSERT_FALSE(bytecode.empty());
        // DXIL containers start with the 'DXBC' fourcc
        ASSERT_GE(bytecode.size(), 4);
        EXPECT_EQ(0, memcmp(bytecode.data(), "DXBC", 4));
    }

    TEST_F(SlangBackendTests, CompileProgram_PrivateShaderResourceGroup_Spirv)
    {
        const AZStd::vector<uint8_t> bytecode = CompilePrivateShaderResourceGroupShader(MakeSpirvTarget());
        ASSERT_FALSE(bytecode.empty());
        // SPIR-V modules start with the 0x07230203 magic
        ASSERT_GE(bytecode.size(), 4);
        const uint32_t magic = *reinterpret_cast<const uint32_t*>(bytecode.data());
        EXPECT_EQ(magic, 0x07230203u);
    }

    TEST_F(SlangBackendTests, CompileProgram_ImportedModuleSeesPreludeWithoutImports)
    {
        // A module using the prelude aliases and attribute vocabulary with ZERO import lines:
        // the session file system hook force-includes the prelude into every module it loads.
        constexpr AZStd::string_view importedModuleSource = R"(
public struct SharedTypes
{
    public Vector4F m_color;
    public Matrix4x4<f32> m_transform;
};

[AtomShaderResourceGroupMember("ImportTestSrg", 4)]
RWStructuredBuffer<Vector4F> m_moduleOutput : register(u7, space0);

public void WriteModuleOutput(u32 index, SharedTypes value)
{
    m_moduleOutput[index] = mul(value.m_transform, value.m_color);
}
)";
        constexpr AZStd::string_view shaderSource = R"(
import ImplicitVocabulary;

void MainCS(u32 index : SV_DispatchThreadID)
{
    SharedTypes value;
    value.m_color = Vector4F(1, 2, 3, 4);
    value.m_transform = Matrix4x4<f32>(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
    WriteModuleOutput(index, value);
}
)";
        ASSERT_TRUE(AZ::Test::CreateTestFile(*m_tempDirectory, "ImplicitVocabulary.slang", importedModuleSource));
        ASSERT_TRUE(AZ::Test::CreateTestFile(*m_tempDirectory, "TestShader.slang", shaderSource));
        AZ::IO::FixedMaxPath sourcePath(m_tempDirectory->GetDirectory());
        sourcePath /= "TestShader.slang";

        const AZStd::vector<AZStd::string> includePaths = GetShaderLibIncludePaths();
        const MapOfStringToStageType entryPoints = {
            {"MainCS", RPI::ShaderStageType::Compute},
        };

        SlangBackend::ProgramCompileRequest request;
        request.m_sourcePath = sourcePath.Native();
        request.m_entryPoints = &entryPoints;
        request.m_includePaths = includePaths;

        SlangBackend backend;
        auto compilerLock = SlangCompilerService::Get().AcquireCompilerLock();
        auto compilationOutcome = backend.CompileProgram(MakeSpirvTarget(), request);
        ASSERT_TRUE(compilationOutcome.IsSuccess()) << compilationOutcome.GetError().c_str();
        const SlangBackend::ProgramCompilation compilation = compilationOutcome.TakeValue();

        Slang::ComPtr<slang::IBlob> bytecode;
        Slang::ComPtr<slang::IBlob> diagnostics;
        const SlangResult codeResult = compilation.m_linkedProgram->getEntryPointCode(0, 0, bytecode.writeRef(), diagnostics.writeRef());
        SlangCompilerService::ReportDiagnostics(request.m_sourcePath, diagnostics, SLANG_FAILED(codeResult));
        ASSERT_TRUE(SLANG_SUCCEEDED(codeResult));
        ASSERT_NE(bytecode, nullptr);
        EXPECT_GT(bytecode->getBufferSize(), 0);
    }

    TEST_F(SlangBackendTests, EnumerateSourceDependencies_ReportsInjectedPreludesAndTransitiveImports)
    {
        // A source importing a sibling module, which itself imports the Bindless module.
        constexpr AZStd::string_view importedModuleSource = R"(
import Atom.Features.Bindless;

public Vector4F SampleBindless(u32 index)
{
    return Bindless::GetTexture2D(index).Load(Vector3<i32>(0, 0, 0));
}
)";
        constexpr AZStd::string_view shaderSource = R"(
import TestImports;

void MainCS(u32 index : SV_DispatchThreadID)
{
}
)";
        ASSERT_TRUE(AZ::Test::CreateTestFile(*m_tempDirectory, "TestImports.slang", importedModuleSource));
        ASSERT_TRUE(AZ::Test::CreateTestFile(*m_tempDirectory, "TestShader.slang", shaderSource));
        AZ::IO::FixedMaxPath sourcePath(m_tempDirectory->GetDirectory());
        sourcePath /= "TestShader.slang";

        const AZStd::vector<AZStd::string> includePaths = GetShaderLibIncludePaths();
        SlangBackend backend;
        AZStd::unordered_set<AZStd::string> sourceDependencies;
        backend.EnumerateSourceDependencies(sourcePath.Native(), includePaths, sourceDependencies);

        auto containsEnding = [&sourceDependencies](AZStd::string_view ending)
        {
            for (const AZStd::string& dependency : sourceDependencies)
            {
                AZStd::string normalized = dependency;
                AZStd::replace(normalized.begin(), normalized.end(), '\\', '/');
                if (normalized.ends_with(ending))
                {
                    return true;
                }
            }
            return false;
        };

        // The sibling import and its transitive Bindless import
        EXPECT_TRUE(containsEnding("TestImports.slang"));
        EXPECT_TRUE(containsEnding("Atom/Features/Bindless.slang"));
        // The builder-injected prelude modules
        EXPECT_TRUE(containsEnding("Atom/RPI/Prelude.slang"));
        EXPECT_TRUE(containsEnding("Atom/RPI/ShaderResourceGroup.slang"));
    }
} // namespace UnitTest
