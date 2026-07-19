/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// M10 (SlangIntegrationPlan.md, Phase 2): options productization.
//
// The builder now PRODUCES the ShaderOptionGroup packing (AZSLC parsed it) — so the packing must
// be bit-compatible with what AZSLC assigns for equivalent declarations, or ShaderVariantKey and
// every variant tree breaks. Options are authored as [AtomOption]-attributed extern functions —
// AZSL's global option declarations, as functions; the builder discovers them through
// declaration reflection and satisfies the externs with a generated implementation module
// composed at link time — every lowering mode is exercised here against a real use-site through
// the production service.

#include <AzTest/AzTest.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/Serialization/Json/JsonUtils.h>
#include <AzCore/StringFunc/StringFunc.h>
#include <AzCore/Utils/Utils.h>
#include <AzFramework/Process/ProcessCommunicator.h>
#include <AzFramework/Process/ProcessWatcher.h>
#include <Atom/RPI.Edit/Common/JsonUtils.h>

#include <Common/ShaderBuilderTestFixture.h>

#include <AzslCompiler.h>
#include <Slang/SlangCompilerService.h>
#include <Slang/SlangOptionsModuleGenerator.h>

namespace UnitTest
{
    using namespace AZ;
    using namespace AZ::ShaderBuilder;

    class SlangOptionsModuleGeneratorTests : public ShaderBuilderTestFixture
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

        //! The option set under test, in both languages: a bool, a three-value enum, and an
        //! integer range.
        static AZStd::vector<ShaderOptionDeclaration> MakeDeclarations()
        {
            AZStd::vector<ShaderOptionDeclaration> declarations;
            ShaderOptionDeclaration useTint;
            useTint.m_name = Name{"o_useTint"};
            useTint.m_type = RPI::ShaderOptionType::Boolean;
            useTint.m_typeText = "bool";
            useTint.m_defaultValue = Name{"false"};
            declarations.push_back(useTint);

            ShaderOptionDeclaration quality;
            quality.m_name = Name{"o_quality"};
            quality.m_type = RPI::ShaderOptionType::Enumeration;
            quality.m_typeText = "Quality";
            quality.m_enumValues = {Name{"Low"}, Name{"Medium"}, Name{"High"}};
            quality.m_defaultValue = Name{"Medium"};
            declarations.push_back(quality);

            ShaderOptionDeclaration sampleCount;
            sampleCount.m_name = Name{"o_sampleCount"};
            sampleCount.m_type = RPI::ShaderOptionType::IntegerRange;
            sampleCount.m_typeText = "int";
            sampleCount.m_minValue = 1;
            sampleCount.m_maxValue = 8;
            sampleCount.m_defaultValue = Name{"4"};
            declarations.push_back(sampleCount);
            return declarations;
        }

        //! The discovery result matching MakeUseSiteSource, for driving the generators directly.
        static SlangOptionsModuleGenerator::DiscoveredShaderOptions MakeDiscoveredOptions()
        {
            SlangOptionsModuleGenerator::DiscoveredShaderOptions discovered;
            discovered.m_declarations = MakeDeclarations();
            discovered.m_declaringModuleNames = {"OptionsUseSite"};
            discovered.m_fallbackShaderResourceGroupName = "DrawSrg";
            discovered.m_fallbackMemberName = "m_shaderVariantKeyFallback";
            discovered.m_fallbackDeclaringModuleName = "OptionsUseSite";
            return discovered;
        }

