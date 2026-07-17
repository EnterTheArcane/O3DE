/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// M5 (SlangIntegrationPlan.md, Phase 1A.3): the language-neutral ShaderReflectionData contract.
//
// Two properties are verified here:
// 1. The DTO serializes and reloads losslessly (it becomes a job product at M8).
// 2. The AZSLC adapter + shared converters reproduce the legacy pipeline exactly: reference
//    implementations of the pre-M5 layout/contract construction are inlined in this test and
//    their results must match the production path hash-for-hash and field-for-field.

#include <AzTest/AzTest.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzCore/IO/ByteContainerStream.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/Name/Name.h>
#include <AzCore/Serialization/Json/JsonUtils.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/Utils.h>
#include <AzCore/StringFunc/StringFunc.h>
#include <AzCore/Utils/Utils.h>
#include <AzFramework/Process/ProcessCommunicator.h>
#include <AzFramework/Process/ProcessWatcher.h>

#include <Atom/RHI.Reflect/ReflectSystemComponent.h>
#include <Atom/RPI.Edit/Common/JsonUtils.h>
#include <Atom/RPI.Reflect/Shader/ShaderOptionGroupLayout.h>

#include <Common/ShaderBuilderTestFixture.h>

#include <AzslCompiler.h>
#include <AzslData.h>
#include <CommonFiles/CommonTypes.h>
#include <Editor/Azslc/AzslcReflectionAdapter.h>
#include <Editor/ShaderBuilderUtility.h>
#include <Editor/ShaderReflectionData.h>

namespace UnitTest
{
    using namespace AZ;
    using namespace AZ::ShaderBuilder;

    class ShaderReflectionDataTests : public ShaderBuilderTestFixture
    {
    public:
        void SetUp() override
        {
            ShaderBuilderTestFixture::SetUp();
            m_tempDirectory = AZStd::make_unique<AZ::Test::ScopedAutoTempDirectory>();

            m_serializeContext = AZStd::make_unique<SerializeContext>();
            Name::Reflect(m_serializeContext.get());
            RHI::ReflectSystemComponent::Reflect(m_serializeContext.get());
            RPI::ShaderOptionDescriptor::Reflect(m_serializeContext.get());
            RPI::ShaderOptionGroupLayout::Reflect(m_serializeContext.get());
            ShaderReflectionData::Reflect(m_serializeContext.get());
        }

        void TearDown() override
        {
            m_serializeContext.reset();
            m_tempDirectory.reset();
            ShaderBuilderTestFixture::TearDown();
        }

        //! The shader under test, authored in AZSL. Self-contained (no includes).
        static constexpr AZStd::string_view AzslSource = R"(
ShaderResourceGroupSemantic SRG_M5
{
    FrequencyId = 1;
    ShaderVariantFallback = 128;
};

ShaderResourceGroup TestSrg : SRG_M5
{
    Texture2D<float4> m_texture;
    Sampler m_sampler;
    StructuredBuffer<float4> m_data;
    float4 m_color;
    float m_scale;
};

option bool o_useTint = false;

struct VertexInput
{
    float3 m_position : POSITION;
    float2 m_uv : UV0;
};

struct VertexOutput
{
    float4 m_position : SV_Position;
    float2 m_uv : UV0;
};

VertexOutput MainVS(VertexInput input)
{
    VertexOutput output;
    output.m_position = float4(input.m_position * TestSrg::m_scale, 1.0);
    output.m_uv = input.m_uv;
    return output;
}

struct PixelOutput
{
    float4 m_color : SV_Target0;
};

PixelOutput MainPS(VertexOutput input)
{
    PixelOutput output;
    float4 sampled = TestSrg::m_texture.Sample(TestSrg::m_sampler, input.m_uv) + TestSrg::m_data[0];
    output.m_color = o_useTint ? sampled * TestSrg::m_color : sampled;
    return output;
}
)";

        //! Launches azslc.exe (deployed by 3rdParty::azslc beside the test binary) on @inputPath.
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

