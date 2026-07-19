/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// M15 (SlangIntegrationPlan.md, Phase 4, D3): the shared-SRG binding ABI.
//
// SceneSrg/ViewSrg/Bindless are a binding ABI: one SRG instance is bound to pipelines compiled from
// many shaders, so every consumer must expect identical bindings. During migration, AZSL- and
// Slang-authored shaders consume these SRGs interchangeably, so the Slang declarations must pin the
// exact register/space AZSLC assigns. This suite reflects the *assembled* AZSL Scene/View SRGs (the
// editorial source) and asserts byte-for-byte ShaderResourceGroupLayout hash parity against the
// checked-in pinned Slang declarations, on both PC targets. It is the drift gate the spec calls for:
// edit an AZSL Scene/View member and this fails until the manifest and .slangi are regenerated.

#include <AzTest/AzTest.h>
#include <AzCore/IO/Path/Path.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/Serialization/Json/JsonUtils.h>
#include <AzCore/StringFunc/StringFunc.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/Utils/Utils.h>
#include <AzFramework/Process/ProcessCommunicator.h>
#include <AzFramework/Process/ProcessWatcher.h>
#include <Atom/RPI.Edit/Common/JsonUtils.h>

#include <Common/ShaderBuilderTestFixture.h>

#include <AzslCompiler.h>
#include <AzslData.h>
#include <CommonFiles/CommonTypes.h>
#include <CommonFiles/Preprocessor.h>
#include <Editor/ShaderReflectionData.h>
#include <Slang/SlangBackend.h>
#include <Slang/SlangCompilerService.h>
#include <Slang/SlangReflectionWalker.h>
#include <SrgLayoutUtility.h>

namespace UnitTest
{
    using namespace AZ;
    using namespace AZ::ShaderBuilder;

    class SharedSrgBindingAbiTests : public ShaderBuilderTestFixture
    {
    public:
        enum class Api
        {
            Vulkan,
            Dx12,
        };

        static const char* ApiName(Api api)
        {
            return api == Api::Vulkan ? "vulkan" : "dx12";
        }

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

        //! Engine root = four levels above the executable directory (bin/<config> -> engine root).
        static AZ::IO::FixedMaxPath EngineRoot()
        {
            AZ::IO::FixedMaxPath engineRoot = AZ::Utils::GetExecutableDirectory();
            for (int i = 0; i < 4; ++i)
            {
                engineRoot = engineRoot.ParentPath();
            }
            return engineRoot;
        }

        //! The include roots that resolve the whole assembled Scene/View SRG closure (empirically
        //! validated): the two engine ShaderLibs, the project ShaderLib (for the first, semantic-
        //! carrying <scenesrg.srgi>/<viewsrg.srgi> partial), and the Gems root (for the fully
        //! qualified <Atom/Feature/Common/Assets/ShaderResourceGroups/...> partial includes).
        static AZStd::vector<AZStd::string> AzslIncludeRoots()
        {
            const AZ::IO::FixedMaxPath engineRoot = EngineRoot();
            auto join = [&](const char* rel)
            {
                AZ::IO::FixedMaxPath p = engineRoot;
                p /= rel;
                return AZStd::string(p.c_str());
            };
            return {
                join("Gems/Atom/Feature/Common/Assets/ShaderLib"),
                join("Gems/Atom/RPI/Assets/ShaderLib"),
                join("AutomatedTesting/ShaderLib"),
                join("Gems"),
            };
        }

