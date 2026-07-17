/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// Feasibility gate 1 (SlangIntegrationPlan.md, Phase 0A): shared-SRG binding ABI parity.
//
// Shared SRGs (SceneSrg/ViewSrg/Bindless) are instantiated once against one layout and bound to
// pipelines from every shader that consumes them; the SRG layout hash includes every input's
// register id, so a Slang-compiled shader must reproduce AZSLC's binding layout byte-for-byte.
// This gate compiles the same SRG through both compilers — AZSLC via its reflection JSON, Slang
// via pinned explicit bindings + in-process reflection — and asserts the resulting
// RHI::ShaderResourceGroupLayout hashes are identical.

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
#include <CommonFiles/CommonTypes.h>
#include <SrgLayoutUtility.h>
#include <Slang/SlangCompilerService.h>

namespace UnitTest
{
    using namespace AZ;
    using namespace AZ::ShaderBuilder;

    class Gate1_SharedShaderResourceGroupAbiParityTests : public ShaderBuilderTestFixture
    {
    public:
        //! The two PC compilation targets under test. Bindings pin differently per target:
        //! Vulkan uses [[vk::binding(binding, set)]]; DX12 uses register(<class><index>, space<n>).
        enum class GateTarget
        {
            Vulkan,
            Dx12,
        };

        //! Shared body of the per-target parity check: AZSLC reflection -> pinned Slang source ->
        //! Slang reflection -> field-level and layout-hash comparison.
        void RunAbiParityCheck(GateTarget target, const AZStd::string& azslcApiArguments);
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

        //! The SRG under test, authored in AZSL. Self-contained (no includes).
        static constexpr AZStd::string_view AzslSource = R"(
ShaderResourceGroupSemantic SRG_Gate1
{
    FrequencyId = 2;
};

ShaderResourceGroup TestSrg : SRG_Gate1
{
    Texture2D<float4> m_texture;
    Sampler m_sampler;
    RWStructuredBuffer<float4> m_output;
    float4 m_color;
};

[numthreads(1,1,1)]
void MainCS(uint3 id : SV_DispatchThreadID)
{
    TestSrg::m_output[id.x] = TestSrg::m_texture.SampleLevel(TestSrg::m_sampler, float2(0, 0), 0) + TestSrg::m_color;
}
)";

        //! Launches azslc.exe (deployed by 3rdParty::azslc beside the test binary) on @inputPath.
        //! Mirrors RHI::ExecuteShaderCompiler, which is unavailable here because it resolves
        //! relative tool paths through ComponentApplicationBus.
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

        //! Compiles @azslSource with AZSLC and returns the parsed SRG reflection.
        bool CompileAzslAndGetSrgData(AZStd::string_view azslSource, const AZStd::string& apiArguments, SrgDataContainer& outSrgData)
        {
            const char* tempDir = m_tempDirectory->GetDirectory();

            AZStd::string inputPath;
            AZ::StringFunc::Path::Join(tempDir, "gate1.azslin", inputPath);
            if (!AZ::Test::CreateTestFile(*m_tempDirectory, "gate1.azslin", azslSource))
            {
                ADD_FAILURE() << "failed to write azsl input";
                return false;
            }

            AZStd::string outputPath;
            AZ::StringFunc::Path::Join(tempDir, "gate1.hlsl", outputPath);

            AZStd::string azslcOutput;
            const AZStd::string arguments = AZStd::string::format("--full --Zpr --W1 --root-const=128 %s -o \"%s\"", apiArguments.c_str(), outputPath.c_str());
            if (!RunAzslCompiler(inputPath, arguments, azslcOutput))
            {
                ADD_FAILURE() << "azslc failed: " << azslcOutput.c_str();
                return false;
            }

            // Locate the SRG reflection product (named <output stem>.srg.json by azslc).
            AZStd::string srgJsonPath;
            AZ::StringFunc::Path::Join(tempDir, "gate1.srg.json", srgJsonPath);
            if (!AZ::IO::SystemFile::Exists(srgJsonPath.c_str()))
            {
                ADD_FAILURE() << "expected srg reflection at " << srgJsonPath.c_str() << "; azslc output: " << azslcOutput.c_str();
                return false;
            }

            AZ::Outcome<rapidjson::Document, AZStd::string> jsonOutcome = AZ::JsonSerializationUtils::ReadJsonFile(srgJsonPath, AZ::RPI::JsonUtils::DefaultMaxFileSize);
            if (!jsonOutcome.IsSuccess())
            {
                ADD_FAILURE() << jsonOutcome.GetError().c_str();
                return false;
            }

            AzslCompiler parserOnly(inputPath, tempDir);
            return parserOnly.ParseSrgPopulateSrgData(jsonOutcome.GetValue(), outSrgData);
        }

