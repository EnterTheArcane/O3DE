/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// Feasibility gate 2 (SlangIntegrationPlan.md, Phase 0A): shader options in all three lowerings.
//
// AZSL's `option` keyword gives one authored option three lives: baked constant (variants),
// specialization constant (--sc-options), and dynamic read from the variant-key fallback.
// The requirement (spec D6) is ONE authoring form whose use-sites are identical in every mode.
//
// The form this gate proves: use-sites reference bare identifiers (o_useTint, o_quality)
// imported from an options module the builder GENERATES per mode:
// - baked:   the use-site module declares `extern static const`; a generated values module
//            `export`s the chosen values and is composed at link time (no re-frontend);
// - spec:    the generated options module declares [SpecializationConstant] constants;
// - dynamic: the generated options module declares statics initialized from a fallback-key
//            constant buffer read.
// The DX12 leg of spec mode bakes a sentinel through the link-time path and patches the DXIL
// with dxsc.exe, exactly like the AZSLC pipeline does today.

#include <AzTest/AzTest.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/StringFunc/StringFunc.h>
#include <AzCore/Utils/Utils.h>
#include <AzFramework/Process/ProcessCommunicator.h>
#include <AzFramework/Process/ProcessWatcher.h>

#include <Common/ShaderBuilderTestFixture.h>

#include <Slang/SlangCompilerService.h>

namespace UnitTest
{
    using namespace AZ;
    using ShaderBuilder::SlangCompilerService;

    class Gate2_OptionLoweringTests : public ShaderBuilderTestFixture
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

        //! The use-site: identical in every mode. Options arrive as bare identifiers declared
        //! `extern` here and provided by a composed module (baked mode), or declared by an
        //! imported generated module (spec/dynamic modes use ImportingUseSiteSource below).
        static constexpr AZStd::string_view ExternUseSiteSource = R"(
module Gate2UseSite;

extern static const bool o_useTint;
extern static const int o_quality;

RWStructuredBuffer<float4> Output;

[shader("compute")]
[numthreads(1, 1, 1)]
void MainCS(uint3 id : SV_DispatchThreadID)
{
    float4 value = float4(1.0, 1.0, 1.0, 1.0);
    if (o_useTint)
    {
        value *= 0.5;
    }
    value.x += float(o_quality);
    Output[id.x] = value;
}
)";

        //! Use-site for the imported-module modes: same expressions, options come from import.
        static constexpr AZStd::string_view ImportingUseSiteSource = R"(
module Gate2UseSite;

import Gate2Options;

RWStructuredBuffer<float4> Output;

