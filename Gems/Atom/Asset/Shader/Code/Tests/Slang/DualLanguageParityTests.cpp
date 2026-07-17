/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// M9 (SlangIntegrationPlan.md, Phase 1B exit): dual-language parity through the PRODUCTION path.
//
// The feasibility gates proved parity with prototype walkers; these tests prove it through the
// shipping pipeline: the same shader authored in AZSL (compiled by azslc, parsed by the AZSLC
// adapter) and in Slang (compiled in-process, walked by SlangReflectionWalker) must produce
// byte-identical ShaderResourceGroupLayout hashes on both PC targets, matching stage interfaces,
// and exact per-entry stage dependencies.

#include <AzTest/AzTest.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzCore/IO/Path/Path.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/Serialization/Json/JsonUtils.h>
#include <AzCore/StringFunc/StringFunc.h>
#include <AzCore/Utils/Utils.h>
#include <AzFramework/Process/ProcessCommunicator.h>
#include <AzFramework/Process/ProcessWatcher.h>
#include <Atom/RPI.Edit/Common/JsonUtils.h>

#include <Common/ShaderBuilderTestFixture.h>

#include <AzslCompiler.h>
#include <AzslData.h>
#include <CommonFiles/CommonTypes.h>
#include <Editor/ShaderReflectionData.h>
#include <Slang/SlangBackend.h>
#include <Slang/SlangCompilerService.h>
#include <Slang/SlangReflectionWalker.h>
#include <SrgLayoutUtility.h>

namespace UnitTest
{
    using namespace AZ;
    using namespace AZ::ShaderBuilder;

    class DualLanguageParityTests : public ShaderBuilderTestFixture
    {
    public:
        enum class ParityTarget
        {
            Vulkan,
            Dx12,
        };

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

        //! The shared SRG under test, authored in AZSL — the editorial source of the binding ABI.
        static constexpr AZStd::string_view AzslSource = R"(
ShaderResourceGroupSemantic SRG_Parity
{
    FrequencyId = 3;
};

ShaderResourceGroup ParitySrg : SRG_Parity
{
    Texture2D<float4> m_texture;
    Sampler m_sampler;
    RWStructuredBuffer<float4> m_output;
    float4 m_color;
    float m_scale;
};

[numthreads(1,1,1)]
void MainCS(uint3 id : SV_DispatchThreadID)
{
    ParitySrg::m_output[id.x] = ParitySrg::m_texture.SampleLevel(ParitySrg::m_sampler, float2(0, 0), 0) * ParitySrg::m_scale + ParitySrg::m_color;
}
)";

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

        //! Compiles the AZSL fixture and returns its parsed SRG reflection.
        bool CompileAzslAndGetSrgData(ParityTarget target, SrgDataContainer& outSrgData)
        {
            const char* tempDir = m_tempDirectory->GetDirectory();
            if (!AZ::Test::CreateTestFile(*m_tempDirectory, "parity.azslin", AzslSource))
            {
                ADD_FAILURE() << "failed to write azsl input";
                return false;
            }
            AZStd::string inputPath;
            AZ::StringFunc::Path::Join(tempDir, "parity.azslin", inputPath);
            AZStd::string outputPath;
            AZ::StringFunc::Path::Join(tempDir, "parity.hlsl", outputPath);

            const char* apiArguments = target == ParityTarget::Vulkan ? "--namespace=vk --unique-idx" : "--namespace=dx";
            AZStd::string azslcOutput;
            const AZStd::string arguments = AZStd::string::format(
                "--full --Zpr --W1 --root-const=128 --sc-options %s -o \"%s\"", apiArguments, outputPath.c_str());
            if (!RunAzslCompiler(inputPath, arguments, azslcOutput))
            {
                ADD_FAILURE() << "azslc failed: " << azslcOutput.c_str();
                return false;
            }

            AZStd::string srgJsonPath;
            AZ::StringFunc::Path::Join(tempDir, "parity.srg.json", srgJsonPath);
            auto jsonOutcome = AZ::JsonSerializationUtils::ReadJsonFile(srgJsonPath, AZ::RPI::JsonUtils::DefaultMaxFileSize);
            if (!jsonOutcome.IsSuccess())
            {
                ADD_FAILURE() << jsonOutcome.GetError().c_str();
                return false;
            }
            const AzslCompiler parserOnly(inputPath, tempDir);
            return parserOnly.ParseSrgPopulateSrgData(jsonOutcome.GetValue(), outSrgData);
        }