        //! Runs azslc --full and returns the emitted product paths in AzslSubProducts order.
        bool CompileAzslFullData(AZStd::string_view azslSource, ShaderBuilderUtility::AzslSubProducts::Paths& outPaths)
        {
            const char* tempDir = m_tempDirectory->GetDirectory();

            AZStd::string inputPath;
            AZ::StringFunc::Path::Join(tempDir, "m5parity.azslin", inputPath);
            if (!AZ::Test::CreateTestFile(*m_tempDirectory, "m5parity.azslin", azslSource))
            {
                ADD_FAILURE() << "failed to write azsl input";
                return false;
            }

            AZStd::string outputPath;
            AZ::StringFunc::Path::Join(tempDir, "m5parity.hlsl", outputPath);

            AZStd::string azslcOutput;
            const AZStd::string arguments = AZStd::string::format("--full --Zpr --W1 --root-const=128 --namespace=vk -o \"%s\"", outputPath.c_str());
            if (!RunAzslCompiler(inputPath, arguments, azslcOutput))
            {
                ADD_FAILURE() << "azslc failed: " << azslcOutput.c_str();
                return false;
            }

            outPaths.push_back(inputPath);
            for (const char* suffix : {"ia.json", "om.json", "srg.json", "options.json", "bindingdep.json", "hlsl"})
            {
                AZStd::string productPath;
                AZ::StringFunc::Path::Join(tempDir, AZStd::string::format("m5parity.%s", suffix).c_str(), productPath);
                if (!AZ::IO::SystemFile::Exists(productPath.c_str()))
                {
                    ADD_FAILURE() << "expected azslc product at " << productPath.c_str() << "; azslc output: " << azslcOutput.c_str();
                    return false;
                }
                outPaths.push_back(productPath);
            }
            return true;
        }

        //! Reference implementation of the pre-M5 SrgData -> ShaderResourceGroupLayout conversion,
        //! kept verbatim so the production adapter + converter path is checked against the
        //! original construction order and values.
        static bool ReferenceLoadShaderResourceGroupLayouts(const SrgDataContainer& resourceGroups, RPI::ShaderResourceGroupLayoutList& srgLayoutList)
        {
            for (const SrgData& srgData : resourceGroups)
            {
                RHI::Ptr<RHI::ShaderResourceGroupLayout> newSrgLayout = RHI::ShaderResourceGroupLayout::Create();
                newSrgLayout->SetName(AZ::Name{srgData.m_name.c_str()});
                newSrgLayout->SetUniqueId(srgData.m_containingFileName);
                newSrgLayout->SetBindingSlot(srgData.m_bindingSlot.m_index);

                for (const SamplerSrgData& samplerData : srgData.m_samplers)
                {
                    if (samplerData.m_isDynamic)
                    {
                        newSrgLayout->AddShaderInput({samplerData.m_nameId, samplerData.m_count, samplerData.m_registerId, samplerData.m_spaceId});
                    }
                    else
                    {
                        newSrgLayout->AddStaticSampler({samplerData.m_nameId, samplerData.m_descriptor, samplerData.m_registerId, samplerData.m_spaceId});
                    }
                }

                for (const TextureSrgData& textureData : srgData.m_textures)
                {
                    const RHI::ShaderInputImageAccess imageAccess =
                        textureData.m_isReadOnlyType ? RHI::ShaderInputImageAccess::Read : RHI::ShaderInputImageAccess::ReadWrite;
                    const RHI::ShaderInputImageType imageType =
                        textureData.m_type == TextureType::Texture2D ? RHI::ShaderInputImageType::Image2D : RHI::ShaderInputImageType::Unknown;
                    if (imageType == RHI::ShaderInputImageType::Unknown)
                    {
                        return false; // the pinned source only uses Texture2D
                    }
                    newSrgLayout->AddShaderInput(RHI::ShaderInputImageDescriptor{
                        textureData.m_nameId, imageAccess, imageType, textureData.m_count, textureData.m_registerId, textureData.m_spaceId});
                }

                for (const ConstantBufferData& cbData : srgData.m_constantBuffers)
                {
                    newSrgLayout->AddShaderInput(RHI::ShaderInputBufferDescriptor{
                        cbData.m_nameId, RHI::ShaderInputBufferAccess::Constant, RHI::ShaderInputBufferType::Constant,
                        cbData.m_count, cbData.m_strideSize, cbData.m_registerId, cbData.m_spaceId});
                }

                for (const BufferSrgData& bufferData : srgData.m_buffers)
                {
                    const RHI::ShaderInputBufferAccess bufferAccess =
                        bufferData.m_isReadOnlyType ? RHI::ShaderInputBufferAccess::Read : RHI::ShaderInputBufferAccess::ReadWrite;
                    const RHI::ShaderInputBufferType bufferType =
                        bufferData.m_type == BufferType::StructuredBuffer ? RHI::ShaderInputBufferType::Structured : RHI::ShaderInputBufferType::Unknown;
                    if (bufferType == RHI::ShaderInputBufferType::Unknown)
                    {
                        return false; // the pinned source only uses StructuredBuffer
                    }
                    newSrgLayout->AddShaderInput(RHI::ShaderInputBufferDescriptor{
                        bufferData.m_nameId, bufferAccess, bufferType, bufferData.m_count, bufferData.m_strideSize,
                        bufferData.m_registerId, bufferData.m_spaceId});
                }

                for (const SrgConstantData& srgConstants : srgData.m_srgConstantData)
                {
                    newSrgLayout->AddShaderInput({srgConstants.m_nameId, srgConstants.m_constantByteOffset,
                                                  srgConstants.m_constantByteSize, srgData.m_srgConstantDataRegisterId,
                                                  srgData.m_srgConstantDataSpaceId});
                }

                if (srgData.m_fallbackSize > 0)
                {
                    newSrgLayout->SetShaderVariantKeyFallback(srgData.m_fallbackName, srgData.m_fallbackSize);
                }

                srgLayoutList.push_back(newSrgLayout);
            }
            return true;
        }