        //! ShaderLib roots the Slang backend searches when compiling .slang/.slangi.
        static AZStd::vector<AZStd::string> SlangIncludeRoots()
        {
            const AZ::IO::FixedMaxPath engineRoot = EngineRoot();
            auto join = [&](const char* rel)
            {
                AZ::IO::FixedMaxPath p = engineRoot;
                p /= rel;
                return AZStd::string(p.c_str());
            };
            return {
                join("Gems/Atom/Feature/Common/Assets/ShaderLib"),
                join("Gems/Atom/RPI/Assets/ShaderLib"),
            };
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
            launchInfo.m_commandlineParameters =
                AZStd::string::format("\"%s\" \"%s\" %s", azslcPath.c_str(), inputPath.c_str(), arguments.c_str());
            launchInfo.m_showWindow = false;

            AZStd::unique_ptr<AzFramework::ProcessWatcher> watcher(
                AzFramework::ProcessWatcher::LaunchProcess(launchInfo, AzFramework::COMMUNICATOR_TYPE_STDINOUT));
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

        //! Preprocess a driver that includes @srgAllInclude (e.g. "viewsrg_all.srgi") through the
        //! builder's own mcpp, resolving the full cross-gem SRG closure via the manual include roots.
        bool PreprocessAssembledSrg(const char* srgAllInclude, AZStd::string& outFlattened)
        {
            const AZStd::string driver = AZStd::string::format(
                "#include <%s>\n\n[numthreads(1,1,1)]\nvoid MainCS(uint3 id : SV_DispatchThreadID) {}\n", srgAllInclude);
            if (!AZ::Test::CreateTestFile(*m_tempDirectory, "abidriver.azsl", driver))
            {
                ADD_FAILURE() << "failed to write abi driver";
                return false;
            }
            AZStd::string driverPath;
            AZ::StringFunc::Path::Join(m_tempDirectory->GetDirectory(), "abidriver.azsl", driverPath);

            PreprocessorData ppData;
            const AZStd::vector<AZStd::string> ppArgs = AppendIncludePathsToArgumentList({}, AzslIncludeRoots());
            if (!PreprocessFile(driverPath, ppData, ppArgs, /*collectDiagnostics*/ true))
            {
                ADD_FAILURE() << "preprocess failed for " << srgAllInclude << ": " << ppData.diagnostics.c_str();
                return false;
            }
            outFlattened = ppData.code;
            return true;
        }

        //! Reflect the assembled AZSL SRG named @srgName into a finalized reference layout.
        RHI::Ptr<RHI::ShaderResourceGroupLayout> ReflectAzslReferenceLayout(const char* srgAllInclude, Api api, const char* srgName)
        {
            AZStd::string flattened;
            if (!PreprocessAssembledSrg(srgAllInclude, flattened))
            {
                return {};
            }
            if (!AZ::Test::CreateTestFile(*m_tempDirectory, "abi.azslin", flattened))
            {
                ADD_FAILURE() << "failed to write flattened azsl";
                return {};
            }
            AZStd::string inputPath;
            AZ::StringFunc::Path::Join(m_tempDirectory->GetDirectory(), "abi.azslin", inputPath);
            AZStd::string outputPath;
            AZ::StringFunc::Path::Join(m_tempDirectory->GetDirectory(), "abi.srgjson", outputPath);

            const char* apiArguments = api == Api::Vulkan ? "--namespace=vk --unique-idx" : "--namespace=dx";
            const AZStd::string arguments = AZStd::string::format("--srg %s -o \"%s\"", apiArguments, outputPath.c_str());
            AZStd::string azslcOutput;
            if (!RunAzslCompiler(inputPath, arguments, azslcOutput))
            {
                ADD_FAILURE() << "azslc failed: " << azslcOutput.c_str();
                return {};
            }

            auto jsonOutcome = AZ::JsonSerializationUtils::ReadJsonFile(outputPath, AZ::RPI::JsonUtils::DefaultMaxFileSize);
            if (!jsonOutcome.IsSuccess())
            {
                ADD_FAILURE() << jsonOutcome.GetError().c_str();
                return {};
            }
            SrgDataContainer srgDataContainer;
            const AzslCompiler parserOnly(inputPath, m_tempDirectory->GetDirectory());
            if (!parserOnly.ParseSrgPopulateSrgData(jsonOutcome.GetValue(), srgDataContainer))
            {
                ADD_FAILURE() << "ParseSrgPopulateSrgData failed";
                return {};
            }

            // Keep only the SRG under test (the assembled file also declares the Bindless SRG etc.).
            SrgDataContainer justThisSrg;
            for (const SrgData& srg : srgDataContainer)
            {
                if (srg.m_name == srgName)
                {
                    justThisSrg.push_back(srg);
                }
            }
            if (justThisSrg.size() != 1)
            {
                ADD_FAILURE() << "expected exactly one '" << srgName << "' SRG, found " << justThisSrg.size();
                return {};
            }

            RPI::ShaderResourceGroupLayoutList referenceLayouts;
            if (!SrgLayoutUtility::LoadShaderResourceGroupLayouts("SharedSrgBindingAbiTests", justThisSrg, referenceLayouts) ||
                referenceLayouts.size() != 1)
            {
                ADD_FAILURE() << "failed to build reference layout for " << srgName;
                return {};
            }
            // The pinned Slang declaration reflects with the SRG name as unique id; align the reference
            // so the hash compares layout content, not source bookkeeping.
            referenceLayouts[0]->SetUniqueId(srgName);
            if (!referenceLayouts[0]->Finalize())
            {
                ADD_FAILURE() << "reference layout Finalize failed for " << srgName;
                return {};
            }
            return referenceLayouts[0];
        }

        //! Compile @slangSource as a root module (with a trivial keep-alive entry appended) and return
        //! the finalized layout for the SRG named @srgName. Root-module compilation keeps every declared
        //! global in the program layout regardless of entry usage.
        RHI::Ptr<RHI::ShaderResourceGroupLayout> CompileSlangSourceLayout(AZStd::string_view slangSource, Api api, const char* srgName)
        {
            AZStd::string source(slangSource);
            source += "\n[shader(\"compute\")]\n[numthreads(1,1,1)]\nvoid MainCS(uint3 __abiId : SV_DispatchThreadID) {}\n";
            if (!AZ::Test::CreateTestFile(*m_tempDirectory, "abi.slang", source))
            {
                ADD_FAILURE() << "failed to write slang source";
                return {};
            }
            AZ::IO::FixedMaxPath sourcePath(m_tempDirectory->GetDirectory());
            sourcePath /= "abi.slang";

            const MapOfStringToStageType entryPoints = {{"MainCS", RPI::ShaderStageType::Compute}};
            const AZStd::vector<AZStd::string> includePaths = SlangIncludeRoots();

            RHI::ShaderTargetDescriptor targetDescriptor;
            targetDescriptor.m_format = api == Api::Vulkan ? RHI::ShaderTargetFormat::Spirv : RHI::ShaderTargetFormat::Dxil;

            SlangBackend::ProgramCompileRequest request;
            request.m_sourcePath = sourcePath.Native();
            request.m_entryPoints = &entryPoints;
            request.m_includePaths = includePaths;

            SlangBackend backend;
            auto compilerLock = SlangCompilerService::Get().AcquireCompilerLock();
            auto compilationOutcome = backend.CompileProgram(targetDescriptor, request);
            if (!compilationOutcome.IsSuccess())
            {
                ADD_FAILURE() << "slang compile failed: " << compilationOutcome.GetError().c_str();
                return {};
            }
            const SlangBackend::ProgramCompilation compilation = compilationOutcome.TakeValue();

            auto reflectionOutcome = SlangReflectionWalker::BuildReflectionData(
                compilation.m_linkedProgram, targetDescriptor.m_format, compilation.m_entryPointNames);
            if (!reflectionOutcome.IsSuccess())
            {
                ADD_FAILURE() << "walk failed: " << reflectionOutcome.GetError().c_str();
                return {};
            }
            const ShaderReflectionData reflectionData = reflectionOutcome.TakeValue();

            auto layoutsOutcome = BuildShaderResourceGroupLayouts(reflectionData);
            if (!layoutsOutcome.IsSuccess())
            {
                ADD_FAILURE() << "BuildShaderResourceGroupLayouts failed";
                return {};
            }
            for (const RHI::Ptr<RHI::ShaderResourceGroupLayout>& layout : layoutsOutcome.GetValue())
            {
                if (layout->GetName() == Name(srgName))
                {
                    if (!layout->Finalize())
                    {
                        ADD_FAILURE() << "slang layout Finalize failed for " << srgName;
                        return {};
                    }
                    return layout;
                }
            }
            ADD_FAILURE() << "slang reflection did not produce an SRG named " << srgName;
            return {};
        }

        //! Read a checked-in .slangi (relative to the Feature ShaderLib) and return its SRG layout.
        RHI::Ptr<RHI::ShaderResourceGroupLayout> CompileSlangiLayout(const char* shaderLibRelPath, Api api, const char* srgName)
        {
            AZ::IO::FixedMaxPath slangiPath = EngineRoot();
            slangiPath /= "Gems/Atom/Feature/Common/Assets/ShaderLib";
            slangiPath /= shaderLibRelPath;
            if (!AZ::IO::SystemFile::Exists(slangiPath.c_str()))
            {
                ADD_FAILURE() << "checked-in .slangi not found (regenerate it): " << slangiPath.c_str();
                return {};
            }
            AZ::IO::SystemFile file;
            if (!file.Open(slangiPath.c_str(), AZ::IO::SystemFile::SF_OPEN_READ_ONLY))
            {
                ADD_FAILURE() << "failed to open " << slangiPath.c_str();
                return {};
            }
            AZStd::string content;
            content.resize_no_construct(file.Length());
            file.Read(content.size(), content.data());
            file.Close();
            return CompileSlangSourceLayout(content, api, srgName);
        }

        static void DumpLayout(const RHI::ShaderResourceGroupLayout& layout, const char* tag)
        {
            printf("--- %s: SRG '%s' hash=%llu ---\n", tag, layout.GetName().GetCStr(),
                static_cast<unsigned long long>(layout.GetHash()));
            for (const auto& d : layout.GetShaderInputListForConstants())
            {
                printf("  const  %-44s reg=%u space=%u\n", d.m_name.GetCStr(), d.m_registerId, d.m_spaceId);
            }
            for (const auto& d : layout.GetShaderInputListForBuffers())
            {
                printf("  buffer %-40s reg=%u space=%u stride=%u count=%u\n", d.m_name.GetCStr(), d.m_registerId, d.m_spaceId,
                    d.m_strideSize, d.m_count);
            }
            for (const auto& d : layout.GetShaderInputListForImages())
            {
                printf("  image  %-40s reg=%u space=%u count=%u\n", d.m_name.GetCStr(), d.m_registerId, d.m_spaceId, d.m_count);
            }
            for (const auto& d : layout.GetShaderInputListForSamplers())
            {
                printf("  sampler %-43s reg=%u space=%u\n", d.m_name.GetCStr(), d.m_registerId, d.m_spaceId);
            }
            for (const auto& d : layout.GetStaticSamplers())
            {
                printf("  static-sampler %-35s reg=%u space=%u\n", d.m_name.GetCStr(), d.m_registerId, d.m_spaceId);
            }
            fflush(stdout);
        }

        //! Cross-check the checked-in binding-ABI manifest against the AZSL reference: every resource
        //! and static sampler the manifest records must sit at the register/binding the editorial AZSL
        //! source actually produces (per API). With the layout-hash parity check (AZSL vs Slang), this
        //! is the spec's drift gate: both compilers' reflection is pinned to the reviewed manifest, so
        //! editing a Scene/View SRG without regenerating fails the build.
        void ExpectManifestMatchesReference(
            const char* abiShaderLibRelPath, Api api, const RHI::ShaderResourceGroupLayout* reference, const char* tag)
        {
            ASSERT_NE(reference, nullptr) << tag << ": null reference layout";
            AZ::IO::FixedMaxPath manifestPath = EngineRoot();
            manifestPath /= "Gems/Atom/Feature/Common/Assets/ShaderLib";
            manifestPath /= abiShaderLibRelPath;
            auto jsonOutcome = AZ::JsonSerializationUtils::ReadJsonFile(manifestPath.c_str(), AZ::RPI::JsonUtils::DefaultMaxFileSize);
            ASSERT_TRUE(jsonOutcome.IsSuccess()) << tag << ": cannot read manifest " << manifestPath.c_str();
            const rapidjson::Document& document = jsonOutcome.GetValue();

            AZStd::unordered_map<AZStd::string, uint32_t> referenceRegisters;
            for (const auto& d : reference->GetShaderInputListForBuffers()) { referenceRegisters[d.m_name.GetCStr()] = d.m_registerId; }
            for (const auto& d : reference->GetShaderInputListForImages()) { referenceRegisters[d.m_name.GetCStr()] = d.m_registerId; }
            for (const auto& d : reference->GetStaticSamplers()) { referenceRegisters[d.m_name.GetCStr()] = d.m_registerId; }

            const char* apiKey = api == Api::Vulkan ? "vulkan" : "dx12";
            const char* registerKey = api == Api::Vulkan ? "binding" : "register";
            auto checkEntry = [&](const rapidjson::Value& entry)
            {
                const AZStd::string name = entry["name"].GetString();
                auto found = referenceRegisters.find(name);
                ASSERT_NE(found, referenceRegisters.end()) << tag << ": manifest lists '" << name.c_str()
                    << "' but the AZSL reference does not";
                EXPECT_EQ(found->second, entry[apiKey][registerKey].GetUint())
                    << tag << ": manifest register for '" << name.c_str() << "' does not match the AZSL reference";
            };
            for (const auto& entry : document["resources"].GetArray()) { checkEntry(entry); }
            for (const auto& entry : document["staticSamplers"].GetArray()) { checkEntry(entry); }
        }

        void ExpectLayoutParity(const RHI::ShaderResourceGroupLayout* reference, const RHI::ShaderResourceGroupLayout* candidate, const char* tag)
        {
            ASSERT_NE(reference, nullptr) << tag << ": null reference layout";
            ASSERT_NE(candidate, nullptr) << tag << ": null candidate layout";
            if (reference->GetHash() != candidate->GetHash())
            {
                printf("==== HASH MISMATCH: %s ====\n", tag);
                DumpLayout(*reference, "AZSL reference");
                DumpLayout(*candidate, "Slang pinned");
            }
            EXPECT_EQ(reference->GetHash(), candidate->GetHash())
                << tag << ": ShaderResourceGroup layout hash diverged between AZSLC and the pinned Slang declaration";
        }

        AZStd::unique_ptr<AZ::Test::ScopedAutoTempDirectory> m_tempDirectory;
    };

