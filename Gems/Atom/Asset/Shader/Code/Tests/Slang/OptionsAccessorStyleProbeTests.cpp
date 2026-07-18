/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// M10 authoring-surface probe, accessor variant: can option use-sites route through a FUNCTION
// instead of a constant? A function body may return a link-time constant, a specialization
// constant, or a fallback-buffer read — so ONE authored declaration could serve all three modes
// with no #ifdef guard and no identifier collision with generated modules.
//
// Questions this probe answers against the pinned compiler:
// 1. Do global-scope `property` declarations exist (bare-identifier ergonomics)?
// 2. Can an `extern` FUNCTION declaration be satisfied by an `export` definition composed at
//    link time (the values-module pattern, but for functions)?
// 3. Does a baked (link-time constant) implementation still specialize codegen — different
//    values, different bytecode?

#include <AzTest/AzTest.h>
#include <AzCore/UnitTest/TestTypes.h>

#include <Common/ShaderBuilderTestFixture.h>

#include <Slang/SlangCompilerService.h>

namespace UnitTest
{
    using namespace AZ;
    using ShaderBuilder::SlangCompilerService;

    class OptionsAccessorStyleProbeTests : public ShaderBuilderTestFixture
    {
    public:
        //! Compiles @moduleSource (+ optional composed @implementationSource module) to SPIR-V.
        static bool CompileProgram(
            AZStd::string_view moduleSource,
            AZStd::string_view implementationSource,
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
                "AccessorProbe", "AccessorProbe.slang", AZStd::string(moduleSource).c_str(), diagnostics.writeRef());
            SlangCompilerService::ReportDiagnostics("AccessorProbe", diagnostics, !useSiteModule);
            if (!useSiteModule)
            {
                return false;
            }

            Slang::ComPtr<slang::IEntryPoint> entryPoint;
            diagnostics = nullptr;
            useSiteModule->findAndCheckEntryPoint("MainCS", SLANG_STAGE_COMPUTE, entryPoint.writeRef(), diagnostics.writeRef());
            SlangCompilerService::ReportDiagnostics("AccessorProbe", diagnostics, !entryPoint);
            if (!entryPoint)
            {
                return false;
            }

            AZStd::vector<slang::IComponentType*> components;
            components.push_back(useSiteModule);
            components.push_back(entryPoint.get());

            if (!implementationSource.empty())
            {
                diagnostics = nullptr;
                slang::IModule* implementationModule = session->loadModuleFromSourceString(
                    "AccessorImplementation", "AccessorImplementation.slang", AZStd::string(implementationSource).c_str(), diagnostics.writeRef());
                SlangCompilerService::ReportDiagnostics("AccessorImplementation", diagnostics, !implementationModule);
                if (!implementationModule)
                {
                    return false;
                }
                components.push_back(implementationModule);
            }

            Slang::ComPtr<slang::IComponentType> composedProgram;
            diagnostics = nullptr;
            if (SLANG_FAILED(session->createCompositeComponentType(components.data(), components.size(), composedProgram.writeRef(), diagnostics.writeRef())))
            {
                SlangCompilerService::ReportDiagnostics("AccessorProbe", diagnostics, true);
                return false;
            }

            Slang::ComPtr<slang::IComponentType> linkedProgram;
            diagnostics = nullptr;
            if (SLANG_FAILED(composedProgram->link(linkedProgram.writeRef(), diagnostics.writeRef())))
            {
                SlangCompilerService::ReportDiagnostics("AccessorProbe", diagnostics, true);
                return false;
            }

            Slang::ComPtr<slang::IBlob> byteCode;
            diagnostics = nullptr;
            if (SLANG_FAILED(linkedProgram->getEntryPointCode(0, 0, byteCode.writeRef(), diagnostics.writeRef())) || !byteCode)
            {
                SlangCompilerService::ReportDiagnostics("AccessorProbe", diagnostics, true);
                return false;
            }
            const uint8_t* bytes = static_cast<const uint8_t*>(byteCode->getBufferPointer());
            outByteCode.assign(bytes, bytes + byteCode->getBufferSize());
            return true;
        }
    };

    TEST_F(OptionsAccessorStyleProbeTests, GlobalProperty_BareIdentifierUseSite)
    {
        // A global property with a getter: use-sites read `o_useTint` with no parens
        constexpr AZStd::string_view moduleSource = R"(
module AccessorProbe;

property bool o_useTint
{
    get { return true; }
}

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
    Output[id.x] = value;
}
)";
        AZStd::vector<uint8_t> byteCode;
        const bool compiled = CompileProgram(moduleSource, {}, byteCode);
        printf("[PROBE] global property with bare-identifier use-site: %s\n", compiled ? "SUPPORTED" : "NOT SUPPORTED");
        EXPECT_TRUE(compiled);
    }

    TEST_F(OptionsAccessorStyleProbeTests, ExternFunction_SatisfiedByExportAtLink)
    {
        // The authored declaration: an extern accessor FUNCTION (not a const) — every lowering
        // can implement a function body
        constexpr AZStd::string_view useSiteSource = R"(
module AccessorProbe;

extern bool AtomOptionImpl_o_useTint();

property bool o_useTint
{
    get { return AtomOptionImpl_o_useTint(); }
}

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
    Output[id.x] = value;
}
)";
        constexpr AZStd::string_view bakedTrueImplementation = R"(