        //! Generates the pinned Slang declaration of the SRG that AZSLC reflected — the same
        //! derivation the binding-ABI manifest generator will productize.
        static AZStd::string GeneratePinnedSlangSource(const SrgData& srg, ParityTarget target)
        {
            const TextureSrgData& texture = srg.m_textures[0];
            const SamplerSrgData& sampler = srg.m_samplers[0];
            const BufferSrgData& buffer = srg.m_buffers[0];
            const uint32_t bindingSlot = srg.m_bindingSlot.GetIndex();

            auto declareMember = [&](AZStd::string_view typeText, AZStd::string_view memberName, char dxRegisterClass, uint32_t registerId, uint32_t spaceId)
            {
                AZStd::string declaration = AZStd::string::format("[AtomShaderResourceGroupMember(\"ParitySrg\", %u)]\n", bindingSlot);
                if (target == ParityTarget::Vulkan)
                {
                    declaration += AZStd::string::format("[[vk::binding(%u, %u)]]\n", registerId, spaceId);
                    declaration += AZStd::string::format("%.*s %.*s;\n\n", AZ_STRING_ARG(typeText), AZ_STRING_ARG(memberName));
                }
                else
                {
                    declaration += AZStd::string::format(
                        "%.*s %.*s : register(%c%u, space%u);\n\n",
                        AZ_STRING_ARG(typeText), AZ_STRING_ARG(memberName), dxRegisterClass, registerId, spaceId);
                }
                return declaration;
            };

            AZStd::string source = R"(
struct ParitySrg_Constants
{
    Vector4F m_color;
    f32 m_scale;
};

)";
            source += declareMember("Texture2D<Vector4F>", "m_texture", 't', texture.m_registerId, texture.m_spaceId);
            source += declareMember("SamplerState", "m_sampler", 's', sampler.m_registerId, sampler.m_spaceId);
            source += declareMember("RWStructuredBuffer<Vector4F>", "m_output", 'u', buffer.m_registerId, buffer.m_spaceId);
            source += declareMember(
                "ConstantBuffer<ParitySrg_Constants>", "ParitySrg_SRGConstantBuffer", 'b',
                srg.m_srgConstantDataRegisterId, srg.m_srgConstantDataSpaceId);
            source += R"(
[numthreads(1,1,1)]
void MainCS(u32 index : SV_DispatchThreadID)
{
    m_output[index] = m_texture.SampleLevel(m_sampler, Vector2F(0, 0), 0) * ParitySrg_SRGConstantBuffer.m_scale + ParitySrg_SRGConstantBuffer.m_color;
}
)";
            return source;
        }

        //! Compiles @slangSource through the production backend and walks it into the DTO.
        bool CompileSlangAndWalk(ParityTarget target, AZStd::string_view slangSource, ShaderReflectionData& outReflectionData)
        {
            if (!AZ::Test::CreateTestFile(*m_tempDirectory, "parity.slang", slangSource))
            {
                ADD_FAILURE() << "failed to write slang input";
                return false;
            }
            AZ::IO::FixedMaxPath sourcePath(m_tempDirectory->GetDirectory());
            sourcePath /= "parity.slang";

            AZ::IO::FixedMaxPath engineRoot = AZ::Utils::GetExecutableDirectory();
            for (int i = 0; i < 4; ++i)
            {
                engineRoot = engineRoot.ParentPath();
            }
            AZ::IO::FixedMaxPath shaderLibPath = engineRoot;
            shaderLibPath /= "Gems/Atom/RPI/Assets/ShaderLib";
            const AZStd::vector<AZStd::string> includePaths = {AZStd::string(shaderLibPath.String())};

            const MapOfStringToStageType entryPoints = {
                {"MainCS", RPI::ShaderStageType::Compute},
            };

            RHI::ShaderTargetDescriptor targetDescriptor;
            targetDescriptor.m_format = target == ParityTarget::Vulkan ? RHI::ShaderTargetFormat::Spirv : RHI::ShaderTargetFormat::Dxil;

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
                return false;
            }
            const SlangBackend::ProgramCompilation compilation = compilationOutcome.TakeValue();

            auto reflectionOutcome = SlangReflectionWalker::BuildReflectionData(
                compilation.m_linkedProgram, targetDescriptor.m_format, compilation.m_entryPointNames);
            if (!reflectionOutcome.IsSuccess())
            {
                ADD_FAILURE() << reflectionOutcome.GetError().c_str();
                return false;
            }
            outReflectionData = reflectionOutcome.TakeValue();
            return true;
        }

        //! The full parity check for one target: AZSLC reference layouts vs production Slang
        //! pipeline layouts, hash for hash.
        void RunLayoutHashParityCheck(ParityTarget target)
        {
            SrgDataContainer srgData;
            if (!CompileAzslAndGetSrgData(target, srgData))
            {
                return;
            }
            ASSERT_EQ(srgData.size(), 1);

            RPI::ShaderResourceGroupLayoutList referenceLayouts;
            ASSERT_TRUE(SrgLayoutUtility::LoadShaderResourceGroupLayouts("DualLanguageParityTests", srgData, referenceLayouts));
            ASSERT_EQ(referenceLayouts.size(), 1);
            // The pinned Slang declaration reflects with the module name as unique id; align the
            // reference so the hash compares layout content, not source bookkeeping
            referenceLayouts[0]->SetUniqueId("ParitySrg");
            ASSERT_TRUE(referenceLayouts[0]->Finalize());

            ShaderReflectionData reflectionData;
            if (!CompileSlangAndWalk(target, GeneratePinnedSlangSource(srgData[0], target), reflectionData))
            {
                return;
            }
            ASSERT_EQ(reflectionData.m_shaderResourceGroups.size(), 1);

            auto layoutsOutcome = BuildShaderResourceGroupLayouts(reflectionData);
            ASSERT_TRUE(layoutsOutcome.IsSuccess());
            ASSERT_EQ(layoutsOutcome.GetValue().size(), 1);
            ASSERT_TRUE(layoutsOutcome.GetValue()[0]->Finalize());

            EXPECT_EQ(layoutsOutcome.GetValue()[0]->GetHash(), referenceLayouts[0]->GetHash())
                << "ShaderResourceGroup layout hash diverged between AZSLC and the Slang production pipeline";
        }

        AZStd::unique_ptr<AZ::Test::ScopedAutoTempDirectory> m_tempDirectory;
    };

    TEST_F(DualLanguageParityTests, SharedShaderResourceGroup_LayoutHashParity_Vulkan)
    {
        RunLayoutHashParityCheck(ParityTarget::Vulkan);
    }

    TEST_F(DualLanguageParityTests, SharedShaderResourceGroup_LayoutHashParity_Dx12)
    {
        RunLayoutHashParityCheck(ParityTarget::Dx12);
    }

    TEST_F(DualLanguageParityTests, StageDependencies_ExactPerEntryUsage)
    {
        // VS samples only m_vertexTexture; PS reads only m_pixelBuffer. Exact IMetadata-derived
        // dependencies must reflect that split — conservative all-entries masks would not.
        constexpr AZStd::string_view slangSource = R"(
[AtomShaderResourceGroup(3)]
struct UsageSrgLayout
{
    Texture2D<Vector4F> m_vertexTexture;
    SamplerState m_sampler;
    StructuredBuffer<Vector4F> m_pixelBuffer;
};
ParameterBlock<UsageSrgLayout> UsageSrg;

struct VertexOutput
{
    Vector4F m_position : SV_Position;
};

VertexOutput MainVS(u32 vertexIndex : SV_VertexID)
{
    VertexOutput output;
    output.m_position = UsageSrg.m_vertexTexture.SampleLevel(UsageSrg.m_sampler, Vector2F(0, 0), 0);
    return output;
}

struct PixelOutput
{
    Vector4F m_color : SV_Target0;
};

PixelOutput MainPS(VertexOutput input)
{
    PixelOutput output;
    output.m_color = UsageSrg.m_pixelBuffer[0];
    return output;
}
)";
        ASSERT_TRUE(AZ::Test::CreateTestFile(*m_tempDirectory, "usage.slang", slangSource));
        AZ::IO::FixedMaxPath sourcePath(m_tempDirectory->GetDirectory());
        sourcePath /= "usage.slang";

        AZ::IO::FixedMaxPath engineRoot = AZ::Utils::GetExecutableDirectory();
        for (int i = 0; i < 4; ++i)
        {
            engineRoot = engineRoot.ParentPath();
        }
        AZ::IO::FixedMaxPath shaderLibPath = engineRoot;
        shaderLibPath /= "Gems/Atom/RPI/Assets/ShaderLib";
        const AZStd::vector<AZStd::string> includePaths = {AZStd::string(shaderLibPath.String())};

        const MapOfStringToStageType entryPoints = {
            {"MainVS", RPI::ShaderStageType::Vertex},
            {"MainPS", RPI::ShaderStageType::Fragment},
        };

        RHI::ShaderTargetDescriptor targetDescriptor;
        targetDescriptor.m_format = RHI::ShaderTargetFormat::Spirv;

        SlangBackend::ProgramCompileRequest request;
        request.m_sourcePath = sourcePath.Native();
        request.m_entryPoints = &entryPoints;
        request.m_includePaths = includePaths;

        SlangBackend backend;
        auto compilerLock = SlangCompilerService::Get().AcquireCompilerLock();
        auto compilationOutcome = backend.CompileProgram(targetDescriptor, request);
        ASSERT_TRUE(compilationOutcome.IsSuccess()) << compilationOutcome.GetError().c_str();
        const SlangBackend::ProgramCompilation compilation = compilationOutcome.TakeValue();

        auto reflectionOutcome = SlangReflectionWalker::BuildReflectionData(
            compilation.m_linkedProgram, RHI::ShaderTargetFormat::Spirv, compilation.m_entryPointNames);
        ASSERT_TRUE(reflectionOutcome.IsSuccess()) << reflectionOutcome.GetError().c_str();
        const ShaderReflectionData reflectionData = reflectionOutcome.TakeValue();

        const auto findBinding = reflectionData.m_shaderResourceGroupBindings.find("UsageSrg");
        ASSERT_NE(findBinding, reflectionData.m_shaderResourceGroupBindings.end());
        auto dependentFunctionsOf = [&](AZStd::string_view resourceName) -> AZStd::vector<AZStd::string>
        {
            for (const ResourceBindingReflection& resourceBinding : findBinding->second.m_resourceBindings)
            {
                if (resourceBinding.m_name.GetStringView() == resourceName)
                {
                    return resourceBinding.m_dependentFunctions;
                }
            }
            return {};
        };

        EXPECT_THAT(dependentFunctionsOf("m_vertexTexture"), ::testing::ElementsAre("MainVS"));
        EXPECT_THAT(dependentFunctionsOf("m_sampler"), ::testing::ElementsAre("MainVS"));
        EXPECT_THAT(dependentFunctionsOf("m_pixelBuffer"), ::testing::ElementsAre("MainPS"));

        // Stage interfaces: same expectations the AZSL adapter produces for the equivalent
        // fixture (ShaderReflectionDataTests.AzslcAdapter_MatchesLegacyPipeline)
        ASSERT_EQ(reflectionData.m_fragmentEntryOutputs.size(), 1);
        const auto& pixelOutputs = reflectionData.m_fragmentEntryOutputs.at("MainPS");
        ASSERT_EQ(pixelOutputs.size(), 1);
        EXPECT_TRUE(AZ::StringFunc::Equal(pixelOutputs[0].m_semanticText, "SV_Target0"));
        EXPECT_EQ(pixelOutputs[0].m_componentCount, 4);
    }
} // namespace UnitTest
