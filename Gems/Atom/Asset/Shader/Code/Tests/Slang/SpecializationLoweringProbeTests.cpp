/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// M11 probe: the DX12 specialization-constant leg. AZSLC blocks constant folding of the
// patchable specialization id with a `volatile int` local (DirectX12PlatformEmitter.cpp) so the
// id survives into the DXIL as a discrete dword dxsc.exe can find and replace with
// SCSentinelValue|id. Slang reaches DXIL by emitting HLSL into DXC — so the question is which
// Slang construct survives to the emitted HLSL as an equivalent fold blocker.

#include <AzTest/AzTest.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzCore/IO/Path/Path.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/Serialization/Json/JsonUtils.h>
#include <AzCore/Utils/Utils.h>
#include <AzFramework/Process/ProcessCommunicator.h>
#include <AzFramework/Process/ProcessWatcher.h>

#include <Atom/RHI.Edit/Utils.h>

#include <Common/ShaderBuilderTestFixture.h>

#include <Slang/SlangCompilerService.h>

namespace UnitTest
{
    using namespace AZ;
    using ShaderBuilder::SlangCompilerService;

    class SpecializationLoweringProbeTests : public ShaderBuilderTestFixture
    {
    public:
        //! Compiles @source to HLSL text (SLANG_HLSL target) and returns the emitted code, or
        //! empty on failure with diagnostics printed.
        static AZStd::string EmitHlsl(AZStd::string_view source)
        {
            SlangCompilerService& service = SlangCompilerService::Get();
            const AZStd::unique_lock<AZStd::recursive_mutex> compilerLock = service.AcquireCompilerLock();

            SlangCompilerService::SessionDescriptor sessionDescriptor;
            sessionDescriptor.m_target = SLANG_HLSL;
            sessionDescriptor.m_profile = "sm_6_2";
            auto sessionOutcome = service.CreateSession(sessionDescriptor);
            if (!sessionOutcome.IsSuccess())
            {
                ADD_FAILURE() << sessionOutcome.GetError().c_str();
                return {};
            }
            Slang::ComPtr<slang::ISession> session = sessionOutcome.TakeValue();

            Slang::ComPtr<slang::IBlob> diagnostics;
            slang::IModule* probeModule = session->loadModuleFromSourceString(
                "SpecProbe", "SpecProbe.slang", AZStd::string(source).c_str(), diagnostics.writeRef());
            if (diagnostics && diagnostics->getBufferSize() > 0)
            {
                printf("[PROBE]   diagnostics: %.*s\n",
                    static_cast<int>(diagnostics->getBufferSize()),
                    static_cast<const char*>(diagnostics->getBufferPointer()));
            }
            if (!probeModule)
            {
                return {};
            }

            Slang::ComPtr<slang::IEntryPoint> entryPoint;
            diagnostics = nullptr;
            probeModule->findAndCheckEntryPoint("MainCS", SLANG_STAGE_COMPUTE, entryPoint.writeRef(), diagnostics.writeRef());
            if (!entryPoint)
            {
                return {};
            }

            slang::IComponentType* components[] = {probeModule, entryPoint.get()};
            Slang::ComPtr<slang::IComponentType> composedProgram;
            diagnostics = nullptr;
            if (SLANG_FAILED(session->createCompositeComponentType(components, 2, composedProgram.writeRef(), diagnostics.writeRef())))
            {
                return {};
            }
            Slang::ComPtr<slang::IComponentType> linkedProgram;
            diagnostics = nullptr;
            if (SLANG_FAILED(composedProgram->link(linkedProgram.writeRef(), diagnostics.writeRef())))
            {
                return {};
            }
            Slang::ComPtr<slang::IBlob> code;
            diagnostics = nullptr;
            const SlangResult codeResult = linkedProgram->getEntryPointCode(0, 0, code.writeRef(), diagnostics.writeRef());
            if (diagnostics && diagnostics->getBufferSize() > 0)
            {
                printf("[PROBE]   codegen diagnostics: %.*s\n",
                    static_cast<int>(diagnostics->getBufferSize()),
                    static_cast<const char*>(diagnostics->getBufferPointer()));
            }
            if (SLANG_FAILED(codeResult) || !code)
            {
                return {};
            }
            return AZStd::string(static_cast<const char*>(code->getBufferPointer()), code->getBufferSize());
        }
    };