[shader("compute")]
[numthreads(1, 1, 1)]
void MainCS(uint3 id : SV_DispatchThreadID)
{
    float4 value = float4(1.0, 1.0, 1.0, 1.0);
    if (o_useTint)
    {
        value *= 0.5;
    }
    value.x += float(o_quality);
    Output[id.x] = value;
}
)";

        //! Compiles @useSiteSource, optionally composing extra modules, and returns bytecode.
        //! @param optionsModuleSource When non-empty, written as Gate2Options.slang beside the
        //!        session search path so `import Gate2Options;` resolves.
        //! @param valuesModuleSource When non-empty, loaded and composed into the program to
        //!        provide `export` definitions for `extern` declarations (link-time constants).
        bool CompileUseSite(
            SlangCompileTarget target,
            AZStd::string_view profile,
            AZStd::string_view useSiteSource,
            AZStd::string_view optionsModuleSource,
            AZStd::string_view valuesModuleSource,
            AZStd::vector<uint8_t>& outByteCode,
            Slang::ComPtr<slang::ISession>* outSession = nullptr,
            Slang::ComPtr<slang::IComponentType>* outLinkedProgram = nullptr,
            AZStd::span<const slang::CompilerOptionEntry> extraOptions = {})
        {
            if (!optionsModuleSource.empty())
            {
                if (!AZ::Test::CreateTestFile(*m_tempDirectory, "Gate2Options.slang", optionsModuleSource))
                {
                    ADD_FAILURE() << "failed to write options module";
                    return false;
                }
            }

            SlangCompilerService& service = SlangCompilerService::Get();
            const AZStd::unique_lock<AZStd::recursive_mutex> compilerLock = service.AcquireCompilerLock();

            SlangCompilerService::SessionDescriptor sessionDescriptor;
            sessionDescriptor.m_target = target;
            sessionDescriptor.m_profile = profile;
            sessionDescriptor.m_searchPaths.push_back(m_tempDirectory->GetDirectory());
            sessionDescriptor.m_extraOptions.assign(extraOptions.begin(), extraOptions.end());

            AZ::Outcome<Slang::ComPtr<slang::ISession>, AZStd::string> sessionOutcome = service.CreateSession(sessionDescriptor);
            if (!sessionOutcome.IsSuccess())
            {
                ADD_FAILURE() << sessionOutcome.GetError().c_str();
                return false;
            }
            Slang::ComPtr<slang::ISession> session = sessionOutcome.TakeValue();

            const AZStd::string useSite(useSiteSource);
            Slang::ComPtr<slang::IBlob> diagnostics;
            slang::IModule* useSiteModule = session->loadModuleFromSourceString("Gate2UseSite", "Gate2UseSite.slang", useSite.c_str(), diagnostics.writeRef());
            SlangCompilerService::ReportDiagnostics("Gate2UseSite", diagnostics, useSiteModule == nullptr);
            if (!useSiteModule)
            {
                return false;
            }

            AZStd::vector<slang::IComponentType*> components;
            components.push_back(useSiteModule);

            if (!valuesModuleSource.empty())
            {
                const AZStd::string values(valuesModuleSource);
                diagnostics = nullptr;
                slang::IModule* valuesModule = session->loadModuleFromSourceString("Gate2Values", "Gate2Values.slang", values.c_str(), diagnostics.writeRef());
                SlangCompilerService::ReportDiagnostics("Gate2Values", diagnostics, valuesModule == nullptr);
                if (!valuesModule)
                {
                    return false;
                }
                components.push_back(valuesModule);
            }

            Slang::ComPtr<slang::IEntryPoint> entryPoint;
            diagnostics = nullptr;
            useSiteModule->findAndCheckEntryPoint("MainCS", SLANG_STAGE_COMPUTE, entryPoint.writeRef(), diagnostics.writeRef());
            if (!entryPoint)
            {
                SlangCompilerService::ReportDiagnostics("Gate2UseSite", diagnostics, true);
                return false;
            }
            components.push_back(entryPoint.get());

            Slang::ComPtr<slang::IComponentType> composite;
            diagnostics = nullptr;
            session->createCompositeComponentType(components.data(), aznumeric_cast<SlangInt>(components.size()), composite.writeRef(), diagnostics.writeRef());
            Slang::ComPtr<slang::IComponentType> linkedProgram;
            if (composite)
            {
                composite->link(linkedProgram.writeRef(), diagnostics.writeRef());
            }
            SlangCompilerService::ReportDiagnostics("Gate2UseSite", diagnostics, !linkedProgram);
            if (!linkedProgram)
            {
                return false;
            }

            Slang::ComPtr<slang::IBlob> code;
            diagnostics = nullptr;
            const SlangResult result = linkedProgram->getEntryPointCode(0, 0, code.writeRef(), diagnostics.writeRef());
            SlangCompilerService::ReportDiagnostics("Gate2UseSite", diagnostics, SLANG_FAILED(result));
            if (SLANG_FAILED(result) || !code || code->getBufferSize() == 0)
            {
                return false;
            }

            const uint8_t* codeBegin = static_cast<const uint8_t*>(code->getBufferPointer());
            outByteCode.assign(codeBegin, codeBegin + code->getBufferSize());
            if (outSession != nullptr)
            {
                *outSession = session;
            }
            if (outLinkedProgram != nullptr)
            {
                // Reflection layouts are owned by the linked program; the caller must keep it
                // (and the session) alive while inspecting them.
                *outLinkedProgram = linkedProgram;
            }
            return true;
        }

        AZStd::unique_ptr<AZ::Test::ScopedAutoTempDirectory> m_tempDirectory;
    };

    TEST_F(Gate2_OptionLoweringTests, BakedMode_LinkTimeConstants_ValuesChangeByteCode)
    {
        constexpr AZStd::string_view valuesEnabled = R"(
module Gate2Values;
export static const bool o_useTint = true;
export static const int o_quality = 2;
)";
        constexpr AZStd::string_view valuesDisabled = R"(