        //! Emits one pinned global declaration: the grouping attribute, the target-specific
        //! binding annotation, and the declaration itself.
        static AZStd::string DeclarePinnedMember(
            GateTarget target,
            uint32_t bindingSlot,
            AZStd::string_view typeText,
            AZStd::string_view memberName,
            char dxRegisterClass,
            uint32_t registerId,
            uint32_t spaceId)
        {
            AZStd::string declaration = AZStd::string::format("[AtomShaderResourceGroupMember(\"TestSrg\", %u)]\n", bindingSlot);
            if (target == GateTarget::Vulkan)
            {
                declaration += AZStd::string::format("[[vk::binding(%u, %u)]]\n", registerId, spaceId);
                declaration += AZStd::string::format("%.*s %.*s;\n\n", AZ_STRING_ARG(typeText), AZ_STRING_ARG(memberName));
            }
            else
            {
                declaration += AZStd::string::format(
                    "%.*s %.*s : register(%c%u, space%u);\n\n",
                    AZ_STRING_ARG(typeText),
                    AZ_STRING_ARG(memberName),
                    dxRegisterClass,
                    registerId,
                    spaceId);
            }
            return declaration;
        }

        //! Generates the pinned Slang source for the SRG that AZSLC reflected — the same flow the
        //! binding-ABI manifest generator will use in production (AZSL is the editorial source;
        //! the pinned declarations are derived from its reflection).
        static AZStd::string GeneratePinnedSlangSource(const SrgData& srg, GateTarget target)
        {
            const TextureSrgData& texture = srg.m_textures[0];
            const SamplerSrgData& sampler = srg.m_samplers[0];
            const BufferSrgData& buffer = srg.m_buffers[0];
            const uint32_t bindingSlot = srg.m_bindingSlot.GetIndex();

            AZStd::string source = R"(
[__AttributeUsage(_AttributeTargets.Var)]
struct AtomShaderResourceGroupMemberAttribute
{
    string shaderResourceGroupName;
    int bindingSlot;
};

struct TestSrg_Constants
{
    float4 m_color;
};

)";
            source += DeclarePinnedMember(target, bindingSlot, "Texture2D<float4>", "m_texture", 't', texture.m_registerId, texture.m_spaceId);
            source += DeclarePinnedMember(target, bindingSlot, "SamplerState", "m_sampler", 's', sampler.m_registerId, sampler.m_spaceId);
            source += DeclarePinnedMember(target, bindingSlot, "RWStructuredBuffer<float4>", "m_output", 'u', buffer.m_registerId, buffer.m_spaceId);
            source += DeclarePinnedMember(
                target,
                bindingSlot,
                "ConstantBuffer<TestSrg_Constants>",
                "TestSrg_SRGConstantBuffer",
                'b',
                srg.m_srgConstantDataRegisterId,
                srg.m_srgConstantDataSpaceId);
            source += R"(
[shader("compute")]
[numthreads(1,1,1)]
void MainCS(uint3 id : SV_DispatchThreadID)
{
    m_output[id.x] = m_texture.SampleLevel(m_sampler, float2(0, 0), 0) + TestSrg_SRGConstantBuffer.m_color;
}
)";
            return source;
        }

        //! Returns the reflection category that carries a resource's binding for @target.
        //! SPIR-V exposes every resource as a descriptor-table slot; DXIL splits bindings by
        //! register class.
        static SlangParameterCategory GetBindingCategory(
            GateTarget target,
            slang::TypeReflection::Kind kind,
            SlangResourceAccess access)
        {
            if (target == GateTarget::Vulkan)
            {
                return SLANG_PARAMETER_CATEGORY_DESCRIPTOR_TABLE_SLOT;
            }

            switch (kind)
            {
            case slang::TypeReflection::Kind::SamplerState:
                return SLANG_PARAMETER_CATEGORY_SAMPLER_STATE;
            case slang::TypeReflection::Kind::ConstantBuffer:
                return SLANG_PARAMETER_CATEGORY_CONSTANT_BUFFER;
            case slang::TypeReflection::Kind::Resource:
            default:
                if (access == SLANG_RESOURCE_ACCESS_READ)
                {
                    return SLANG_PARAMETER_CATEGORY_SHADER_RESOURCE;
                }
                return SLANG_PARAMETER_CATEGORY_UNORDERED_ACCESS;
            }
        }

        //! Prototype reflection walker (gate quality): groups attribute-tagged globals of a linked
        //! Slang program into SrgData, reading bindings back from reflection — never from the
        //! source annotations — so the test proves reflection agrees with the pinned ABI.
        static bool BuildSrgDataFromSlangProgram(
            slang::IComponentType* linkedProgram,
            GateTarget target,
            SrgDataContainer& outSrgData)
        {
            slang::ShaderReflection* reflection = linkedProgram->getLayout(0);
            if (!reflection)
            {
                ADD_FAILURE() << "no reflection layout";
                return false;
            }

            SrgData srg;
            srg.m_name = "";

            const unsigned parameterCount = reflection->getParameterCount();
            for (unsigned parameterIndex = 0; parameterIndex < parameterCount; ++parameterIndex)
            {
                slang::VariableLayoutReflection* varLayout = reflection->getParameterByIndex(parameterIndex);
                slang::VariableReflection* variable = varLayout->getVariable();

                // Only attribute-tagged globals belong to the SRG.
                AZStd::string srgName;
                int bindingSlot = -1;
                for (unsigned attributeIndex = 0; attributeIndex < variable->getUserAttributeCount(); ++attributeIndex)
                {
                    slang::UserAttribute* attribute = variable->getUserAttributeByIndex(attributeIndex);
                    if (azstricmp(attribute->getName(), "AtomShaderResourceGroupMember") == 0)
                    {
                        size_t nameLength = 0;
                        if (const char* name = attribute->getArgumentValueString(0, &nameLength); name && nameLength > 0)
                        {
                            srgName.assign(name, nameLength);
                        }
                        attribute->getArgumentValueInt(1, &bindingSlot);
                    }
                }
                if (srgName.empty())
                {
                    continue;
                }

                srg.m_name = srgName;
                srg.m_bindingSlot = RHI::Handle<uint32_t>(static_cast<uint32_t>(bindingSlot));

                slang::TypeLayoutReflection* typeLayout = varLayout->getTypeLayout();
                SlangResourceAccess resourceAccess = SLANG_RESOURCE_ACCESS_NONE;
                if (typeLayout->getKind() == slang::TypeReflection::Kind::Resource)
                {
                    resourceAccess = typeLayout->getType()->getResourceAccess();
                }
                const SlangParameterCategory bindingCategory = GetBindingCategory(target, typeLayout->getKind(), resourceAccess);
                const uint32_t binding = static_cast<uint32_t>(varLayout->getOffset(bindingCategory));
                const uint32_t space = static_cast<uint32_t>(varLayout->getBindingSpace(bindingCategory));
                const char* variableName = variable->getName();

                switch (typeLayout->getKind())
                {
                case slang::TypeReflection::Kind::Resource:
                    {
                        const SlangResourceShape shape = typeLayout->getType()->getResourceShape();
                        const SlangResourceAccess access = typeLayout->getType()->getResourceAccess();
                        if ((shape & SLANG_RESOURCE_BASE_SHAPE_MASK) == SLANG_TEXTURE_2D)
                        {
                            TextureSrgData texture;
                            texture.m_nameId = Name(variableName);
                            texture.m_type = TextureType::Texture2D;
                            texture.m_isReadOnlyType = (access == SLANG_RESOURCE_ACCESS_READ);
                            texture.m_registerId = binding;
                            texture.m_spaceId = space;
                            srg.m_textures.push_back(AZStd::move(texture));
                        }
                        else if ((shape & SLANG_RESOURCE_BASE_SHAPE_MASK) == SLANG_STRUCTURED_BUFFER)
                        {
                            BufferSrgData buffer;
                            buffer.m_nameId = Name(variableName);
                            if (access == SLANG_RESOURCE_ACCESS_READ)
                            {
                                buffer.m_type = BufferType::StructuredBuffer;
                            }
                            else
                            {
                                buffer.m_type = BufferType::RwStructuredBuffer;
                            }
                            buffer.m_isReadOnlyType = (access == SLANG_RESOURCE_ACCESS_READ);
                            buffer.m_strideSize = static_cast<uint32_t>(typeLayout->getElementTypeLayout()->getSize(SLANG_PARAMETER_CATEGORY_UNIFORM));
                            buffer.m_registerId = binding;
                            buffer.m_spaceId = space;
                            srg.m_buffers.push_back(AZStd::move(buffer));
                        }
                        else
                        {
                            ADD_FAILURE() << "unhandled resource shape for " << variableName;
                            return false;
                        }
                        break;
                    }
                case slang::TypeReflection::Kind::SamplerState:
                    {
                        SamplerSrgData sampler;
                        sampler.m_nameId = Name(variableName);
                        sampler.m_isDynamic = true;
                        sampler.m_registerId = binding;
                        sampler.m_spaceId = space;
                        srg.m_samplers.push_back(AZStd::move(sampler));
                        break;
                    }
                case slang::TypeReflection::Kind::ConstantBuffer:
                    {
                        // The SRG-constants constant buffer: fields become SRG constants.
                        srg.m_srgConstantDataRegisterId = binding;
                        srg.m_srgConstantDataSpaceId = space;
                        slang::TypeLayoutReflection* elementLayout = typeLayout->getElementTypeLayout();
                        for (unsigned fieldIndex = 0; fieldIndex < elementLayout->getFieldCount(); ++fieldIndex)
                        {
                            slang::VariableLayoutReflection* field = elementLayout->getFieldByIndex(fieldIndex);
                            SrgConstantData constant;
                            constant.m_nameId = Name(field->getVariable()->getName());
                            constant.m_constantByteOffset = static_cast<uint32_t>(field->getOffset(SLANG_PARAMETER_CATEGORY_UNIFORM));
                            constant.m_constantByteSize = static_cast<uint32_t>(field->getTypeLayout()->getSize(SLANG_PARAMETER_CATEGORY_UNIFORM));
                            srg.m_srgConstantData.push_back(AZStd::move(constant));
                        }
                        break;
                    }
                default:
                    ADD_FAILURE() << "unhandled parameter kind for " << variableName;
                    return false;
                }
            }

            if (srg.m_name.empty())
            {
                ADD_FAILURE() << "no attribute-tagged globals found in reflection";
                return false;
            }

            outSrgData.push_back(AZStd::move(srg));
            return true;
        }

        //! Compiles @slangSource for @target and walks its reflection into SrgData.
        static bool CompileSlangAndGetSrgData(
            AZStd::string_view slangSource,
            GateTarget target,
            SrgDataContainer& outSrgData)
        {
            SlangCompilerService& service = SlangCompilerService::Get();
            AZStd::unique_lock<AZStd::recursive_mutex> lock = service.AcquireCompilerLock();

            SlangCompilerService::SessionDescriptor sessionDescriptor;
            if (target == GateTarget::Vulkan)
            {
                sessionDescriptor.m_target = SLANG_SPIRV;
                sessionDescriptor.m_profile = "spirv_1_5";
            }
            else
            {
                sessionDescriptor.m_target = SLANG_DXIL;
                sessionDescriptor.m_profile = "sm_6_2";
            }
            AZ::Outcome<Slang::ComPtr<slang::ISession>, AZStd::string> sessionOutcome = service.CreateSession(sessionDescriptor);
            if (!sessionOutcome.IsSuccess())
            {
                ADD_FAILURE() << sessionOutcome.GetError().c_str();
                return false;
            }
            Slang::ComPtr<slang::ISession> session = sessionOutcome.TakeValue();

            const AZStd::string source(slangSource);
            Slang::ComPtr<slang::IBlob> diagnostics;
            slang::IModule* module = session->loadModuleFromSourceString("Gate1", "Gate1.slang", source.c_str(), diagnostics.writeRef());
            SlangCompilerService::ReportDiagnostics("Gate1.slang", diagnostics, module == nullptr);
            if (!module)
            {
                ADD_FAILURE() << "Slang module failed to compile";
                return false;
            }

            Slang::ComPtr<slang::IEntryPoint> entryPoint;
            module->findAndCheckEntryPoint("MainCS", SLANG_STAGE_COMPUTE, entryPoint.writeRef(), diagnostics.writeRef());
            if (!entryPoint)
            {
                ADD_FAILURE() << "MainCS not found";
                return false;
            }

            slang::IComponentType* components[] = {module, entryPoint.get()};
            Slang::ComPtr<slang::IComponentType> composite;
            session->createCompositeComponentType(components, AZ_ARRAY_SIZE(components), composite.writeRef(), diagnostics.writeRef());
            Slang::ComPtr<slang::IComponentType> linkedProgram;
            if (composite)
            {
                composite->link(linkedProgram.writeRef(), diagnostics.writeRef());
            }
            if (!linkedProgram)
            {
                ADD_FAILURE() << "failed to link Slang program";
                return false;
            }

            return BuildSrgDataFromSlangProgram(linkedProgram, target, outSrgData);
        }

        static RPI::ShaderResourceGroupLayoutList BuildLayouts(const SrgDataContainer& srgData)
        {
            RPI::ShaderResourceGroupLayoutList layouts;
            EXPECT_TRUE(SrgLayoutUtility::LoadShaderResourceGroupLayouts("Gate1", srgData, layouts));
            return layouts;
        }

        AZStd::unique_ptr<AZ::Test::ScopedAutoTempDirectory> m_tempDirectory;
    };

    void Gate1_SharedShaderResourceGroupAbiParityTests::RunAbiParityCheck(
        GateTarget target,
        const AZStd::string& azslcApiArguments)
    {
        // 1) AZSLC is the editorial source of the binding ABI.
        SrgDataContainer azslSrgData;
        ASSERT_TRUE(CompileAzslAndGetSrgData(AzslSource, azslcApiArguments, azslSrgData));
        ASSERT_EQ(azslSrgData.size(), 1u);
        const SrgData& azslSrg = azslSrgData[0];
        ASSERT_EQ(azslSrg.m_textures.size(), 1u);
        ASSERT_EQ(azslSrg.m_samplers.size(), 1u);
        ASSERT_EQ(azslSrg.m_buffers.size(), 1u);
        ASSERT_EQ(azslSrg.m_srgConstantData.size(), 1u);

        // 2) Generate the pinned Slang declarations from AZSLC's reflection and compile them.
        SrgDataContainer slangSrgData;
        ASSERT_TRUE(CompileSlangAndGetSrgData(GeneratePinnedSlangSource(azslSrg, target), target, slangSrgData));
        ASSERT_EQ(slangSrgData.size(), 1u);
        const SrgData& slangSrg = slangSrgData[0];

        // 3) Field-level comparison first, for actionable failures.
        EXPECT_EQ(slangSrg.m_name, azslSrg.m_name);
        EXPECT_EQ(slangSrg.m_bindingSlot.GetIndex(), azslSrg.m_bindingSlot.GetIndex());
        ASSERT_EQ(slangSrg.m_textures.size(), 1u);
        EXPECT_EQ(slangSrg.m_textures[0].m_nameId, azslSrg.m_textures[0].m_nameId);
        EXPECT_EQ(slangSrg.m_textures[0].m_registerId, azslSrg.m_textures[0].m_registerId);
        EXPECT_EQ(slangSrg.m_textures[0].m_spaceId, azslSrg.m_textures[0].m_spaceId);
        ASSERT_EQ(slangSrg.m_samplers.size(), 1u);
        EXPECT_EQ(slangSrg.m_samplers[0].m_registerId, azslSrg.m_samplers[0].m_registerId);
        EXPECT_EQ(slangSrg.m_samplers[0].m_spaceId, azslSrg.m_samplers[0].m_spaceId);
        ASSERT_EQ(slangSrg.m_buffers.size(), 1u);
        EXPECT_EQ(slangSrg.m_buffers[0].m_registerId, azslSrg.m_buffers[0].m_registerId);
        EXPECT_EQ(slangSrg.m_buffers[0].m_spaceId, azslSrg.m_buffers[0].m_spaceId);
        EXPECT_EQ(slangSrg.m_buffers[0].m_strideSize, azslSrg.m_buffers[0].m_strideSize);
        ASSERT_EQ(slangSrg.m_srgConstantData.size(), 1u);
        EXPECT_EQ(slangSrg.m_srgConstantData[0].m_nameId, azslSrg.m_srgConstantData[0].m_nameId);
        EXPECT_EQ(slangSrg.m_srgConstantData[0].m_constantByteOffset, azslSrg.m_srgConstantData[0].m_constantByteOffset);
        EXPECT_EQ(slangSrg.m_srgConstantData[0].m_constantByteSize, azslSrg.m_srgConstantData[0].m_constantByteSize);
        EXPECT_EQ(slangSrg.m_srgConstantDataRegisterId, azslSrg.m_srgConstantDataRegisterId);
        EXPECT_EQ(slangSrg.m_srgConstantDataSpaceId, azslSrg.m_srgConstantDataSpaceId);

        // 4) The gate's verdict: byte-identical layout hashes.
        RPI::ShaderResourceGroupLayoutList azslLayouts = BuildLayouts(azslSrgData);
        RPI::ShaderResourceGroupLayoutList slangLayouts = BuildLayouts(slangSrgData);
        ASSERT_EQ(azslLayouts.size(), 1u);
        ASSERT_EQ(slangLayouts.size(), 1u);
        ASSERT_TRUE(azslLayouts[0]->Finalize());
        ASSERT_TRUE(slangLayouts[0]->Finalize());
        EXPECT_EQ(slangLayouts[0]->GetHash(), azslLayouts[0]->GetHash());
    }

    TEST_F(Gate1_SharedShaderResourceGroupAbiParityTests, Vulkan_PinnedBindings_LayoutHashesMatchAzslc)
    {
        RunAbiParityCheck(GateTarget::Vulkan, "--namespace=vk --unique-idx");
    }

    TEST_F(Gate1_SharedShaderResourceGroupAbiParityTests, Dx12_PinnedBindings_LayoutHashesMatchAzslc)
    {
        RunAbiParityCheck(GateTarget::Dx12, "--namespace=dx");
    }
} // namespace UnitTest