    TEST_F(SpecializationLoweringProbeTests, FoldBlockers_WhatSurvivesToEmittedHlsl)
    {
        // Candidate A: `volatile` on a local — the exact AZSLC pattern, if Slang parses it
        constexpr AZStd::string_view volatileSource = R"(
module SpecProbe;

int ReadSpecializationId()
{
    volatile int specializationValue = 7;
    return specializationValue;
}

RWStructuredBuffer<int> Output;

[shader("compute")]
[numthreads(1, 1, 1)]
void MainCS(uint3 id : SV_DispatchThreadID)
{
    if (ReadSpecializationId() == 2)
    {
        Output[id.x] = 1;
    }
}
)";
        const AZStd::string volatileHlsl = EmitHlsl(volatileSource);
        printf("[PROBE] `volatile int` local parses and emits: %s\n", volatileHlsl.empty() ? "NO" : "YES");
        if (!volatileHlsl.empty())
        {
            printf("[PROBE]   emitted HLSL contains 'volatile': %s\n",
                volatileHlsl.find("volatile") != AZStd::string::npos ? "YES" : "NO");
            printf("[PROBE]   emitted HLSL contains the id literal '7': %s\n",
                volatileHlsl.find(" 7") != AZStd::string::npos || volatileHlsl.find("= 7") != AZStd::string::npos ? "YES" : "NO");
        }

        // Candidate B: a mutable static global initialized with the id — survives as a static
        // in HLSL, but DXC may still fold reads if it proves no writes
        constexpr AZStd::string_view mutableStaticSource = R"(
module SpecProbe;

static int s_specializationValue = 7;

RWStructuredBuffer<int> Output;

[shader("compute")]
[numthreads(1, 1, 1)]
void MainCS(uint3 id : SV_DispatchThreadID)
{
    if (s_specializationValue == 2)
    {
        Output[id.x] = 1;
    }
}
)";
        const AZStd::string mutableStaticHlsl = EmitHlsl(mutableStaticSource);
        printf("[PROBE] mutable static global emits: %s\n", mutableStaticHlsl.empty() ? "NO" : "YES");
        if (!mutableStaticHlsl.empty())
        {
            const bool comparisonSurvives = mutableStaticHlsl.find("== 2") != AZStd::string::npos
                || mutableStaticHlsl.find("2 ==") != AZStd::string::npos;
            printf("[PROBE]   emitted HLSL still contains the comparison (not folded by Slang): %s\n",
                comparisonSurvives ? "YES" : "NO");
        }
        if (!volatileHlsl.empty())
        {
            printf("[PROBE] ---- volatile variant emitted HLSL ----\n%s\n[PROBE] ---- end ----\n", volatileHlsl.c_str());
        }
    }

    TEST_F(SpecializationLoweringProbeTests, FoldBlockers_RoundTwo)
    {
        // Candidate C: [vk::constant_id] on the DXIL/HLSL target — what does Slang lower a real
        // specialization constant to when the target has none?
        constexpr AZStd::string_view specConstantSource = R"(
module SpecProbe;

[vk::constant_id(7)]
const int s_quality = 1;

RWStructuredBuffer<int> Output;

[shader("compute")]
[numthreads(1, 1, 1)]
void MainCS(uint3 id : SV_DispatchThreadID)
{
    if (s_quality == 2)
    {
        Output[id.x] = 1;
    }
}
)";
        const AZStd::string specConstantHlsl = EmitHlsl(specConstantSource);
        printf("[PROBE] [vk::constant_id] on HLSL target emits: %s\n", specConstantHlsl.empty() ? "NO" : "YES");
        if (!specConstantHlsl.empty())
        {
            printf("[PROBE] ---- spec-constant variant emitted HLSL ----\n%s\n[PROBE] ---- end ----\n", specConstantHlsl.c_str());
        }

        // Candidate D: __intrinsic_asm text splice — emits the AZSLC volatile pattern verbatim
        // into the generated HLSL
        constexpr AZStd::string_view intrinsicAsmSource = R"SLANG(
module SpecProbe;

int ReadSpecializationId(int specializationId)
{
    __intrinsic_asm "AtomReadSpecializationConstant($0)";
}

RWStructuredBuffer<int> Output;

[shader("compute")]
[numthreads(1, 1, 1)]
void MainCS(uint3 id : SV_DispatchThreadID)
{
    if (ReadSpecializationId(7) == 2)
    {
        Output[id.x] = 1;
    }
}
)SLANG";
        const AZStd::string intrinsicAsmHlsl = EmitHlsl(intrinsicAsmSource);
        printf("[PROBE] __intrinsic_asm splice emits: %s\n", intrinsicAsmHlsl.empty() ? "NO" : "YES");
        if (!intrinsicAsmHlsl.empty())
        {
            printf("[PROBE]   emitted HLSL contains the splice: %s\n",
                intrinsicAsmHlsl.find("AtomReadSpecializationConstant") != AZStd::string::npos ? "YES" : "NO");
            printf("[PROBE]   comparison survives: %s\n",
                intrinsicAsmHlsl.find("== 2") != AZStd::string::npos || intrinsicAsmHlsl.find("== int(2)") != AZStd::string::npos
                    ? "YES" : "NO");
        }
    }

    TEST_F(SpecializationLoweringProbeTests, Dx12SpliceLeg_TwoOptionDxscRoundTrip)
    {
        // The production DXIL spec-mode shape end to end: the generated implementation module
        // routes each option function through the __intrinsic_asm splice into the service's
        // HLSL-prelude volatile helper, both options feed BRANCHES (the constant-folding
        // hazard), and dxsc.exe must find both specialization ids and report their patch
        // offsets.
        constexpr AZStd::string_view useSiteSource = R"(
module SpecProbe;

public extern bool o_useTint();
public extern int o_quality();

RWStructuredBuffer<int> Output;

[shader("compute")]
[numthreads(1, 1, 1)]
void MainCS(uint3 id : SV_DispatchThreadID)
{
    int value = 0;
    if (o_useTint())
    {
        value += 1;
    }
    if (o_quality() == 2)
    {
        value += 10;
    }
    Output[id.x] = value;
}
)";
        constexpr AZStd::string_view implementationSource = R"SLANG(