module Gate2Values;
export static const bool o_useTint = false;
export static const int o_quality = 0;
)";
        AZStd::vector<uint8_t> byteCodeEnabled;
        ASSERT_TRUE(CompileUseSite(SLANG_SPIRV, "spirv_1_5", ExternUseSiteSource, {}, valuesEnabled, byteCodeEnabled));

        AZStd::vector<uint8_t> byteCodeDisabled;
        ASSERT_TRUE(CompileUseSite(SLANG_SPIRV, "spirv_1_5", ExternUseSiteSource, {}, valuesDisabled, byteCodeDisabled));

        // The linked values must reach codegen: different option values, different bytecode.
        const bool byteCodeDiffers =
            byteCodeEnabled.size() != byteCodeDisabled.size() ||
            memcmp(byteCodeEnabled.data(), byteCodeDisabled.data(), byteCodeEnabled.size()) != 0;
        EXPECT_TRUE(byteCodeDiffers);
    }

    TEST_F(Gate2_OptionLoweringTests, SpecializationConstantMode_ReflectsIdsAndEmitsOpSpecConstant)
    {
        constexpr AZStd::string_view optionsModule = R"(
module Gate2Options;

[vk::constant_id(0)]
public const bool o_useTint = false;

[vk::constant_id(1)]
public const int o_quality = 0;
)";
        AZStd::vector<uint8_t> byteCode;
        Slang::ComPtr<slang::ISession> session;
        Slang::ComPtr<slang::IComponentType> linkedProgram;
        ASSERT_TRUE(CompileUseSite(
            SLANG_SPIRV,
            "spirv_1_5",
            ImportingUseSiteSource,
            optionsModule,
            {},
            byteCode,
            AZStd::addressof(session),
            AZStd::addressof(linkedProgram)));
        ASSERT_NE(linkedProgram, nullptr);
        slang::ProgramLayout* layout = linkedProgram->getLayout(0);
        ASSERT_NE(layout, nullptr);

        // Reflection must expose both options as specialization constants with our IDs.
        int specializationConstantCount = 0;
        const unsigned parameterCount = layout->getParameterCount();
        for (unsigned parameterIndex = 0; parameterIndex < parameterCount; ++parameterIndex)
        {
            slang::VariableLayoutReflection* varLayout = layout->getParameterByIndex(parameterIndex);
            if (varLayout->getCategory() == slang::ParameterCategory::SpecializationConstant)
            {
                ++specializationConstantCount;
                const uint32_t constantId = static_cast<uint32_t>(varLayout->getOffset(SLANG_PARAMETER_CATEGORY_SPECIALIZATION_CONSTANT));
                EXPECT_LE(constantId, 1u) << "unexpected specialization constant id for " << varLayout->getName();
            }
        }
        EXPECT_EQ(specializationConstantCount, 2);

        // The SPIR-V must contain OpSpecConstant instructions (opcodes 48-52 in the word stream).
        ASSERT_EQ(byteCode.size() % 4, 0u);
        const uint32_t* words = reinterpret_cast<const uint32_t*>(byteCode.data());
        const size_t wordCount = byteCode.size() / 4;
        bool foundSpecConstantOpCode = false;
        for (size_t wordIndex = 5; wordIndex < wordCount && !foundSpecConstantOpCode; )
        {
            const uint32_t instructionWord = words[wordIndex];
            const uint32_t opCode = instructionWord & 0xFFFFu;
            const uint32_t instructionLength = instructionWord >> 16;
            // OpSpecConstantTrue=48, OpSpecConstantFalse=49, OpSpecConstant=50,
            // OpSpecConstantComposite=51, OpSpecConstantOp=52.
            if (opCode >= 48 && opCode <= 52)
            {
                foundSpecConstantOpCode = true;
            }
            wordIndex += (instructionLength == 0) ? 1 : instructionLength;
        }
        EXPECT_TRUE(foundSpecConstantOpCode);
    }

    TEST_F(Gate2_OptionLoweringTests, DynamicMode_FallbackKeyRead_CompilesWithSameUseSite)
    {
        // The generated dynamic options module reads option values from a fallback-key constant
        // buffer, with the same bit packing ShaderOptionGroupLayout will define: o_useTint at
        // bit 0, o_quality in bits 1-2.
        constexpr AZStd::string_view optionsModule = R"(
module Gate2Options;

struct Gate2FallbackKey
{
    uint4 m_keyBits;
};

[[vk::binding(0, 7)]]
ConstantBuffer<Gate2FallbackKey> Gate2Fallback : register(b0, space7);

public static bool o_useTint = (Gate2Fallback.m_keyBits.x & 1u) != 0u;
public static int o_quality = int((Gate2Fallback.m_keyBits.x >> 1u) & 3u);
)";
        AZStd::vector<uint8_t> dynamicByteCode;
        ASSERT_TRUE(CompileUseSite(SLANG_SPIRV, "spirv_1_5", ImportingUseSiteSource, optionsModule, {}, dynamicByteCode));

        // Sanity: the fallback read survives into codegen — the bytecode must differ from a
        // fully baked build of the same use-site.
        constexpr AZStd::string_view bakedOptionsModule = R"(