module AccessorImplementation;
export bool AtomOptionImpl_o_useTint() { return true; }
)";
        constexpr AZStd::string_view bakedFalseImplementation = R"(
module AccessorImplementation;
export bool AtomOptionImpl_o_useTint() { return false; }
)";

        AZStd::vector<uint8_t> byteCodeTrue;
        const bool compiledTrue = CompileProgram(useSiteSource, bakedTrueImplementation, byteCodeTrue);
        printf("[PROBE] extern function satisfied by export at link: %s\n", compiledTrue ? "SUPPORTED" : "NOT SUPPORTED");
        ASSERT_TRUE(compiledTrue);

        AZStd::vector<uint8_t> byteCodeFalse;
        ASSERT_TRUE(CompileProgram(useSiteSource, bakedFalseImplementation, byteCodeFalse));

        // Link-time constant implementations must specialize codegen
        const bool byteCodeDiffers =
            byteCodeTrue.size() != byteCodeFalse.size() ||
            memcmp(byteCodeTrue.data(), byteCodeFalse.data(), byteCodeTrue.size()) != 0;
        printf("[PROBE] baked accessor values specialize codegen: %s\n", byteCodeDiffers ? "YES" : "NO");
        EXPECT_TRUE(byteCodeDiffers);
    }

    TEST_F(OptionsAccessorStyleProbeTests, ExternFunction_SpecConstantAndDynamicImplementations)
    {
        constexpr AZStd::string_view useSiteSource = R"(
module AccessorProbe;

extern bool AtomOptionImpl_o_useTint();
extern int AtomOptionImpl_o_quality();

RWStructuredBuffer<float4> Output;

[shader("compute")]
[numthreads(1, 1, 1)]
void MainCS(uint3 id : SV_DispatchThreadID)
{
    float4 value = float4(1.0, 1.0, 1.0, 1.0);
    if (AtomOptionImpl_o_useTint())
    {
        value *= 0.5;
    }
    value.x += float(AtomOptionImpl_o_quality());
    Output[id.x] = value;
}
)";
        // Specialization-constant bodies
        constexpr AZStd::string_view specImplementation = R"(
module AccessorImplementation;

[vk::constant_id(0)]
const bool s_useTint = false;

[vk::constant_id(1)]
const int s_quality = 1;

export bool AtomOptionImpl_o_useTint() { return s_useTint; }
export int AtomOptionImpl_o_quality() { return s_quality; }
)";
        AZStd::vector<uint8_t> specByteCode;
        const bool specCompiled = CompileProgram(useSiteSource, specImplementation, specByteCode);
        printf("[PROBE] spec-constant accessor implementation: %s\n", specCompiled ? "SUPPORTED" : "NOT SUPPORTED");
        EXPECT_TRUE(specCompiled);

        // Dynamic fallback-read bodies
        constexpr AZStd::string_view dynamicImplementation = R"(
module AccessorImplementation;

struct FallbackKey
{
    uint4 m_keyBits;
};

ConstantBuffer<FallbackKey> OptionsFallback;

export bool AtomOptionImpl_o_useTint() { return (OptionsFallback.m_keyBits.x & 1u) != 0u; }
export int AtomOptionImpl_o_quality() { return int((OptionsFallback.m_keyBits.x >> 1u) & 3u); }
)";
        AZStd::vector<uint8_t> dynamicByteCode;
        const bool dynamicCompiled = CompileProgram(useSiteSource, dynamicImplementation, dynamicByteCode);
        printf("[PROBE] dynamic fallback-read accessor implementation: %s\n", dynamicCompiled ? "SUPPORTED" : "NOT SUPPORTED");
        EXPECT_TRUE(dynamicCompiled);
    }
} // namespace UnitTest