module SpecImplementation;
import SpecProbe;

int AtomReadSpecializationConstantRaw(int specializationId)
{
    __intrinsic_asm "AtomReadSpecializationConstant($0)";
}

export bool o_useTint() { return AtomReadSpecializationConstantRaw(0) != 0; }
export int o_quality() { return AtomReadSpecializationConstantRaw(1); }
)SLANG";

        AZStd::vector<uint8_t> dxilByteCode;
        {
            SlangCompilerService& service = SlangCompilerService::Get();
            const AZStd::unique_lock<AZStd::recursive_mutex> compilerLock = service.AcquireCompilerLock();

            SlangCompilerService::SessionDescriptor sessionDescriptor;
            sessionDescriptor.m_target = SLANG_DXIL;
            sessionDescriptor.m_profile = "sm_6_2";
            // The shipped dxsc.exe embeds a DXIL validator capped at 1.9 (gate 2 finding)
            slang::CompilerOptionEntry validatorVersionOption = {};
            validatorVersionOption.name = slang::CompilerOptionName::DownstreamArgs;
            validatorVersionOption.value.kind = slang::CompilerOptionValueKind::String;
            validatorVersionOption.value.stringValue0 = "dxc";
            validatorVersionOption.value.stringValue1 = "-validator-version 1.9";
            sessionDescriptor.m_extraOptions.push_back(validatorVersionOption);

            auto sessionOutcome = service.CreateSession(sessionDescriptor);
            ASSERT_TRUE(sessionOutcome.IsSuccess());
            Slang::ComPtr<slang::ISession> session = sessionOutcome.TakeValue();

            Slang::ComPtr<slang::IBlob> diagnostics;
            slang::IModule* useSiteModule = session->loadModuleFromSourceString(
                "SpecProbe", "SpecProbe.slang", AZStd::string(useSiteSource).c_str(), diagnostics.writeRef());
            SlangCompilerService::ReportDiagnostics("SpecProbe", diagnostics, !useSiteModule);
            ASSERT_NE(useSiteModule, nullptr);

            Slang::ComPtr<slang::IEntryPoint> entryPoint;
            diagnostics = nullptr;
            useSiteModule->findAndCheckEntryPoint("MainCS", SLANG_STAGE_COMPUTE, entryPoint.writeRef(), diagnostics.writeRef());
            ASSERT_NE(entryPoint, nullptr);

            diagnostics = nullptr;
            slang::IModule* implementationModule = session->loadModuleFromSourceString(
                "SpecImplementation", "SpecImplementation.slang", AZStd::string(implementationSource).c_str(), diagnostics.writeRef());
            SlangCompilerService::ReportDiagnostics("SpecImplementation", diagnostics, !implementationModule);
            ASSERT_NE(implementationModule, nullptr);

            slang::IComponentType* components[] = {useSiteModule, implementationModule, entryPoint.get()};
            Slang::ComPtr<slang::IComponentType> composedProgram;
            diagnostics = nullptr;
            ASSERT_TRUE(SLANG_SUCCEEDED(session->createCompositeComponentType(components, 3, composedProgram.writeRef(), diagnostics.writeRef())));
            Slang::ComPtr<slang::IComponentType> linkedProgram;
            diagnostics = nullptr;
            ASSERT_TRUE(SLANG_SUCCEEDED(composedProgram->link(linkedProgram.writeRef(), diagnostics.writeRef())));

            Slang::ComPtr<slang::IBlob> code;
            diagnostics = nullptr;
            const SlangResult codeResult = linkedProgram->getEntryPointCode(0, 0, code.writeRef(), diagnostics.writeRef());
            SlangCompilerService::ReportDiagnostics("SpecProbe", diagnostics, SLANG_FAILED(codeResult));
            ASSERT_TRUE(SLANG_SUCCEEDED(codeResult));
            ASSERT_NE(code, nullptr);
            const uint8_t* bytes = static_cast<const uint8_t*>(code->getBufferPointer());
            dxilByteCode.assign(bytes, bytes + code->getBufferSize());
        }

        // Round trip through dxsc with the production sentinel (DX12 SCSentinelValue)
        AZ::Test::ScopedAutoTempDirectory tempDirectory;
        const char* tempDir = tempDirectory.GetDirectory();
        AZ::IO::FixedMaxPath dxilPath(tempDir);
        dxilPath /= "specprobe.dxil.bin";
        {
            AZ::IO::SystemFile dxilFile;
            ASSERT_TRUE(dxilFile.Open(dxilPath.c_str(), AZ::IO::SystemFile::SF_OPEN_CREATE | AZ::IO::SystemFile::SF_OPEN_WRITE_ONLY));
            dxilFile.Write(dxilByteCode.data(), dxilByteCode.size());
        }
        AZ::IO::FixedMaxPath patchedPath(tempDir);
        patchedPath /= "specprobe.dxil.patched.bin";
        AZ::IO::FixedMaxPath offsetsPath(tempDir);
        offsetsPath /= "specprobe.offsets.json";

        AZ::IO::FixedMaxPath dxscPath = AZ::Utils::GetExecutableDirectory();
        dxscPath /= "Builders/DirectXShaderCompiler/dxsc.exe";
        ASSERT_TRUE(AZ::IO::SystemFile::Exists(dxscPath.c_str())) << "dxsc.exe not deployed at " << dxscPath.c_str();

        AzFramework::ProcessLauncher::ProcessLaunchInfo launchInfo;
        launchInfo.m_commandlineParameters = AZStd::string::format(
            "\"%s\" -sv=%lu -o=\"%s\" -f=\"%s\" \"%s\"",
            dxscPath.c_str(),
            0x45678900ul, // DX12 SCSentinelValue
            patchedPath.c_str(),
            offsetsPath.c_str(),
            dxilPath.c_str());
        launchInfo.m_showWindow = false;
        AZStd::unique_ptr<AzFramework::ProcessWatcher> watcher(
            AzFramework::ProcessWatcher::LaunchProcess(launchInfo, AzFramework::COMMUNICATOR_TYPE_STDINOUT));
        ASSERT_NE(watcher, nullptr);
        AZStd::string processOutput;
        uint32_t exitCode = 0;
        while (watcher->IsProcessRunning(&exitCode))
        {
            if (const AZ::u32 byteCount = watcher->GetCommunicator()->PeekOutput())
            {
                AZStd::string chunk;
                chunk.resize_no_construct(byteCount);
                watcher->GetCommunicator()->ReadOutput(chunk.data(), byteCount);
                processOutput += chunk;
            }
        }
        printf("[PROBE] dxsc on splice-form Slang DXIL: exit %u\n", exitCode);
        ASSERT_EQ(exitCode, 0u) << processOutput.c_str();

        // The offsets file must map BOTH specialization ids to patch offsets
        auto jsonOutcome = AZ::JsonSerializationUtils::ReadJsonFile(offsetsPath.String());
        ASSERT_TRUE(jsonOutcome.IsSuccess()) << jsonOutcome.GetError().c_str();
        const rapidjson::Document& offsetsDocument = jsonOutcome.GetValue();
        AZStd::vector<AZStd::string> offsetIds;
        for (auto memberIterator = offsetsDocument.MemberBegin(); memberIterator != offsetsDocument.MemberEnd(); ++memberIterator)
        {
            offsetIds.push_back(memberIterator->name.GetString());
            printf("[PROBE]   offsets.json id %s -> %u\n", memberIterator->name.GetString(), memberIterator->value.GetUint());
        }
        EXPECT_EQ(offsetIds.size(), 2);
        EXPECT_TRUE(AZStd::find(offsetIds.begin(), offsetIds.end(), "0") != offsetIds.end());
        EXPECT_TRUE(AZStd::find(offsetIds.begin(), offsetIds.end(), "1") != offsetIds.end());

        // And the patched DXIL must differ from the input (sentinels written in)
        AZ::IO::SystemFile patchedFile;
        ASSERT_TRUE(patchedFile.Open(patchedPath.c_str(), AZ::IO::SystemFile::SF_OPEN_READ_ONLY));
        AZStd::vector<uint8_t> patchedByteCode;
        patchedByteCode.resize_no_construct(patchedFile.Length());
        patchedFile.Read(patchedByteCode.size(), patchedByteCode.data());
        patchedFile.Close();
        ASSERT_FALSE(patchedByteCode.empty());
        const bool patchedDiffers = patchedByteCode.size() != dxilByteCode.size()
            || memcmp(patchedByteCode.data(), dxilByteCode.data(), dxilByteCode.size()) != 0;
        EXPECT_TRUE(patchedDiffers);
    }
} // namespace UnitTest