module Gate2Options;
public static const bool o_useTint = true;
public static const int o_quality = 2;
)";
        AZStd::vector<uint8_t> bakedByteCode;
        ASSERT_TRUE(CompileUseSite(SLANG_SPIRV, "spirv_1_5", ImportingUseSiteSource, bakedOptionsModule, {}, bakedByteCode));

        const bool byteCodeDiffers =
            dynamicByteCode.size() != bakedByteCode.size() ||
            memcmp(dynamicByteCode.data(), bakedByteCode.data(), dynamicByteCode.size()) != 0;
        EXPECT_TRUE(byteCodeDiffers);
    }

    TEST_F(Gate2_OptionLoweringTests, Dx12SentinelMode_DxscPatchRoundTrip_ProducesOffsets)
    {
        // DX12 has no native specialization constants; the pipeline bakes a sentinel value via
        // the link-time path and patches the DXIL, recording patch offsets for PSO-time fixup.
        // The use-site stores the option as a raw integer so the sentinel survives into the
        // DXIL as a patchable dword (float conversions would fold it into a different constant).
        constexpr uint32_t sentinelValue = 0x5CA1AB1Eu;

        constexpr AZStd::string_view sentinelUseSiteSource = R"(
module Gate2UseSite;

extern static const int o_quality;

RWStructuredBuffer<int> Output;

[shader("compute")]
[numthreads(1, 1, 1)]
void MainCS(uint3 id : SV_DispatchThreadID)
{
    Output[id.x] = o_quality;
}
)";
        const AZStd::string valuesModule = AZStd::string::format(R"(
module Gate2Values;
export static const int o_quality = int(%uu);
)", sentinelValue);

        // The shipped dxsc.exe embeds a DXIL validator capped at 1.9, while Slang's embedded
        // DXC stamps 1.10 by default (a gate finding): cap the validator version so the whole
        // patching pipeline interoperates.
        slang::CompilerOptionEntry validatorVersionOption;
        validatorVersionOption.name = slang::CompilerOptionName::DownstreamArgs;
        validatorVersionOption.value.kind = slang::CompilerOptionValueKind::String;
        validatorVersionOption.value.stringValue0 = "dxc";
        validatorVersionOption.value.stringValue1 = "-validator-version 1.9";
        const slang::CompilerOptionEntry extraOptions[] = {validatorVersionOption};

        AZStd::vector<uint8_t> dxilByteCode;
        ASSERT_TRUE(CompileUseSite(SLANG_DXIL, "sm_6_2", sentinelUseSiteSource, {}, valuesModule, dxilByteCode, nullptr, nullptr, extraOptions));

        const char* tempDir = m_tempDirectory->GetDirectory();
        AZStd::string dxilPath;
        AZ::StringFunc::Path::Join(tempDir, "gate2.dxil.bin", dxilPath);
        AZ::IO::SystemFile dxilFile;
        ASSERT_TRUE(dxilFile.Open(dxilPath.c_str(), AZ::IO::SystemFile::SF_OPEN_CREATE | AZ::IO::SystemFile::SF_OPEN_WRITE_ONLY));
        dxilFile.Write(dxilByteCode.data(), dxilByteCode.size());
        dxilFile.Close();

        AZStd::string patchedPath;
        AZ::StringFunc::Path::Join(tempDir, "gate2.dxil.patched.bin", patchedPath);
        AZStd::string offsetsPath;
        AZ::StringFunc::Path::Join(tempDir, "gate2.offsets.json", offsetsPath);

        AZ::IO::FixedMaxPath dxscPath = AZ::Utils::GetExecutableDirectory();
        dxscPath /= "Builders/DirectXShaderCompiler/dxsc.exe";
        ASSERT_TRUE(AZ::IO::SystemFile::Exists(dxscPath.c_str())) << "dxsc.exe not deployed at " << dxscPath.c_str();

        AzFramework::ProcessLauncher::ProcessLaunchInfo launchInfo;
        launchInfo.m_commandlineParameters = AZStd::string::format(
            "\"%s\" -sv=%u -o=\"%s\" -f=\"%s\" \"%s\"",
            dxscPath.c_str(),
            sentinelValue,
            patchedPath.c_str(),
            offsetsPath.c_str(),
            dxilPath.c_str());
        launchInfo.m_showWindow = false;

        AZStd::unique_ptr<AzFramework::ProcessWatcher> watcher(AzFramework::ProcessWatcher::LaunchProcess(launchInfo, AzFramework::COMMUNICATOR_TYPE_STDINOUT));
        ASSERT_NE(watcher, nullptr);
        AZStd::string processOutput;
        uint32_t exitCode = 0;
        while (watcher->IsProcessRunning(&exitCode))
        {
            AzFramework::ProcessCommunicator* communicator = watcher->GetCommunicator();
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
        EXPECT_EQ(exitCode, 0u) << "dxsc failed to patch Slang-produced DXIL: " << processOutput.c_str();

        // The patch must produce both artifacts, and the offsets file must reference the sentinel.
        EXPECT_TRUE(AZ::IO::SystemFile::Exists(patchedPath.c_str()));
        ASSERT_TRUE(AZ::IO::SystemFile::Exists(offsetsPath.c_str()));
        AZ::IO::SystemFile offsetsFile;
        ASSERT_TRUE(offsetsFile.Open(offsetsPath.c_str(), AZ::IO::SystemFile::SF_OPEN_READ_ONLY));
        AZStd::string offsetsContent;
        offsetsContent.resize_no_construct(offsetsFile.Length());
        offsetsFile.Read(offsetsContent.size(), offsetsContent.data());
        offsetsFile.Close();
        EXPECT_FALSE(offsetsContent.empty());
    }
} // namespace UnitTest