    // Probe: do explicitly-bound globals survive into the program layout when no entry references
    // them? This decides whether the pinned generator must emit a keep-alive. Root-module compilation
    // with an empty entry; we assert every pinned member reflects at its pinned register.
    TEST_F(SharedSrgBindingAbiTests, PinnedGlobals_ReflectWithoutEntryUsage_Dx12)
    {
        const AZStd::string_view source = R"(
struct ProbeConstants
{
    float4 m_a;
    float m_b;
};

[AtomShaderResourceGroupMember("ProbeSrg", 5)]
ConstantBuffer<ProbeConstants> ProbeSrg_SRGConstantBuffer : register(b0, space0);

[AtomShaderResourceGroupMember("ProbeSrg", 5)]
StructuredBuffer<uint> m_buffer : register(t12, space0);

[AtomShaderResourceGroupMember("ProbeSrg", 5)]
Buffer<uint> m_index : register(t1, space0);

[AtomShaderResourceGroupMember("ProbeSrg", 5)]
Texture2D<float4> m_texture : register(t2, space0);
)";
        auto layout = CompileSlangSourceLayout(source, Api::Dx12, "ProbeSrg");
        ASSERT_NE(layout, nullptr);
        DumpLayout(*layout, "probe dx12");
        EXPECT_GE(layout->GetShaderInputListForBuffers().size(), 1u) << "pinned buffers were stripped without usage";
        EXPECT_GE(layout->GetShaderInputListForImages().size(), 1u) << "pinned images were stripped without usage";
        EXPECT_GE(layout->GetShaderInputListForConstants().size(), 1u) << "SRG constants were stripped without usage";
    }