        static bool RunAzslCompiler(const AZStd::string& inputPath, const AZStd::string& arguments, AZStd::string& output)
        {
            AZ::IO::FixedMaxPath azslcPath = AZ::Utils::GetExecutableDirectory();
            azslcPath /= "Builders/AZSLc/azslc.exe";
            if (!AZ::IO::SystemFile::Exists(azslcPath.c_str()))
            {
                output = AZStd::string::format("azslc not found at '%s'", azslcPath.c_str());
                return false;
            }
            AzFramework::ProcessLauncher::ProcessLaunchInfo launchInfo;
            launchInfo.m_commandlineParameters = AZStd::string::format("\"%s\" \"%s\" %s", azslcPath.c_str(), inputPath.c_str(), arguments.c_str());
            launchInfo.m_showWindow = false;
            AZStd::unique_ptr<AzFramework::ProcessWatcher> watcher(AzFramework::ProcessWatcher::LaunchProcess(launchInfo, AzFramework::COMMUNICATOR_TYPE_STDINOUT));
            if (!watcher)
            {
                output = "failed to launch azslc";
                return false;
            }
            uint32_t exitCode = 0;
            while (watcher->IsProcessRunning(&exitCode))
            {
                AzFramework::ProcessCommunicator* communicator = watcher->GetCommunicator();
                if (const AZ::u32 byteCount = communicator->PeekOutput())
                {
                    AZStd::string chunk;
                    chunk.resize_no_construct(byteCount);
                    communicator->ReadOutput(chunk.data(), byteCount);
                    output += chunk;
                }
                if (const AZ::u32 byteCount = communicator->PeekError())
                {
                    AZStd::string chunk;
                    chunk.resize_no_construct(byteCount);
                    communicator->ReadError(chunk.data(), byteCount);
                    output += chunk;
                }
            }
            return exitCode == 0;
        }

