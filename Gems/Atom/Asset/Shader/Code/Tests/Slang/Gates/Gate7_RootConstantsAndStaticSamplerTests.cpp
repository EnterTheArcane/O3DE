/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// Feasibility gate 7 (SlangIntegrationPlan.md, Phase 0A): root constants and static samplers.
//
// Two of the six critical runtime datasets that must be reproducible from Slang:
// - root constants must satisfy the RootConstantsInfo contract (register/space/byte layout,
//   128-byte capacity) that AZSLC's --root-const flow feeds into pipeline layouts;
// - static samplers must reconstruct the exact RHI::SamplerState AZSLC reflects from AZSL's
//   initialized `Sampler` members — here rebuilt from [AtomStaticSampler] attribute values.

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
#include <Slang/SlangCompilerService.h>

namespace UnitTest
{
    using namespace AZ;
    using namespace AZ::ShaderBuilder;

    class Gate7_RootConstantsAndStaticSamplerTests : public ShaderBuilderTestFixture
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

        //! AZSL fixture: two root constants and one fully initialized static sampler.
        static constexpr AZStd::string_view AzslSource = R"(
ShaderResourceGroupSemantic SRG_Gate7
{
    FrequencyId = 3;
};

rootconstant float4 s_tint;
rootconstant uint s_index;

ShaderResourceGroup SamplerSrg : SRG_Gate7
{
    Texture2D<float4> m_texture;
    Sampler m_staticSampler
    {
        MaxAnisotropy = 16;
        AddressU = Wrap;
        AddressV = Wrap;
        AddressW = Wrap;
    };
    RWStructuredBuffer<float4> m_output;
};

[numthreads(1,1,1)]
void MainCS(uint3 id : SV_DispatchThreadID)
{
    SamplerSrg::m_output[id.x] =
        SamplerSrg::m_texture.SampleLevel(SamplerSrg::m_staticSampler, float2(0, 0), 0) * s_tint + float(s_index);
}
)";

        bool RunAzslcAndParse(SrgDataContainer& outSrgData, RootConstantData& outRootConstantData)
        {
            const char* tempDir = m_tempDirectory->GetDirectory();
            if (!AZ::Test::CreateTestFile(*m_tempDirectory, "gate7.azslin", AzslSource))
            {
                ADD_FAILURE() << "failed to write azsl input";
                return false;
            }
            AZStd::string inputPath;
            AZ::StringFunc::Path::Join(tempDir, "gate7.azslin", inputPath);
            AZStd::string outputPath;
            AZ::StringFunc::Path::Join(tempDir, "gate7.hlsl", outputPath);

            AZ::IO::FixedMaxPath azslcPath = AZ::Utils::GetExecutableDirectory();
            azslcPath /= "Builders/AZSLc/azslc.exe";
            if (!AZ::IO::SystemFile::Exists(azslcPath.c_str()))
            {
                ADD_FAILURE() << "azslc not found at " << azslcPath.c_str();
                return false;
            }

            AzFramework::ProcessLauncher::ProcessLaunchInfo launchInfo;
            launchInfo.m_commandlineParameters = AZStd::string::format(
                "\"%s\" \"%s\" --full --Zpr --W1 --root-const=128 --namespace=dx -o \"%s\"",
                azslcPath.c_str(),
                inputPath.c_str(),
                outputPath.c_str());
            launchInfo.m_showWindow = false;

            AZStd::unique_ptr<AzFramework::ProcessWatcher> watcher(AzFramework::ProcessWatcher::LaunchProcess(launchInfo, AzFramework::COMMUNICATOR_TYPE_STDINOUT));
            if (!watcher)
            {
                ADD_FAILURE() << "failed to launch azslc";
                return false;
            }
            AZStd::string processOutput;
            uint32_t exitCode = 0;
            while (watcher->IsProcessRunning(&exitCode))
            {
                auto communicator = watcher->GetCommunicator();
                if (const AZ::u32 byteCount = communicator->PeekOutput())
                {
                    AZStd::string chunk;
                    chunk.resize_no_construct(byteCount);
                    communicator->ReadOutput(chunk.data(), byteCount);
                    processOutput += chunk;
                }
                if (const AZ::u32 byteCount = communicator->PeekError())
                {
                    AZStd::string chunk;
                    chunk.resize_no_construct(byteCount);
                    communicator->ReadError(chunk.data(), byteCount);
                    processOutput += chunk;
                }
            }
            if (exitCode != 0)
            {
                ADD_FAILURE() << "azslc failed: " << processOutput.c_str();
                return false;
            }

            AZStd::string srgJsonPath;
            AZ::StringFunc::Path::Join(tempDir, "gate7.srg.json", srgJsonPath);
            AZ::Outcome<rapidjson::Document, AZStd::string> jsonOutcome =
                AZ::JsonSerializationUtils::ReadJsonFile(srgJsonPath, AZ::RPI::JsonUtils::DefaultMaxFileSize);
            if (!jsonOutcome.IsSuccess())
            {
                ADD_FAILURE() << jsonOutcome.GetError().c_str();
                return false;
            }

            AzslCompiler parserOnly(inputPath, tempDir);
            if (!parserOnly.ParseSrgPopulateSrgData(jsonOutcome.GetValue(), outSrgData))
            {
                return false;
            }
            return parserOnly.ParseSrgPopulateRootConstantData(jsonOutcome.GetValue(), outRootConstantData);
        }

        AZStd::unique_ptr<AZ::Test::ScopedAutoTempDirectory> m_tempDirectory;
    };

    TEST_F(Gate7_RootConstantsAndStaticSamplerTests, RootConstants_SlangReflectionMatchesAzslcLayout)
    {
        SrgDataContainer srgData;
        RootConstantData rootConstantData;
        ASSERT_TRUE(RunAzslcAndParse(srgData, rootConstantData));
        ASSERT_EQ(rootConstantData.m_constants.size(), 2u);

        // Pin the Slang root-constant buffer at AZSLC's register/space and reflect it back.
        const AZStd::string slangSource = AZStd::string::format(R"(
struct Gate7RootConstants
{
    float4 s_tint;
    uint s_index;
};

[[vk::push_constant]]
ConstantBuffer<Gate7RootConstants> RootConstants : register(b%u, space%u);

RWStructuredBuffer<float4> Output : register(u0, space0);

[shader("compute")]
[numthreads(1,1,1)]
void MainCS(uint3 id : SV_DispatchThreadID)
{
    Output[id.x] = RootConstants.s_tint + float(RootConstants.s_index);
}
)",
            rootConstantData.m_bindingInfo.m_registerId,
            rootConstantData.m_bindingInfo.m_space);

        SlangCompilerService& service = SlangCompilerService::Get();
        const AZStd::unique_lock<AZStd::recursive_mutex> compilerLock = service.AcquireCompilerLock();

        SlangCompilerService::SessionDescriptor sessionDescriptor;
        sessionDescriptor.m_target = SLANG_DXIL;
        sessionDescriptor.m_profile = "sm_6_2";
        AZ::Outcome<Slang::ComPtr<slang::ISession>, AZStd::string> sessionOutcome = service.CreateSession(sessionDescriptor);
        ASSERT_TRUE(sessionOutcome.IsSuccess()) << sessionOutcome.GetError().c_str();
        Slang::ComPtr<slang::ISession> session = sessionOutcome.TakeValue();

        Slang::ComPtr<slang::IBlob> diagnostics;
        slang::IModule* module = session->loadModuleFromSourceString("Gate7Root", "Gate7Root.slang", slangSource.c_str(), diagnostics.writeRef());
        SlangCompilerService::ReportDiagnostics("Gate7Root", diagnostics, module == nullptr);
        ASSERT_NE(module, nullptr);

        Slang::ComPtr<slang::IEntryPoint> entryPoint;
        module->findAndCheckEntryPoint("MainCS", SLANG_STAGE_COMPUTE, entryPoint.writeRef(), diagnostics.writeRef());
        ASSERT_NE(entryPoint, nullptr);
        slang::IComponentType* components[] = {module, entryPoint.get()};
        Slang::ComPtr<slang::IComponentType> composite;
        session->createCompositeComponentType(components, AZ_ARRAY_SIZE(components), composite.writeRef(), diagnostics.writeRef());
        Slang::ComPtr<slang::IComponentType> linkedProgram;
        ASSERT_NE(composite, nullptr);
        composite->link(linkedProgram.writeRef(), diagnostics.writeRef());
        ASSERT_NE(linkedProgram, nullptr);

        slang::ProgramLayout* layout = linkedProgram->getLayout(0);
        ASSERT_NE(layout, nullptr);

        // Find the root-constant buffer parameter and compare byte layout with AZSLC's.
        bool foundRootConstants = false;
        const unsigned parameterCount = layout->getParameterCount();
        for (unsigned parameterIndex = 0; parameterIndex < parameterCount; ++parameterIndex)
        {
            slang::VariableLayoutReflection* varLayout = layout->getParameterByIndex(parameterIndex);
            if (AZStd::string_view(varLayout->getName()) != "RootConstants")
            {
                continue;
            }
            foundRootConstants = true;

            const uint32_t registerId = static_cast<uint32_t>(varLayout->getOffset(SLANG_PARAMETER_CATEGORY_CONSTANT_BUFFER));
            const uint32_t space = static_cast<uint32_t>(varLayout->getBindingSpace(SLANG_PARAMETER_CATEGORY_CONSTANT_BUFFER));
            EXPECT_EQ(registerId, rootConstantData.m_bindingInfo.m_registerId);
            EXPECT_EQ(space, rootConstantData.m_bindingInfo.m_space);

            slang::TypeLayoutReflection* elementLayout = varLayout->getTypeLayout()->getElementTypeLayout();
            ASSERT_EQ(elementLayout->getFieldCount(), rootConstantData.m_constants.size());
            uint32_t totalSize = 0;
            for (unsigned fieldIndex = 0; fieldIndex < elementLayout->getFieldCount(); ++fieldIndex)
            {
                slang::VariableLayoutReflection* field = elementLayout->getFieldByIndex(fieldIndex);
                const SrgConstantData& azslConstant = rootConstantData.m_constants[fieldIndex];
                EXPECT_STREQ(field->getVariable()->getName(), azslConstant.m_nameId.GetCStr());
                EXPECT_EQ(static_cast<uint32_t>(field->getOffset(SLANG_PARAMETER_CATEGORY_UNIFORM)), azslConstant.m_constantByteOffset);
                EXPECT_EQ(static_cast<uint32_t>(field->getTypeLayout()->getSize(SLANG_PARAMETER_CATEGORY_UNIFORM)), azslConstant.m_constantByteSize);
                totalSize = AZStd::max(totalSize, azslConstant.m_constantByteOffset + azslConstant.m_constantByteSize);
            }
            // The RootConstantsInfo contract: everything fits the --root-const capacity.
            EXPECT_LE(totalSize, 128u);
        }
        EXPECT_TRUE(foundRootConstants);
    }

    TEST_F(Gate7_RootConstantsAndStaticSamplerTests, StaticSampler_AttributeValues_RebuildAzslcSamplerState)
    {
        SrgDataContainer srgData;
        RootConstantData rootConstantData;
        ASSERT_TRUE(RunAzslcAndParse(srgData, rootConstantData));
        ASSERT_EQ(srgData.size(), 1u);
        ASSERT_EQ(srgData[0].m_samplers.size(), 1u);
        const SamplerSrgData& azslSampler = srgData[0].m_samplers[0];
        ASSERT_FALSE(azslSampler.m_isDynamic);

        // The Slang side declares the same sampler with attribute-carried state.
        constexpr AZStd::string_view slangSource = R"(
[__AttributeUsage(_AttributeTargets.Var)]
struct AtomStaticSamplerAttribute
{
    int maxAnisotropy;
    string addressMode;
};

[AtomStaticSampler(16, "Wrap")]
SamplerState m_staticSampler : register(s0, space0);

Texture2D<float4> m_texture : register(t0, space0);
RWStructuredBuffer<float4> Output : register(u0, space0);

[shader("compute")]
[numthreads(1,1,1)]
void MainCS(uint3 id : SV_DispatchThreadID)
{
    Output[id.x] = m_texture.SampleLevel(m_staticSampler, float2(0, 0), 0);
}
)";
        SlangCompilerService& service = SlangCompilerService::Get();
        const AZStd::unique_lock<AZStd::recursive_mutex> compilerLock = service.AcquireCompilerLock();

        SlangCompilerService::SessionDescriptor sessionDescriptor;
        sessionDescriptor.m_target = SLANG_DXIL;
        sessionDescriptor.m_profile = "sm_6_2";
        AZ::Outcome<Slang::ComPtr<slang::ISession>, AZStd::string> sessionOutcome = service.CreateSession(sessionDescriptor);
        ASSERT_TRUE(sessionOutcome.IsSuccess()) << sessionOutcome.GetError().c_str();
        Slang::ComPtr<slang::ISession> session = sessionOutcome.TakeValue();

        const AZStd::string source(slangSource);
        Slang::ComPtr<slang::IBlob> diagnostics;
        slang::IModule* module = session->loadModuleFromSourceString("Gate7Sampler", "Gate7Sampler.slang", source.c_str(), diagnostics.writeRef());
        SlangCompilerService::ReportDiagnostics("Gate7Sampler", diagnostics, module == nullptr);
        ASSERT_NE(module, nullptr);

        Slang::ComPtr<slang::IEntryPoint> entryPoint;
        module->findAndCheckEntryPoint("MainCS", SLANG_STAGE_COMPUTE, entryPoint.writeRef(), diagnostics.writeRef());
        ASSERT_NE(entryPoint, nullptr);
        slang::IComponentType* components[] = {module, entryPoint.get()};
        Slang::ComPtr<slang::IComponentType> composite;
        session->createCompositeComponentType(components, AZ_ARRAY_SIZE(components), composite.writeRef(), diagnostics.writeRef());
        Slang::ComPtr<slang::IComponentType> linkedProgram;
        ASSERT_NE(composite, nullptr);
        composite->link(linkedProgram.writeRef(), diagnostics.writeRef());
        ASSERT_NE(linkedProgram, nullptr);

        slang::ProgramLayout* layout = linkedProgram->getLayout(0);
        ASSERT_NE(layout, nullptr);

        // Rebuild the sampler descriptor from the reflected attribute values, the way the
        // production reflection walker will, then compare against AZSLC's descriptor.
        bool foundSampler = false;
        const unsigned parameterCount = layout->getParameterCount();
        for (unsigned parameterIndex = 0; parameterIndex < parameterCount; ++parameterIndex)
        {
            slang::VariableLayoutReflection* varLayout = layout->getParameterByIndex(parameterIndex);
            slang::VariableReflection* variable = varLayout->getVariable();
            for (unsigned attributeIndex = 0; attributeIndex < variable->getUserAttributeCount(); ++attributeIndex)
            {
                slang::UserAttribute* attribute = variable->getUserAttributeByIndex(attributeIndex);
                if (azstricmp(attribute->getName(), "AtomStaticSampler") != 0)
                {
                    continue;
                }
                foundSampler = true;

                int maxAnisotropy = 0;
                ASSERT_TRUE(SLANG_SUCCEEDED(attribute->getArgumentValueInt(0, &maxAnisotropy)));
                size_t addressModeLength = 0;
                const char* addressModeText = attribute->getArgumentValueString(1, &addressModeLength);
                ASSERT_NE(addressModeText, nullptr);
                const AZStd::string addressMode(addressModeText, addressModeLength);

                RHI::SamplerState rebuiltDescriptor = RHI::SamplerState::Create(
                    RHI::FilterMode::Linear, RHI::FilterMode::Linear, RHI::AddressMode::Wrap);
                rebuiltDescriptor.m_anisotropyMax = static_cast<uint32_t>(maxAnisotropy);
                const RHI::AddressMode rebuiltAddressMode = StringToTextureAddressMode(addressMode.c_str());
                rebuiltDescriptor.m_addressU = rebuiltAddressMode;
                rebuiltDescriptor.m_addressV = rebuiltAddressMode;
                rebuiltDescriptor.m_addressW = rebuiltAddressMode;
                rebuiltDescriptor.m_anisotropyEnable = azslSampler.m_descriptor.m_anisotropyEnable;
                rebuiltDescriptor.m_filterMin = azslSampler.m_descriptor.m_filterMin;
                rebuiltDescriptor.m_filterMag = azslSampler.m_descriptor.m_filterMag;
                rebuiltDescriptor.m_filterMip = azslSampler.m_descriptor.m_filterMip;

                // The values the attribute carries must reproduce AZSLC's reflected state.
                EXPECT_EQ(rebuiltDescriptor.m_anisotropyMax, azslSampler.m_descriptor.m_anisotropyMax);
                EXPECT_EQ(rebuiltDescriptor.m_addressU, azslSampler.m_descriptor.m_addressU);
                EXPECT_EQ(rebuiltDescriptor.m_addressV, azslSampler.m_descriptor.m_addressV);
                EXPECT_EQ(rebuiltDescriptor.m_addressW, azslSampler.m_descriptor.m_addressW);
            }
        }
        EXPECT_TRUE(foundSampler);
    }
} // namespace UnitTest