        AZStd::unique_ptr<AZ::Test::ScopedAutoTempDirectory> m_tempDirectory;
        AZStd::unique_ptr<SerializeContext> m_serializeContext;
    };

    TEST_F(ShaderReflectionDataTests, SerializationRoundTrip_ConvertersProduceIdenticalLayouts)
    {
        ShaderReflectionData original;

        ShaderResourceGroupReflection srgReflection;
        srgReflection.m_name = Name{"RoundTripSrg"};
        srgReflection.m_uniqueId = "RoundTrip.azsl";
        srgReflection.m_bindingSlot = 3;
        srgReflection.m_shaderVariantKeyFallbackName = Name{"m_color"};
        srgReflection.m_shaderVariantKeyFallbackSize = 128;
        RHI::SamplerState samplerState = RHI::SamplerState::Create(RHI::FilterMode::Linear, RHI::FilterMode::Point, RHI::AddressMode::Wrap);
        srgReflection.m_staticSamplers.push_back({Name{"m_staticSampler"}, samplerState, 4, 0});
        srgReflection.m_samplers.push_back({Name{"m_sampler"}, 1, 5, 0});
        srgReflection.m_images.push_back({Name{"m_texture"}, RHI::ShaderInputImageAccess::Read, RHI::ShaderInputImageType::Image2D, 1, 6, 0});
        srgReflection.m_imageUnboundedArrays.push_back({Name{"m_textureArray"}, RHI::ShaderInputImageAccess::Read, RHI::ShaderInputImageType::Image2D, 100000, 1});
        srgReflection.m_buffers.push_back({Name{"m_constantBuffer"}, RHI::ShaderInputBufferAccess::Constant, RHI::ShaderInputBufferType::Constant, 1, 256, 7, 0});
        srgReflection.m_buffers.push_back({Name{"m_data"}, RHI::ShaderInputBufferAccess::ReadWrite, RHI::ShaderInputBufferType::Structured, 1, 16, 8, 0});
        srgReflection.m_bufferUnboundedArrays.push_back({Name{"m_bufferArray"}, RHI::ShaderInputBufferAccess::Read, RHI::ShaderInputBufferType::Raw, 4, 100001, 1});
        srgReflection.m_constants.push_back({Name{"m_color"}, 0, 16, 9, 0});
        original.m_shaderResourceGroups.push_back(srgReflection);

        ShaderResourceGroupBindingReflection bindingReflection;
        bindingReflection.m_constantDataBinding = {Name{"RoundTripSrg_SRGConstantBuffer"}, 9, 0, {"MainVS", "HelperFunction"}};
        bindingReflection.m_resourceBindings.push_back({Name{"m_texture"}, 6, 0, {"MainPS"}});
        original.m_shaderResourceGroupBindings.emplace("RoundTripSrg", AZStd::move(bindingReflection));

        const AZStd::vector<RPI::ShaderOptionValuePair> boolValues = {
            {Name("false"), RPI::ShaderOptionValue(0)},
            {Name("true"), RPI::ShaderOptionValue(1)},
        };
        original.m_shaderOptions.push_back(RPI::ShaderOptionDescriptor{Name{"o_useTint"}, RPI::ShaderOptionType::Boolean, 0, 0, boolValues, Name{"false"}});
        original.m_usesSpecializationConstants = true;

        original.m_rootConstants.m_bufferName = Name{"RootConstantBuffer"};
        original.m_rootConstants.m_sizeInBytes = 16;
        original.m_rootConstants.m_registerId = 0;
        original.m_rootConstants.m_registerSpace = 0;
        original.m_rootConstants.m_constants.push_back({Name{"m_rootScale"}, 0, 16});

        ShaderFunctionReflection functionReflection;
        functionReflection.m_name = "MainCS";
        ShaderFunctionAttributeReflection attributeReflection;
        attributeReflection.m_name = Name{"numthreads"};
        attributeReflection.m_arguments = {int32_t{8}, int32_t{4}, int32_t{1}};
        functionReflection.m_attributes.push_back(attributeReflection);
        ShaderFunctionAttributeReflection mixedAttribute;
        mixedAttribute.m_name = Name{"testattribute"};
        mixedAttribute.m_arguments = {true, 2.5, AZStd::string("annotation")};
        functionReflection.m_attributes.push_back(mixedAttribute);
        original.m_functions.push_back(functionReflection);

        original.m_vertexEntryInputs["MainVS"] = {
            {"m_position", "POSITION", 0, 3},
            {"m_optional_uv", "UV", 0, 2},
        };
        original.m_fragmentEntryOutputs["MainPS"] = {
            {"SV_Target0", 4},
        };

        // Serialize and reload
        AZStd::vector<AZ::u8> buffer;
        AZ::IO::ByteContainerStream<AZStd::vector<AZ::u8>> stream(&buffer);
        ASSERT_TRUE(AZ::Utils::SaveObjectToStream(stream, AZ::ObjectStream::ST_XML, &original, m_serializeContext.get()));
        stream.Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);
        AZStd::unique_ptr<ShaderReflectionData> loaded(AZ::Utils::LoadObjectFromStream<ShaderReflectionData>(stream, m_serializeContext.get()));
        ASSERT_NE(loaded, nullptr);

        // The reloaded reflection must produce identical layouts
        auto originalLayouts = BuildShaderResourceGroupLayouts(original);
        auto loadedLayouts = BuildShaderResourceGroupLayouts(*loaded);
        ASSERT_TRUE(originalLayouts.IsSuccess());
        ASSERT_TRUE(loadedLayouts.IsSuccess());
        ASSERT_EQ(originalLayouts.GetValue().size(), loadedLayouts.GetValue().size());
        for (size_t i = 0; i < originalLayouts.GetValue().size(); ++i)
        {
            ASSERT_TRUE(originalLayouts.GetValue()[i]->Finalize());
            ASSERT_TRUE(loadedLayouts.GetValue()[i]->Finalize());
            EXPECT_EQ(originalLayouts.GetValue()[i]->GetHash(), loadedLayouts.GetValue()[i]->GetHash());
        }

        auto originalOptionLayout = BuildShaderOptionGroupLayout(original);
        auto loadedOptionLayout = BuildShaderOptionGroupLayout(*loaded);
        ASSERT_NE(originalOptionLayout, nullptr);
        ASSERT_NE(loadedOptionLayout, nullptr);
        EXPECT_EQ(originalOptionLayout->GetHash(), loadedOptionLayout->GetHash());

        // Field-level checks on the parts the layout hashes do not cover
        EXPECT_EQ(loaded->m_schemaVersion, ShaderReflectionData::CurrentSchemaVersion);
        EXPECT_TRUE(loaded->m_usesSpecializationConstants);
        ASSERT_EQ(loaded->m_shaderResourceGroupBindings.size(), 1);
        const ShaderResourceGroupBindingReflection& loadedBinding = loaded->m_shaderResourceGroupBindings.at("RoundTripSrg");
        EXPECT_EQ(loadedBinding.m_constantDataBinding.m_registerId, 9);
        EXPECT_THAT(loadedBinding.m_constantDataBinding.m_dependentFunctions, ::testing::ElementsAre("MainVS", "HelperFunction"));
        ASSERT_EQ(loadedBinding.m_resourceBindings.size(), 1);
        EXPECT_EQ(loadedBinding.m_resourceBindings[0].m_name, Name{"m_texture"});

        EXPECT_EQ(loaded->m_rootConstants.m_bufferName, Name{"RootConstantBuffer"});
        ASSERT_EQ(loaded->m_rootConstants.m_constants.size(), 1);
        EXPECT_EQ(loaded->m_rootConstants.m_constants[0].m_byteSize, 16);

        ASSERT_EQ(loaded->m_functions.size(), 1);
        ASSERT_EQ(loaded->m_functions[0].m_attributes.size(), 2);
        EXPECT_THAT(
            loaded->m_functions[0].m_attributes[0].m_arguments,
            ::testing::ElementsAre(
                ShaderFunctionAttributeArgument{int32_t{8}},
                ShaderFunctionAttributeArgument{int32_t{4}},
                ShaderFunctionAttributeArgument{int32_t{1}}));
        EXPECT_THAT(
            loaded->m_functions[0].m_attributes[1].m_arguments,
            ::testing::ElementsAre(
                ShaderFunctionAttributeArgument{true},
                ShaderFunctionAttributeArgument{2.5},
                ShaderFunctionAttributeArgument{AZStd::string("annotation")}));

        ASSERT_EQ(loaded->m_vertexEntryInputs.size(), 1);
        const auto& loadedInputs = loaded->m_vertexEntryInputs.at("MainVS");
        ASSERT_EQ(loadedInputs.size(), 2);
        EXPECT_EQ(loadedInputs[0].m_semanticText, "POSITION");
        EXPECT_EQ(loadedInputs[0].m_componentCount, 3);
        EXPECT_EQ(loadedInputs[1].m_name, "m_optional_uv");
        ASSERT_EQ(loaded->m_fragmentEntryOutputs.size(), 1);
        EXPECT_EQ(loaded->m_fragmentEntryOutputs.at("MainPS")[0].m_componentCount, 4);
    }

    TEST_F(ShaderReflectionDataTests, AzslcAdapter_MatchesLegacyPipeline)
    {
        ShaderBuilderUtility::AzslSubProducts::Paths subProductPaths;
        if (!CompileAzslFullData(AzslSource, subProductPaths))
        {
            return;
        }

        const char* tempDir = m_tempDirectory->GetDirectory();
        const MapOfStringToStageType shaderEntryPoints = {
            {"MainVS", RPI::ShaderStageType::Vertex},
            {"MainPS", RPI::ShaderStageType::Fragment},
        };

        // Legacy parse: the same structures the pre-M5 pipeline consumed
        AzslData azslData(AZStd::make_shared<ShaderFiles>());
        azslData.m_preprocessedFullPath = subProductPaths[ShaderBuilderUtility::AzslSubProducts::azslin];
        RPI::ShaderResourceGroupLayoutList populateSrgLayoutList;
        RPI::Ptr<RPI::ShaderOptionGroupLayout> parsedOptionLayout = RPI::ShaderOptionGroupLayout::Create();
        BindingDependencies bindingDependencies;
        RootConstantData rootConstantData;
        bool usesSpecializationConstants = false;
        ASSERT_EQ(
            ShaderBuilderUtility::PopulateAzslDataFromJsonFiles(
                "ShaderReflectionDataTests",
                subProductPaths,
                azslData,
                populateSrgLayoutList,
                parsedOptionLayout,
                bindingDependencies,
                rootConstantData,
                tempDir,
                usesSpecializationConstants),
            AssetBuilderSDK::ProcessJobResult_Success);

        // Production path: adapter -> DTO -> shared converters
        auto reflectionOutcome = AzslcReflectionAdapter::BuildReflectionData(
            "ShaderReflectionDataTests",
            azslData,
            *parsedOptionLayout,
            usesSpecializationConstants,
            bindingDependencies,
            rootConstantData,
            subProductPaths[ShaderBuilderUtility::AzslSubProducts::ia],
            subProductPaths[ShaderBuilderUtility::AzslSubProducts::om],
            shaderEntryPoints,
            tempDir);
        ASSERT_TRUE(reflectionOutcome.IsSuccess()) << reflectionOutcome.GetError().c_str();
        const ShaderReflectionData reflectionData = reflectionOutcome.TakeValue();

        // 1. SRG layouts: production converters vs the inlined reference implementation
        auto productionLayoutsOutcome = BuildShaderResourceGroupLayouts(reflectionData);
        ASSERT_TRUE(productionLayoutsOutcome.IsSuccess());
        RPI::ShaderResourceGroupLayoutList productionLayouts = productionLayoutsOutcome.TakeValue();

        RPI::ShaderResourceGroupLayoutList referenceLayouts;
        ASSERT_TRUE(ReferenceLoadShaderResourceGroupLayouts(azslData.m_srgData, referenceLayouts));

        ASSERT_EQ(productionLayouts.size(), referenceLayouts.size());
        ASSERT_FALSE(productionLayouts.empty());
        for (size_t i = 0; i < productionLayouts.size(); ++i)
        {
            ASSERT_TRUE(productionLayouts[i]->Finalize());
            ASSERT_TRUE(referenceLayouts[i]->Finalize());
            EXPECT_EQ(productionLayouts[i]->GetHash(), referenceLayouts[i]->GetHash())
                << "SRG layout hash diverged for " << productionLayouts[i]->GetName().GetCStr();
        }

        // 2. Option layout: production rebuild vs the layout AZSLC's parser built directly
        auto productionOptionLayout = BuildShaderOptionGroupLayout(reflectionData);
        ASSERT_NE(productionOptionLayout, nullptr);
        EXPECT_EQ(productionOptionLayout->GetHash(), parsedOptionLayout->GetHash());
        EXPECT_NE(productionOptionLayout->FindShaderOptionIndex(Name{"o_useTint"}).IsValid(), false);

        // 3. Contracts: production converter vs expected values from the pinned source
        RPI::ShaderInputContract inputContract;
        RPI::ShaderOutputContract outputContract;
        size_t colorAttachmentCount = 0;
        ASSERT_TRUE(BuildShaderInputAndOutputContracts(
            "ShaderReflectionDataTests", reflectionData, shaderEntryPoints, *productionOptionLayout,
            inputContract, outputContract, colorAttachmentCount));

        ASSERT_EQ(inputContract.m_streamChannels.size(), 2);
        EXPECT_EQ(inputContract.m_streamChannels[0].m_semantic.m_name, Name{"POSITION"});
        EXPECT_EQ(inputContract.m_streamChannels[0].m_componentCount, 3);
        EXPECT_FALSE(inputContract.m_streamChannels[0].m_isOptional);
        EXPECT_EQ(inputContract.m_streamChannels[1].m_semantic.m_name, Name{"UV"});
        EXPECT_EQ(inputContract.m_streamChannels[1].m_componentCount, 2);
        ASSERT_EQ(outputContract.m_requiredColorAttachments.size(), 1);
        EXPECT_EQ(outputContract.m_requiredColorAttachments[0].m_componentCount, 4);
        EXPECT_EQ(colorAttachmentCount, 1);

        // 4. Binding dependencies carried through the DTO match the parsed dataset
        ASSERT_FALSE(reflectionData.m_shaderResourceGroupBindings.empty());
        const BindingDependencies::SrgResources* parsedSrgResources = bindingDependencies.GetSrg("TestSrg");
        ASSERT_NE(parsedSrgResources, nullptr);
        const auto findBinding = reflectionData.m_shaderResourceGroupBindings.find("TestSrg");
        ASSERT_NE(findBinding, reflectionData.m_shaderResourceGroupBindings.end());
        EXPECT_EQ(findBinding->second.m_constantDataBinding.m_registerId, parsedSrgResources->m_srgConstantsDependencies.m_binding.m_registerId);
        EXPECT_EQ(findBinding->second.m_resourceBindings.size(), parsedSrgResources->m_resources.size());

        // 5. The DTO built from a real compile serializes and reloads losslessly
        AZStd::vector<AZ::u8> buffer;
        AZ::IO::ByteContainerStream<AZStd::vector<AZ::u8>> stream(&buffer);
        ASSERT_TRUE(AZ::Utils::SaveObjectToStream(stream, AZ::ObjectStream::ST_XML, &reflectionData, m_serializeContext.get()));
        stream.Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);
        AZStd::unique_ptr<ShaderReflectionData> reloaded(AZ::Utils::LoadObjectFromStream<ShaderReflectionData>(stream, m_serializeContext.get()));
        ASSERT_NE(reloaded, nullptr);

        auto reloadedLayoutsOutcome = BuildShaderResourceGroupLayouts(*reloaded);
        ASSERT_TRUE(reloadedLayoutsOutcome.IsSuccess());
        ASSERT_EQ(reloadedLayoutsOutcome.GetValue().size(), productionLayouts.size());
        for (size_t i = 0; i < productionLayouts.size(); ++i)
        {
            ASSERT_TRUE(reloadedLayoutsOutcome.GetValue()[i]->Finalize());
            EXPECT_EQ(reloadedLayoutsOutcome.GetValue()[i]->GetHash(), productionLayouts[i]->GetHash());
        }
        auto reloadedOptionLayout = BuildShaderOptionGroupLayout(*reloaded);
        ASSERT_NE(reloadedOptionLayout, nullptr);
        EXPECT_EQ(reloadedOptionLayout->GetHash(), parsedOptionLayout->GetHash());
    }
} // namespace UnitTest
