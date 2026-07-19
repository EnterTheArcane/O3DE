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
#include <AzCore/std/time.h>

#include <Common/ShaderBuilderTestFixture.h>

#include <Atom/RPI.Reflect/Shader/ShaderOptionGroup.h>

#include <Slang/SlangBackend.h>
#include <Slang/SlangCompilerService.h>
#include <Slang/SlangModuleClosure.h>
#include <Slang/SlangReflectionWalker.h>

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
[AtomShaderResourceGroup(2)]
struct TestShaderResourceGroupLayout
{
    Texture2D<Vector4F> m_texture;
    SamplerState m_sampler;
    RWStructuredBuffer<Vector4F> m_output;
    Vector4F m_color;
    f32 m_scale;
};
ParameterBlock<TestShaderResourceGroupLayout> TestSrg;

[numthreads(8, 1, 1)]
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

    TEST_F(SlangBackendTests, ReflectionWalker_PrivateShaderResourceGroup_ProducesRuntimeLayouts)
    {
        ASSERT_TRUE(AZ::Test::CreateTestFile(*m_tempDirectory, "TestShader.slang", PrivateShaderResourceGroupSource));
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

        auto reflectionOutcome = SlangReflectionWalker::BuildReflectionData(
            compilation.m_linkedProgram, RHI::ShaderTargetFormat::Spirv, compilation.m_entryPointNames);
        ASSERT_TRUE(reflectionOutcome.IsSuccess()) << reflectionOutcome.GetError().c_str();
        const ShaderReflectionData reflectionData = reflectionOutcome.TakeValue();

        // One SRG with the authored members sorted into their categories
        ASSERT_EQ(reflectionData.m_shaderResourceGroups.size(), 1);
        const ShaderResourceGroupReflection& srgReflection = reflectionData.m_shaderResourceGroups[0];
        EXPECT_EQ(srgReflection.m_name, Name{"TestSrg"});
        EXPECT_EQ(srgReflection.m_bindingSlot, 2);
        ASSERT_EQ(srgReflection.m_images.size(), 1);
        EXPECT_EQ(srgReflection.m_images[0].m_name, Name{"m_texture"});
        ASSERT_EQ(srgReflection.m_samplers.size(), 1);
        ASSERT_EQ(srgReflection.m_buffers.size(), 1);
        EXPECT_EQ(srgReflection.m_buffers[0].m_type, RHI::ShaderInputBufferType::Structured);
        ASSERT_EQ(srgReflection.m_constants.size(), 2);
        EXPECT_EQ(srgReflection.m_constants[0].m_name, Name{"m_color"});
        EXPECT_EQ(srgReflection.m_constants[0].m_constantByteOffset, 0);
        EXPECT_EQ(srgReflection.m_constants[0].m_constantByteCount, 16);
        EXPECT_EQ(srgReflection.m_constants[1].m_name, Name{"m_scale"});
        EXPECT_EQ(srgReflection.m_constants[1].m_constantByteOffset, 16);

        // The compute entry carries its numthreads attribute for the runtime dispatch queries
        ASSERT_EQ(reflectionData.m_functions.size(), 1);
        ASSERT_EQ(reflectionData.m_functions[0].m_attributes.size(), 1);
        EXPECT_EQ(reflectionData.m_functions[0].m_attributes[0].m_name, Name{"numthreads"});
        EXPECT_THAT(
            reflectionData.m_functions[0].m_attributes[0].m_arguments,
            ::testing::ElementsAre(
                ShaderFunctionAttributeArgument{int32_t{8}},
                ShaderFunctionAttributeArgument{int32_t{1}},
                ShaderFunctionAttributeArgument{int32_t{1}}));

        // The shared converters accept the walked reflection: layouts build and finalize
        auto layoutsOutcome = BuildShaderResourceGroupLayouts(reflectionData);
        ASSERT_TRUE(layoutsOutcome.IsSuccess());
        ASSERT_EQ(layoutsOutcome.GetValue().size(), 1);
        EXPECT_TRUE(layoutsOutcome.GetValue()[0]->Finalize());
        auto optionLayout = BuildShaderOptionGroupLayout(reflectionData);
        ASSERT_NE(optionLayout, nullptr);
        EXPECT_TRUE(optionLayout->FindShaderOptionIndex(Name{"DefaultOption"}).IsValid());
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

    TEST_F(SlangBackendTests, CompileProgram_AtomOptions_DynamicFallbackEndToEnd)
    {
        // The full production options surface: the force-included prelude carries the attribute
        // vocabulary, options are an interface of [AtomOption]-attributed requirements with an
        // [AtomOptions] extern struct, and the backend discovers them, generates the
        // dynamic-fallback implementation struct and composes it into the link.
        constexpr AZStd::string_view optionsShaderSource = R"(
[AtomShaderResourceGroup(0)]
public struct OptionsTestShaderResourceGroupLayout
{
    RWStructuredBuffer<Vector4F> m_output;
    Vector4F m_color;

    [AtomVariantFallback]
    public Vector4U m_shaderVariantKey;
};
public ParameterBlock<OptionsTestShaderResourceGroupLayout> OptionsSrg;

public enum QualityT
{
    Low,
    Medium,
    High,
}

public interface IOptions
{
    [AtomOption(true)]
    static bool o_useTint();

    [AtomOption(QualityT.Medium)]
    static QualityT o_quality();

    [AtomOption(4)] [AtomOptionRange(1, 8)]
    static i32 o_iterations();
}

[AtomOptions]
public extern struct Options : IOptions;

[numthreads(1, 1, 1)]
void MainCS(u32 index : SV_DispatchThreadID)
{
    Vector4F value = OptionsSrg.m_color;
    if (Options.o_useTint())
    {
        value *= 0.5;
    }
    for (i32 i = 0; i < Options.o_iterations(); ++i)
    {
        value.y += 0.125;
    }
    if (Options.o_quality() == QualityT.High)
    {
        value.z = 0.0;
    }
    OptionsSrg.m_output[index] = value;
}
)";
        ASSERT_TRUE(AZ::Test::CreateTestFile(*m_tempDirectory, "TestShader.slang", optionsShaderSource));
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
        for (const RHI::ShaderTargetDescriptor& targetDescriptor : {MakeSpirvTarget(), MakeDxilTarget()})
        {
            auto compilationOutcome = backend.CompileProgram(targetDescriptor, request);
            ASSERT_TRUE(compilationOutcome.IsSuccess()) << compilationOutcome.GetError().c_str();
            const SlangBackend::ProgramCompilation compilation = compilationOutcome.TakeValue();

            // Discovery found the three options in declaration order and the fallback designation
            ASSERT_NE(compilation.m_shaderOptionLayout, nullptr);
            ASSERT_EQ(compilation.m_shaderOptionLayout->GetShaderOptions().size(), 3);
            EXPECT_EQ(compilation.m_shaderOptionLayout->GetShaderOptions()[0].GetName(), Name{"o_useTint"});
            EXPECT_EQ(compilation.m_shaderOptionLayout->GetShaderOptions()[1].GetName(), Name{"o_quality"});
            EXPECT_EQ(compilation.m_shaderOptionLayout->GetShaderOptions()[2].GetName(), Name{"o_iterations"});
            EXPECT_EQ(compilation.m_discoveredOptions.m_fallbackShaderResourceGroupName, "OptionsSrg");
            EXPECT_EQ(compilation.m_discoveredOptions.m_fallbackMemberName, "m_shaderVariantKey");
            EXPECT_NE(compilation.m_optionsImplementationModule, nullptr);

            // The linked program generates bytecode with the composed accessor implementations
            Slang::ComPtr<slang::IBlob> bytecode;
            Slang::ComPtr<slang::IBlob> diagnostics;
            const SlangResult codeResult = compilation.m_linkedProgram->getEntryPointCode(0, 0, bytecode.writeRef(), diagnostics.writeRef());
            SlangCompilerService::ReportDiagnostics(request.m_sourcePath, diagnostics, SLANG_FAILED(codeResult));
            ASSERT_TRUE(SLANG_SUCCEEDED(codeResult));
            ASSERT_NE(bytecode, nullptr);
            EXPECT_GT(bytecode->getBufferSize(), 0);

            // The walked reflection lists the fallback member among the SRG constants, where
            // CompileFrontend validates and applies the designation
            auto reflectionOutcome = SlangReflectionWalker::BuildReflectionData(
                compilation.m_linkedProgram, targetDescriptor.m_format, compilation.m_entryPointNames);
            ASSERT_TRUE(reflectionOutcome.IsSuccess()) << reflectionOutcome.GetError().c_str();
            const ShaderReflectionData reflectionData = reflectionOutcome.TakeValue();
            ASSERT_EQ(reflectionData.m_shaderResourceGroups.size(), 1);
            EXPECT_TRUE(AZStd::any_of(
                reflectionData.m_shaderResourceGroups[0].m_constants.begin(),
                reflectionData.m_shaderResourceGroups[0].m_constants.end(),
                [](const RHI::ShaderInputConstantDescriptor& constant)
                {
                    return constant.m_name == Name{"m_shaderVariantKey"};
                }));

            // Specialization-constant lowering compiles on the same target, and the walker skips
            // the specialization parameters (Spirv) rather than misreading them as bindings
            SlangBackend::ProgramCompileRequest specializationRequest = request;
            specializationRequest.m_optionsLoweringMode = ShaderOptionLoweringMode::SpecializationConstant;
            auto specializationOutcome = backend.CompileProgram(targetDescriptor, specializationRequest);
            ASSERT_TRUE(specializationOutcome.IsSuccess()) << specializationOutcome.GetError().c_str();
            const SlangBackend::ProgramCompilation specializationCompilation = specializationOutcome.TakeValue();

            Slang::ComPtr<slang::IBlob> specializationByteCode;
            diagnostics = nullptr;
            const SlangResult specializationCodeResult = specializationCompilation.m_linkedProgram->getEntryPointCode(
                0, 0, specializationByteCode.writeRef(), diagnostics.writeRef());
            SlangCompilerService::ReportDiagnostics(request.m_sourcePath, diagnostics, SLANG_FAILED(specializationCodeResult));
            ASSERT_TRUE(SLANG_SUCCEEDED(specializationCodeResult));
            EXPECT_GT(specializationByteCode->getBufferSize(), 0);

            auto specializationReflectionOutcome = SlangReflectionWalker::BuildReflectionData(
                specializationCompilation.m_linkedProgram, targetDescriptor.m_format, specializationCompilation.m_entryPointNames);
            ASSERT_TRUE(specializationReflectionOutcome.IsSuccess()) << specializationReflectionOutcome.GetError().c_str();
            EXPECT_EQ(specializationReflectionOutcome.GetValue().m_shaderResourceGroups.size(), 1);
        }
    }

    TEST_F(SlangBackendTests, CompileProgramFromClosure_VariantRelink_MatchesSourceCompile)
    {
        // M12: variant builds relink from the frontend's serialized module closure instead of
        // re-running the source frontend. The relinked bytecode must be byte-identical to a
        // source recompile with the same option values, partial variants must mix baked and
        // dynamic accessors, and a bundle from a different compiler must be rejected.
        constexpr AZStd::string_view optionsShaderSource = R"(
[AtomShaderResourceGroup(0)]
public struct ClosureTestShaderResourceGroupLayout
{
    RWStructuredBuffer<Vector4F> m_output;
    Vector4F m_color;

    [AtomVariantFallback]
    public Vector4U m_shaderVariantKey;
};
public ParameterBlock<ClosureTestShaderResourceGroupLayout> OptionsSrg;

public enum QualityT
{
    Low,
    Medium,
    High,
}

public interface IOptions
{
    [AtomOption(true)]
    static bool o_useTint();

    [AtomOption(QualityT.Medium)]
    static QualityT o_quality();

    [AtomOption(4)] [AtomOptionRange(1, 8)]
    static i32 o_iterations();
}

[AtomOptions]
public extern struct Options : IOptions;

[numthreads(1, 1, 1)]
void MainCS(u32 index : SV_DispatchThreadID)
{
    Vector4F value = OptionsSrg.m_color;
    if (Options.o_useTint())
    {
        value *= 0.5;
    }
    for (i32 i = 0; i < Options.o_iterations(); ++i)
    {
        value.y += 0.125;
    }
    if (Options.o_quality() == QualityT.High)
    {
        value.z = 0.0;
    }
    OptionsSrg.m_output[index] = value;
}
)";
        ASSERT_TRUE(AZ::Test::CreateTestFile(*m_tempDirectory, "ClosureTestShader.slang", optionsShaderSource));
        AZ::IO::FixedMaxPath sourcePath(m_tempDirectory->GetDirectory());
        sourcePath /= "ClosureTestShader.slang";

        const AZStd::vector<AZStd::string> includePaths = GetShaderLibIncludePaths();
        const MapOfStringToStageType entryPoints = {
            {"MainCS", RPI::ShaderStageType::Compute},
        };

        SlangBackend::ProgramCompileRequest request;
        request.m_sourcePath = sourcePath.Native();
        request.m_entryPoints = &entryPoints;
        request.m_includePaths = includePaths;

        SlangBackend backend;
        SlangCompilerService& service = SlangCompilerService::Get();
        auto compilerLock = service.AcquireCompilerLock();
        const RHI::ShaderTargetDescriptor targetDescriptor = MakeSpirvTarget();

        // The frontend-equivalent compile, and the bundle exactly as CompileFrontend builds it
        // (the generated options module excluded)
        auto frontendOutcome = backend.CompileProgram(targetDescriptor, request);
        ASSERT_TRUE(frontendOutcome.IsSuccess()) << frontendOutcome.GetError().c_str();
        const SlangBackend::ProgramCompilation frontendCompilation = frontendOutcome.TakeValue();
        ASSERT_NE(frontendCompilation.m_shaderOptionLayout, nullptr);

        static constexpr AZStd::string_view excludedModuleNames[] = {"AtomGeneratedOptions"};
        auto bundleOutcome = BuildModuleClosureBundle(
            frontendCompilation.m_session,
            service.GetCompilerBuildTag(),
            static_cast<uint32_t>(targetDescriptor.m_format),
            frontendCompilation.m_module->getName(),
            excludedModuleNames);
        ASSERT_TRUE(bundleOutcome.IsSuccess()) << bundleOutcome.GetError().c_str();
        const SlangModuleClosureBundle bundle = bundleOutcome.TakeValue();
        EXPECT_EQ(bundle.m_rootModuleName, "ClosureTestShader");
        for (const SlangModuleClosureBundle::Module& bundleModule : bundle.m_modules)
        {
            EXPECT_NE(bundleModule.m_name, "AtomGeneratedOptions");
        }

        auto getByteCode = [&](const SlangBackend::ProgramCompilation& compilation, AZStd::vector<uint8_t>& outByteCode)
        {
            Slang::ComPtr<slang::IBlob> byteCode;
            Slang::ComPtr<slang::IBlob> diagnostics;
            const SlangResult codeResult = compilation.m_linkedProgram->getEntryPointCode(0, 0, byteCode.writeRef(), diagnostics.writeRef());
            SlangCompilerService::ReportDiagnostics(request.m_sourcePath, diagnostics, SLANG_FAILED(codeResult));
            ASSERT_TRUE(SLANG_SUCCEEDED(codeResult));
            ASSERT_NE(byteCode, nullptr);
            const uint8_t* bytes = static_cast<const uint8_t*>(byteCode->getBufferPointer());
            outByteCode.assign(bytes, bytes + byteCode->getBufferSize());
        };

        // Fully specified variant: closure relink == source recompile, byte for byte
        RPI::ShaderOptionGroup bakedValues(frontendCompilation.m_shaderOptionLayout);
        bakedValues.SetValue(Name{"o_useTint"}, Name{"true"});
        bakedValues.SetValue(Name{"o_quality"}, Name{"High"});
        bakedValues.SetValue(Name{"o_iterations"}, Name{"7"});

        SlangBackend::ProgramCompileRequest bakedRequest = request;
        bakedRequest.m_optionsLoweringMode = ShaderOptionLoweringMode::Baked;
        bakedRequest.m_bakedOptionValues = &bakedValues;

        auto closureOutcome = backend.CompileProgramFromClosure(targetDescriptor, bakedRequest, bundle);
        ASSERT_TRUE(closureOutcome.IsSuccess()) << closureOutcome.GetError().c_str();
        AZStd::vector<uint8_t> closureByteCode;
        getByteCode(closureOutcome.GetValue(), closureByteCode);

        auto sourceOutcome = backend.CompileProgram(targetDescriptor, bakedRequest);
        ASSERT_TRUE(sourceOutcome.IsSuccess()) << sourceOutcome.GetError().c_str();
        AZStd::vector<uint8_t> sourceByteCode;
        getByteCode(sourceOutcome.GetValue(), sourceByteCode);

        ASSERT_FALSE(closureByteCode.empty());
        EXPECT_EQ(closureByteCode, sourceByteCode) << "closure-relinked bytecode diverged from the source recompile";

        // Partially specified variant: pinned options bake, the rest read the fallback key —
        // and the result differs from the fully baked variant (the runtime branches remain)
        RPI::ShaderOptionGroup partialValues(frontendCompilation.m_shaderOptionLayout);
        partialValues.SetValue(Name{"o_useTint"}, Name{"true"});
        SlangBackend::ProgramCompileRequest partialRequest = request;
        partialRequest.m_optionsLoweringMode = ShaderOptionLoweringMode::Baked;
        partialRequest.m_bakedOptionValues = &partialValues;

        auto partialOutcome = backend.CompileProgramFromClosure(targetDescriptor, partialRequest, bundle);
        ASSERT_TRUE(partialOutcome.IsSuccess()) << partialOutcome.GetError().c_str();
        AZStd::vector<uint8_t> partialByteCode;
        getByteCode(partialOutcome.GetValue(), partialByteCode);
        ASSERT_FALSE(partialByteCode.empty());
        EXPECT_NE(partialByteCode, closureByteCode);

        // A bundle from another compiler build must be rejected (the caller then falls back to
        // the source recompile)
        SlangModuleClosureBundle tamperedBundle = bundle;
        tamperedBundle.m_compilerBuildTag = "some-other-compiler";
        EXPECT_FALSE(backend.CompileProgramFromClosure(targetDescriptor, bakedRequest, tamperedBundle).IsSuccess());

        // Record the relink-vs-recompile cost on this shader (plan M12 asks for the numbers
        // before claiming wins; not asserted — machine-dependent)
        constexpr int benchmarkIterations = 5;
        const AZStd::sys_time_t frequency = AZStd::GetTimeTicksPerSecond();
        AZStd::sys_time_t closureTicks = 0;
        AZStd::sys_time_t sourceTicks = 0;
        for (int i = 0; i < benchmarkIterations; ++i)
        {
            AZStd::sys_time_t start = AZStd::GetTimeNowTicks();
            auto timedClosure = backend.CompileProgramFromClosure(targetDescriptor, bakedRequest, bundle);
            ASSERT_TRUE(timedClosure.IsSuccess());
            AZStd::vector<uint8_t> timedByteCode;
            getByteCode(timedClosure.GetValue(), timedByteCode);
            closureTicks += AZStd::GetTimeNowTicks() - start;

            start = AZStd::GetTimeNowTicks();
            auto timedSource = backend.CompileProgram(targetDescriptor, bakedRequest);
            ASSERT_TRUE(timedSource.IsSuccess());
            getByteCode(timedSource.GetValue(), timedByteCode);
            sourceTicks += AZStd::GetTimeNowTicks() - start;
        }
        printf(
            "Variant compile benchmark (%d iterations): closure relink %.2f ms/variant, source recompile %.2f ms/variant\n",
            benchmarkIterations,
            1000.0 * static_cast<double>(closureTicks) / static_cast<double>(benchmarkIterations) / static_cast<double>(frequency),
            1000.0 * static_cast<double>(sourceTicks) / static_cast<double>(benchmarkIterations) / static_cast<double>(frequency));
    }

    TEST_F(SlangBackendTests, SupervariantDefinitions_ChangeCodeNotLayout_AndGuardedOptionsDivergeTheHash)
    {
        // M13: supervariants reach the Slang session as preprocessor macro deltas through the
        // build-argument scope stack (.shader "Definitions" roll into -D preprocessor
        // arguments). Two contracts matter:
        // 1. A definition that changes code must specialize bytecode WITHOUT touching the
        //    option layout — supervariants share one ShaderOptionGroupLayout by design.
        // 2. A definition that adds/removes option declarations diverges the layout hash —
        //    exactly the signal ShaderAssetBuilder's cross-supervariant equality check
        //    rejects, in either language.
        constexpr AZStd::string_view supervariantShaderSource = R"(
[AtomShaderResourceGroup(0)]
public struct SupervariantTestShaderResourceGroupLayout
{
    RWStructuredBuffer<Vector4F> m_output;
    Vector4F m_color;

    [AtomVariantFallback]
    public Vector4U m_shaderVariantKey;
};
public ParameterBlock<SupervariantTestShaderResourceGroupLayout> OptionsSrg;

#ifndef SLANGTEST_TINT_STRENGTH
#define SLANGTEST_TINT_STRENGTH 1
#endif

public interface IOptions
{
    [AtomOption(true)]
    static bool o_useTint();

#ifdef SLANGTEST_EXTRA_OPTION
    [AtomOption(2)] [AtomOptionRange(1, 4)]
    static i32 o_extraBands();
#endif
}

[AtomOptions]
public extern struct Options : IOptions;

[numthreads(1, 1, 1)]
void MainCS(u32 index : SV_DispatchThreadID)
{
    Vector4F value = OptionsSrg.m_color;
    if (Options.o_useTint())
    {
        value *= 0.25 * f32(SLANGTEST_TINT_STRENGTH);
    }
#ifdef SLANGTEST_EXTRA_OPTION
    value.y += f32(Options.o_extraBands());
#endif
    OptionsSrg.m_output[index] = value;
}
)";
        ASSERT_TRUE(AZ::Test::CreateTestFile(*m_tempDirectory, "SupervariantTestShader.slang", supervariantShaderSource));
        AZ::IO::FixedMaxPath sourcePath(m_tempDirectory->GetDirectory());
        sourcePath /= "SupervariantTestShader.slang";

        const AZStd::vector<AZStd::string> includePaths = GetShaderLibIncludePaths();
        const MapOfStringToStageType entryPoints = {
            {"MainCS", RPI::ShaderStageType::Compute},
        };

        SlangBackend backend;
        auto compilerLock = SlangCompilerService::Get().AcquireCompilerLock();
        const RHI::ShaderTargetDescriptor targetDescriptor = MakeSpirvTarget();

        struct CompiledSupervariant
        {
            HashValue64 m_layoutHash;
            size_t m_optionCount;
            AZStd::vector<uint8_t> m_byteCode;
        };
        auto compileWithDefinitions = [&](const AZStd::vector<AZStd::string>& preprocessorArguments) -> CompiledSupervariant
        {
            RHI::ShaderBuildArguments buildArguments;
            buildArguments.m_preprocessorArguments = preprocessorArguments;

            SlangBackend::ProgramCompileRequest request;
            request.m_sourcePath = sourcePath.Native();
            request.m_entryPoints = &entryPoints;
            request.m_includePaths = includePaths;
            request.m_buildArguments = &buildArguments;

            auto compilationOutcome = backend.CompileProgram(targetDescriptor, request);
            if (!compilationOutcome.IsSuccess())
            {
                ADD_FAILURE() << compilationOutcome.GetError().c_str();
                return {};
            }
            const SlangBackend::ProgramCompilation compilation = compilationOutcome.TakeValue();
            if (!compilation.m_shaderOptionLayout)
            {
                ADD_FAILURE() << "no option layout was discovered";
                return {};
            }

            CompiledSupervariant result;
            result.m_layoutHash = compilation.m_shaderOptionLayout->GetHash();
            result.m_optionCount = compilation.m_shaderOptionLayout->GetShaderOptions().size();

            Slang::ComPtr<slang::IBlob> byteCode;
            Slang::ComPtr<slang::IBlob> diagnostics;
            const SlangResult codeResult = compilation.m_linkedProgram->getEntryPointCode(0, 0, byteCode.writeRef(), diagnostics.writeRef());
            SlangCompilerService::ReportDiagnostics(request.m_sourcePath, diagnostics, SLANG_FAILED(codeResult));
            if (SLANG_FAILED(codeResult) || !byteCode)
            {
                ADD_FAILURE() << "failed to generate bytecode";
                return {};
            }
            const uint8_t* bytes = static_cast<const uint8_t*>(byteCode->getBufferPointer());
            result.m_byteCode.assign(bytes, bytes + byteCode->getBufferSize());
            return result;
        };

        const CompiledSupervariant defaultSupervariant = compileWithDefinitions({});
        const CompiledSupervariant codeDeltaSupervariant = compileWithDefinitions({"-DSLANGTEST_TINT_STRENGTH=3"});
        const CompiledSupervariant optionDeltaSupervariant = compileWithDefinitions({"-DSLANGTEST_EXTRA_OPTION"});
        ASSERT_FALSE(defaultSupervariant.m_byteCode.empty());
        ASSERT_FALSE(codeDeltaSupervariant.m_byteCode.empty());
        ASSERT_FALSE(optionDeltaSupervariant.m_byteCode.empty());

        // Contract 1: code-only definition — same layout, different bytecode
        EXPECT_EQ(defaultSupervariant.m_optionCount, 1);
        EXPECT_EQ(codeDeltaSupervariant.m_layoutHash, defaultSupervariant.m_layoutHash);
        EXPECT_NE(codeDeltaSupervariant.m_byteCode, defaultSupervariant.m_byteCode);

        // Contract 2: an option guarded by the definition diverges the layout hash — the
        // signal the builder's cross-supervariant equality check rejects
        EXPECT_EQ(optionDeltaSupervariant.m_optionCount, 2);
        EXPECT_NE(optionDeltaSupervariant.m_layoutHash, defaultSupervariant.m_layoutHash);
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