        //! The use-site module in the production authoring form: the attribute vocabulary
        //! (inlined here; production gets it from the force-included prelude), the flat
        //! [AtomOption] extern functions with typed defaults — o_useTint exercising the omitted
        //! argument (defaults to 0/false) — and a fallback-designated ShaderResourceGroup; the
        //! same option set as MakeDeclarations().
        static constexpr AZStd::string_view UseSiteSource = R"(
module OptionsUseSite;

[__AttributeUsage(_AttributeTargets.Function)]
struct AtomOptionAttribute { int value = 0; };

[__AttributeUsage(_AttributeTargets.Function)]
struct AtomRangeAttribute { int min; int max; };

[__AttributeUsage(_AttributeTargets.Var)]
struct AtomVariantFallbackAttribute {};

public enum Quality
{
    Low,
    Medium,
    High,
}

[AtomOption]
public extern bool o_useTint();

[AtomOption(Quality.Medium)]
public extern Quality o_quality();

[AtomOption(4)] [AtomRange(1, 8)]
public extern int o_sampleCount();

public struct DrawShaderResourceGroup
{
    [AtomVariantFallback]
    public uint4 m_shaderVariantKeyFallback;
};
public ParameterBlock<DrawShaderResourceGroup> DrawSrg;

RWStructuredBuffer<float4> Output;

[shader("compute")]
[numthreads(1, 1, 1)]
void MainCS(uint3 id : SV_DispatchThreadID)
{
    float4 value = float4(1.0, 1.0, 1.0, 1.0);
    if (o_useTint())
    {
        value *= 0.5;
    }
    value.x += float((int)o_quality());
    value.y += float(o_sampleCount());
    Output[id.x] = value;
}
)";

        //! Composes the use-site with a generated implementation module through the production
        //! compiler service.
        static bool CompileWithImplementationModule(
            AZStd::string_view implementationModuleText,
            AZStd::vector<uint8_t>& outByteCode)
        {
            SlangCompilerService& service = SlangCompilerService::Get();
            const AZStd::unique_lock<AZStd::recursive_mutex> compilerLock = service.AcquireCompilerLock();

            SlangCompilerService::SessionDescriptor sessionDescriptor;
            sessionDescriptor.m_target = SLANG_SPIRV;
            sessionDescriptor.m_profile = "spirv_1_5";
            auto sessionOutcome = service.CreateSession(sessionDescriptor);
            if (!sessionOutcome.IsSuccess())
            {
                ADD_FAILURE() << sessionOutcome.GetError().c_str();
                return false;
            }
            Slang::ComPtr<slang::ISession> session = sessionOutcome.TakeValue();

            Slang::ComPtr<slang::IBlob> diagnostics;
            slang::IModule* useSiteModule = session->loadModuleFromSourceString(
                "OptionsUseSite", "OptionsUseSite.slang", AZStd::string(UseSiteSource).c_str(), diagnostics.writeRef());
            SlangCompilerService::ReportDiagnostics("OptionsUseSite", diagnostics, !useSiteModule);
            if (!useSiteModule)
            {
                return false;
            }

            Slang::ComPtr<slang::IEntryPoint> entryPoint;
            diagnostics = nullptr;
            useSiteModule->findAndCheckEntryPoint("MainCS", SLANG_STAGE_COMPUTE, entryPoint.writeRef(), diagnostics.writeRef());
            SlangCompilerService::ReportDiagnostics("OptionsUseSite", diagnostics, !entryPoint);
            if (!entryPoint)
            {
                return false;
            }

            diagnostics = nullptr;
            slang::IModule* implementationModule = session->loadModuleFromSourceString(
                "AtomGeneratedOptions", "AtomGeneratedOptions.slang", AZStd::string(implementationModuleText).c_str(), diagnostics.writeRef());
            SlangCompilerService::ReportDiagnostics("AtomGeneratedOptions", diagnostics, !implementationModule);
            if (!implementationModule)
            {
                return false;
            }

            AZStd::vector<slang::IComponentType*> components;
            components.push_back(useSiteModule);
            components.push_back(implementationModule);
            components.push_back(entryPoint.get());

            Slang::ComPtr<slang::IComponentType> composedProgram;
            diagnostics = nullptr;
            if (SLANG_FAILED(session->createCompositeComponentType(components.data(), components.size(), composedProgram.writeRef(), diagnostics.writeRef())))
            {
                SlangCompilerService::ReportDiagnostics("OptionsUseSite", diagnostics, true);
                return false;
            }

            Slang::ComPtr<slang::IComponentType> linkedProgram;
            diagnostics = nullptr;
            if (SLANG_FAILED(composedProgram->link(linkedProgram.writeRef(), diagnostics.writeRef())))
            {
                SlangCompilerService::ReportDiagnostics("OptionsUseSite", diagnostics, true);
                return false;
            }

            Slang::ComPtr<slang::IBlob> byteCode;
            diagnostics = nullptr;
            if (SLANG_FAILED(linkedProgram->getEntryPointCode(0, 0, byteCode.writeRef(), diagnostics.writeRef())) || !byteCode)
            {
                SlangCompilerService::ReportDiagnostics("OptionsUseSite", diagnostics, true);
                return false;
            }
            const uint8_t* bytes = static_cast<const uint8_t*>(byteCode->getBufferPointer());
            outByteCode.assign(bytes, bytes + byteCode->getBufferSize());
            return true;
        }

        AZStd::unique_ptr<AZ::Test::ScopedAutoTempDirectory> m_tempDirectory;
    };

    TEST_F(SlangOptionsModuleGeneratorTests, DeterministicPacking_MatchesAzslc)
    {
        // The same options declared in AZSL, compiled with the production azslc arguments
        constexpr AZStd::string_view azslSource = R"(
ShaderResourceGroupSemantic SRG_PerDraw
{
    FrequencyId = 0;
    ShaderVariantFallback = 128;
};

ShaderResourceGroup DrawSrg : SRG_PerDraw
{
    RWStructuredBuffer<float4> m_output;
};

option bool o_useTint = false;

enum class Quality { Low, Medium, High };
option Quality o_quality = Quality::Medium;

[[range(1, 8)]]
option int o_sampleCount = 4;

[numthreads(1,1,1)]
void MainCS(uint3 id : SV_DispatchThreadID)
{
    float4 value = float4(1.0, 1.0, 1.0, 1.0);
    if (o_useTint)
    {
        value *= 0.5;
    }
    value.x += float((int)o_quality);
    value.y += float(o_sampleCount);
    DrawSrg::m_output[id.x] = value;
}
)";
        const char* tempDir = m_tempDirectory->GetDirectory();
        ASSERT_TRUE(AZ::Test::CreateTestFile(*m_tempDirectory, "options.azslin", azslSource));
        AZStd::string inputPath;
        AZ::StringFunc::Path::Join(tempDir, "options.azslin", inputPath);
        AZStd::string outputPath;
        AZ::StringFunc::Path::Join(tempDir, "options.hlsl", outputPath);

        AZStd::string azslcOutput;
        ASSERT_TRUE(RunAzslCompiler(
            inputPath,
            AZStd::string::format("--full --Zpr --W1 --root-const=128 --sc-options --namespace=vk --unique-idx -o \"%s\"", outputPath.c_str()),
            azslcOutput)) << azslcOutput.c_str();

        AZStd::string optionsJsonPath;
        AZ::StringFunc::Path::Join(tempDir, "options.options.json", optionsJsonPath);
        auto jsonOutcome = AZ::JsonSerializationUtils::ReadJsonFile(optionsJsonPath, AZ::RPI::JsonUtils::DefaultMaxFileSize);
        ASSERT_TRUE(jsonOutcome.IsSuccess()) << jsonOutcome.GetError().c_str();

        const AzslCompiler parserOnly(inputPath, tempDir);
        RPI::Ptr<RPI::ShaderOptionGroupLayout> azslcLayout = RPI::ShaderOptionGroupLayout::Create();
        bool azslcUsesSpecializationConstants = false;
        ASSERT_TRUE(parserOnly.ParseOptionsPopulateOptionGroupLayout(jsonOutcome.GetValue(), azslcLayout, azslcUsesSpecializationConstants));

        // The generator's packing of the equivalent declarations
        auto layoutOutcome = SlangOptionsModuleGenerator::BuildShaderOptionGroupLayout(MakeDeclarations());
        ASSERT_TRUE(layoutOutcome.IsSuccess()) << layoutOutcome.GetError().c_str();
        const RPI::Ptr<RPI::ShaderOptionGroupLayout> generatedLayout = layoutOutcome.TakeValue();

        EXPECT_EQ(generatedLayout->GetHash(), azslcLayout->GetHash())
            << "Builder-produced option packing diverged from AZSLC's assignment";
    }

    TEST_F(SlangOptionsModuleGeneratorTests, Discovery_ReadsOptionFunctions)
    {
        SlangCompilerService& service = SlangCompilerService::Get();
        const AZStd::unique_lock<AZStd::recursive_mutex> compilerLock = service.AcquireCompilerLock();

        SlangCompilerService::SessionDescriptor sessionDescriptor;
        sessionDescriptor.m_target = SLANG_SPIRV;
        sessionDescriptor.m_profile = "spirv_1_5";
        auto sessionOutcome = service.CreateSession(sessionDescriptor);
        ASSERT_TRUE(sessionOutcome.IsSuccess());
        Slang::ComPtr<slang::ISession> session = sessionOutcome.TakeValue();

        Slang::ComPtr<slang::IBlob> diagnostics;
        slang::IModule* useSiteModule = session->loadModuleFromSourceString(
            "OptionsUseSite", "OptionsUseSite.slang", AZStd::string(UseSiteSource).c_str(), diagnostics.writeRef());
        SlangCompilerService::ReportDiagnostics("OptionsUseSite", diagnostics, !useSiteModule);
        ASSERT_NE(useSiteModule, nullptr);

        auto discoveredOutcome = SlangOptionsModuleGenerator::DiscoverShaderOptions(session);
        ASSERT_TRUE(discoveredOutcome.IsSuccess()) << discoveredOutcome.GetError().c_str();
        const SlangOptionsModuleGenerator::DiscoveredShaderOptions discovered = discoveredOutcome.TakeValue();

        ASSERT_EQ(discovered.m_declarations.size(), 3);
        // The bare [AtomOption] argument defaults to 0: false
        EXPECT_EQ(discovered.m_declarations[0].m_name, Name{"o_useTint"});
        EXPECT_EQ(discovered.m_declarations[0].m_type, RPI::ShaderOptionType::Boolean);
        EXPECT_EQ(discovered.m_declarations[0].m_typeText, "bool");
        EXPECT_EQ(discovered.m_declarations[0].m_defaultValue, Name{"false"});
        EXPECT_EQ(discovered.m_declarations[1].m_name, Name{"o_quality"});
        EXPECT_EQ(discovered.m_declarations[1].m_type, RPI::ShaderOptionType::Enumeration);
        EXPECT_EQ(discovered.m_declarations[1].m_typeText, "Quality");
        ASSERT_EQ(discovered.m_declarations[1].m_enumValues.size(), 3);
        EXPECT_EQ(discovered.m_declarations[1].m_enumValues[0], Name{"Low"});
        EXPECT_EQ(discovered.m_declarations[1].m_enumValues[2], Name{"High"});
        EXPECT_EQ(discovered.m_declarations[1].m_defaultValue, Name{"Medium"});
        EXPECT_EQ(discovered.m_declarations[2].m_name, Name{"o_sampleCount"});
        EXPECT_EQ(discovered.m_declarations[2].m_type, RPI::ShaderOptionType::IntegerRange);
        EXPECT_EQ(discovered.m_declarations[2].m_minValue, 1);
        EXPECT_EQ(discovered.m_declarations[2].m_maxValue, 8);
        EXPECT_EQ(discovered.m_declarations[2].m_defaultValue, Name{"4"});

        EXPECT_THAT(discovered.m_declaringModuleNames, ::testing::ElementsAre("OptionsUseSite"));

        EXPECT_EQ(discovered.m_fallbackShaderResourceGroupName, "DrawSrg");
        EXPECT_EQ(discovered.m_fallbackMemberName, "m_shaderVariantKeyFallback");
        EXPECT_EQ(discovered.m_fallbackDeclaringModuleName, "OptionsUseSite");

        // The discovered declarations must pack identically to the hand-built equivalents the
        // AZSLC parity test uses
        auto discoveredLayoutOutcome = SlangOptionsModuleGenerator::BuildShaderOptionGroupLayout(discovered.m_declarations);
        ASSERT_TRUE(discoveredLayoutOutcome.IsSuccess());
        auto referenceLayoutOutcome = SlangOptionsModuleGenerator::BuildShaderOptionGroupLayout(MakeDeclarations());
        ASSERT_TRUE(referenceLayoutOutcome.IsSuccess());
        EXPECT_EQ(discoveredLayoutOutcome.GetValue()->GetHash(), referenceLayoutOutcome.GetValue()->GetHash());
    }

    TEST_F(SlangOptionsModuleGeneratorTests, GeneratedModules_CompileInAllThreeModes)
    {
        auto layoutOutcome = SlangOptionsModuleGenerator::BuildShaderOptionGroupLayout(MakeDeclarations());
        ASSERT_TRUE(layoutOutcome.IsSuccess()) << layoutOutcome.GetError().c_str();
        const RPI::Ptr<RPI::ShaderOptionGroupLayout> layout = layoutOutcome.TakeValue();
        const SlangOptionsModuleGenerator::DiscoveredShaderOptions discovered = MakeDiscoveredOptions();

        // Specialization-constant mode (Spirv leg — the Dxil splice leg is covered by
        // SpecializationLoweringProbeTests through dxsc)
        {
            const AZStd::string implementationModule = SlangOptionsModuleGenerator::GenerateImplementationModule(
                ShaderOptionLoweringMode::SpecializationConstant, RHI::ShaderTargetFormat::Spirv, "AtomGeneratedOptions", discovered, *layout);
            AZStd::vector<uint8_t> byteCode;
            EXPECT_TRUE(CompileWithImplementationModule(implementationModule, byteCode));
            EXPECT_FALSE(byteCode.empty());
        }

        // Dynamic-fallback mode: the accessors read the [AtomVariantFallback] member directly
        // through an import of the declaring module
        {
            const AZStd::string implementationModule = SlangOptionsModuleGenerator::GenerateImplementationModule(
                ShaderOptionLoweringMode::DynamicFallback, RHI::ShaderTargetFormat::Spirv, "AtomGeneratedOptions", discovered, *layout);
            AZStd::vector<uint8_t> byteCode;
            EXPECT_TRUE(CompileWithImplementationModule(implementationModule, byteCode));
            EXPECT_FALSE(byteCode.empty());
        }

        // Baked mode with a partially specified variant: pinned options bake to literals,
        // unpinned options keep their dynamic fallback reads (AZSL variant semantics), and the
        // mixed module still compiles against the use-site
        {
            RPI::ShaderOptionGroup partialValues(layout);
            partialValues.SetValue(Name{"o_useTint"}, Name{"true"});
            const AZStd::string partialValuesModule =
                SlangOptionsModuleGenerator::GenerateBakedValuesModule("AtomGeneratedOptions", discovered, partialValues);
            EXPECT_NE(partialValuesModule.find("export bool o_useTint() { return true; }"), AZStd::string::npos)
                << partialValuesModule.c_str();
            EXPECT_NE(partialValuesModule.find("DrawSrg.m_shaderVariantKeyFallback"), AZStd::string::npos)
                << partialValuesModule.c_str();

            AZStd::vector<uint8_t> byteCode;
            EXPECT_TRUE(CompileWithImplementationModule(partialValuesModule, byteCode));
            EXPECT_FALSE(byteCode.empty());

            // A fully specified variant needs no fallback read at all
            RPI::ShaderOptionGroup fullValues(layout);
            fullValues.SetAllToDefaultValues();
            const AZStd::string fullValuesModule =
                SlangOptionsModuleGenerator::GenerateBakedValuesModule("AtomGeneratedOptions", discovered, fullValues);
            EXPECT_EQ(fullValuesModule.find("m_shaderVariantKeyFallback"), AZStd::string::npos)
                << fullValuesModule.c_str();
        }

        // Baked mode: accessors return link-time constants, and different baked values must
        // produce different bytecode
        {
            RPI::ShaderOptionGroup defaultValues(layout);
            defaultValues.SetAllToDefaultValues();
            const AZStd::string defaultValuesModule =
                SlangOptionsModuleGenerator::GenerateBakedValuesModule("AtomGeneratedOptions", discovered, defaultValues);

            RPI::ShaderOptionGroup tintedValues(layout);
            tintedValues.SetAllToDefaultValues();
            tintedValues.SetValue(Name{"o_useTint"}, Name{"true"});
            tintedValues.SetValue(Name{"o_quality"}, Name{"High"});
            const AZStd::string tintedValuesModule =
                SlangOptionsModuleGenerator::GenerateBakedValuesModule("AtomGeneratedOptions", discovered, tintedValues);

            AZStd::vector<uint8_t> defaultByteCode;
            EXPECT_TRUE(CompileWithImplementationModule(defaultValuesModule, defaultByteCode));
            AZStd::vector<uint8_t> tintedByteCode;
            EXPECT_TRUE(CompileWithImplementationModule(tintedValuesModule, tintedByteCode));

            const bool byteCodeDiffers =
                defaultByteCode.size() != tintedByteCode.size() ||
                memcmp(defaultByteCode.data(), tintedByteCode.data(), defaultByteCode.size()) != 0;
            EXPECT_TRUE(byteCodeDiffers);
        }
    }
} // namespace UnitTest