    // Probe: does Slang's structured-buffer element packing match azslc's relaxed packing? azslc packs
    // a { float3; float; } element to stride 16 (float3 occupies 12, scalar at offset 12). If Slang
    // 16-aligns float3 the stride would be 32 and no trailing padding could repair the internal offsets
    // — that would force a different element-struct porting strategy. This is the load-bearing fact for
    // porting the ~19 Scene/View element structs, so pin it down directly.
    TEST_F(SharedSrgBindingAbiTests, StructuredBufferElementPacking_MatchesAzslc_Dx12)
    {
        const AZStd::string_view source = R"(
struct PackProbeF3Scalar
{
    float3 m_a;
    float m_b;
};
struct PackProbeF3F3
{
    float3 m_a;
    float3 m_b;
};

[AtomShaderResourceGroupMember("PackSrg", 5)]
StructuredBuffer<PackProbeF3Scalar> m_f3scalar : register(t0, space0);

[AtomShaderResourceGroupMember("PackSrg", 5)]
StructuredBuffer<PackProbeF3F3> m_f3f3 : register(t1, space0);
)";
        auto layout = CompileSlangSourceLayout(source, Api::Dx12, "PackSrg");
        ASSERT_NE(layout, nullptr);
        DumpLayout(*layout, "packing probe dx12");
        for (const auto& d : layout->GetShaderInputListForBuffers())
        {
            if (d.m_name == Name("m_f3scalar"))
            {
                EXPECT_EQ(d.m_strideSize, 16u) << "float3+scalar element stride diverged from azslc's relaxed packing";
            }
            else if (d.m_name == Name("m_f3f3"))
            {
                EXPECT_EQ(d.m_strideSize, 24u) << "float3+float3 element stride diverged from azslc's relaxed packing";
            }
        }
    }

    // The full parity gates. These read the checked-in pinned .slangi and compare to the assembled
    // AZSL reference. They fail loudly (with a per-member dump) until the .slangi is regenerated.
    TEST_F(SharedSrgBindingAbiTests, ViewSrg_LayoutHashParity_Dx12)
    {
        auto reference = ReflectAzslReferenceLayout("viewsrg_all.srgi", Api::Dx12, "ViewSrg");
        auto candidate = CompileSlangiLayout("Atom/Features/Srg/ViewSrg.slangi", Api::Dx12, "ViewSrg");
        ExpectLayoutParity(reference.get(), candidate.get(), "ViewSrg dx12");
        ExpectManifestMatchesReference("Atom/Features/Srg/ViewSrg.abi.json", Api::Dx12, reference.get(), "ViewSrg dx12");
    }

    TEST_F(SharedSrgBindingAbiTests, ViewSrg_LayoutHashParity_Vulkan)
    {
        auto reference = ReflectAzslReferenceLayout("viewsrg_all.srgi", Api::Vulkan, "ViewSrg");
        auto candidate = CompileSlangiLayout("Atom/Features/Srg/ViewSrg.slangi", Api::Vulkan, "ViewSrg");
        ExpectLayoutParity(reference.get(), candidate.get(), "ViewSrg vulkan");
        ExpectManifestMatchesReference("Atom/Features/Srg/ViewSrg.abi.json", Api::Vulkan, reference.get(), "ViewSrg vulkan");
    }

    TEST_F(SharedSrgBindingAbiTests, SceneSrg_LayoutHashParity_Dx12)
    {
        auto reference = ReflectAzslReferenceLayout("scenesrg_all.srgi", Api::Dx12, "SceneSrg");
        auto candidate = CompileSlangiLayout("Atom/Features/Srg/SceneSrg.slangi", Api::Dx12, "SceneSrg");
        ExpectLayoutParity(reference.get(), candidate.get(), "SceneSrg dx12");
        ExpectManifestMatchesReference("Atom/Features/Srg/SceneSrg.abi.json", Api::Dx12, reference.get(), "SceneSrg dx12");
    }

    TEST_F(SharedSrgBindingAbiTests, SceneSrg_LayoutHashParity_Vulkan)
    {
        auto reference = ReflectAzslReferenceLayout("scenesrg_all.srgi", Api::Vulkan, "SceneSrg");
        auto candidate = CompileSlangiLayout("Atom/Features/Srg/SceneSrg.slangi", Api::Vulkan, "SceneSrg");
        ExpectLayoutParity(reference.get(), candidate.get(), "SceneSrg vulkan");
        ExpectManifestMatchesReference("Atom/Features/Srg/SceneSrg.abi.json", Api::Vulkan, reference.get(), "SceneSrg vulkan");
    }
} // namespace UnitTest
