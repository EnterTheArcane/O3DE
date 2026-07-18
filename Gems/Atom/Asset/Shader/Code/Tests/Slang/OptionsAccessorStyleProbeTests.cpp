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
    // The production ATOM_OPTION surface (M10 wiring): the macro expands to an attributed extern
    // accessor plus a property, the impl ABI is always `int AtomOptionImpl_<name>()` so generated
    // modules never need visibility into author-declared enum types, and the attribute carries the
    // stringized type and default so discovery reads functions only.
    //
    // Every piece the wiring depends on is probed here against the pinned compiler:
    // - #/## stringize and paste in Slang's preprocessor
    // - user attributes on FUNCTION declarations, readable through decl reflection
    // - `(type)` cast from the int accessor back to bool and to a Slang enum
    // - enum cases enumerable from the module decl tree (Kind::Enum children)
    // - bidirectional extern/export linking (authored exports the fallback-key getter the
    //   generated module calls, generated exports the accessors the authored module calls)

    static constexpr AZStd::string_view AtomOptionProbePreamble = R"(
[__AttributeUsage(_AttributeTargets.Function)]
struct AtomOptionAttribute
{
    string typeName;
    string defaultValue;
};

#define ATOM_OPTION(type, name, defaultValue) [AtomOption(#type, #defaultValue)] public extern int AtomOptionImpl_##name(); public property type name { get { return (type)AtomOptionImpl_##name(); } }
)";

    TEST_F(OptionsAccessorStyleProbeTests, AtomOptionMacro_ExpansionDiscoveryAndEnumCases)
    {
        const AZStd::string useSiteSource = AZStd::string("\nmodule AccessorProbe;\n")
            + AZStd::string(AtomOptionProbePreamble)
            + R"(
public enum QualityT
{
    Low,
    Medium,
    High,
}

ATOM_OPTION(bool, o_useTint, true)
ATOM_OPTION(int, o_iterations, 4)
ATOM_OPTION(QualityT, o_quality, QualityT.Medium)

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
    for (int i = 0; i < o_iterations; ++i)
    {
        value.y += 0.125;
    }
    if (o_quality == QualityT.High)
    {
        value.z = 0.0;
    }
    Output[id.x] = value;
}
)";
        constexpr AZStd::string_view bakedImplementation = R"(
module AccessorImplementation;
export int AtomOptionImpl_o_useTint() { return 1; }
export int AtomOptionImpl_o_iterations() { return 4; }
export int AtomOptionImpl_o_quality() { return 1; }
)";

        AZStd::vector<uint8_t> byteCode;
        const bool compiled = CompileProgram(useSiteSource, bakedImplementation, byteCode);
        printf("[PROBE] ATOM_OPTION macro (# stringize, ## paste, (type) casts incl. enum): %s\n",
            compiled ? "SUPPORTED" : "NOT SUPPORTED");
        ASSERT_TRUE(compiled);

        // Discovery: load the use-site module alone and walk decl reflection for the
        // [AtomOption]-attributed accessor functions and the enum's cases
        SlangCompilerService& service = SlangCompilerService::Get();
        const AZStd::unique_lock<AZStd::recursive_mutex> compilerLock = service.AcquireCompilerLock();

        SlangCompilerService::SessionDescriptor sessionDescriptor;
        sessionDescriptor.m_target = SLANG_SPIRV;
        sessionDescriptor.m_profile = "spirv_1_5";
        auto sessionOutcome = service.CreateSession(sessionDescriptor);
        ASSERT_TRUE(sessionOutcome.IsSuccess());
        Slang::ComPtr<slang::ISession> session = sessionOutcome.TakeValue();

        Slang::ComPtr<slang::IBlob> diagnostics;
        slang::IModule* probeModule = session->loadModuleFromSourceString(
            "AccessorProbe", "AccessorProbe.slang", useSiteSource.c_str(), diagnostics.writeRef());
        SlangCompilerService::ReportDiagnostics("AccessorProbe", diagnostics, !probeModule);
        ASSERT_NE(probeModule, nullptr);

        struct DiscoveredOption
        {
            AZStd::string m_accessorName;
            AZStd::string m_typeName;
            AZStd::string m_defaultValue;
        };
        AZStd::vector<DiscoveredOption> discoveredOptions;
        AZStd::vector<AZStd::string> enumCaseNames;

        slang::DeclReflection* moduleDecl = probeModule->getModuleReflection();
        ASSERT_NE(moduleDecl, nullptr);
        for (unsigned childIndex = 0; childIndex < moduleDecl->getChildrenCount(); ++childIndex)
        {
            slang::DeclReflection* child = moduleDecl->getChild(childIndex);
            if (!child)
            {
                continue;
            }
            if (child->getKind() == slang::DeclReflection::Kind::Enum
                && child->getName() && azstricmp(child->getName(), "QualityT") == 0)
            {
                // Cases surface as Kind::Unsupported decls carrying the case name, in declaration
                // order, before the enum's synthesized `$__syn_*` operator functions
                for (unsigned caseIndex = 0; caseIndex < child->getChildrenCount(); ++caseIndex)
                {
                    slang::DeclReflection* caseDecl = child->getChild(caseIndex);
                    if (caseDecl
                        && caseDecl->getKind() == slang::DeclReflection::Kind::Unsupported
                        && caseDecl->getName() && caseDecl->getName()[0] != '\0' && caseDecl->getName()[0] != '$')
                    {
                        enumCaseNames.push_back(caseDecl->getName());
                    }
                }
                continue;
            }
            slang::FunctionReflection* function = child->getKind() == slang::DeclReflection::Kind::Func
                ? child->asFunction()
                : nullptr;
            if (!function)
            {
                continue;
            }
            for (unsigned attributeIndex = 0; attributeIndex < function->getUserAttributeCount(); ++attributeIndex)
            {
                slang::UserAttribute* attribute = function->getUserAttributeByIndex(attributeIndex);
                if (azstricmp(attribute->getName(), "AtomOption") != 0)
                {
                    continue;
                }
                size_t typeLength = 0;
                const char* typeText = attribute->getArgumentValueString(0, &typeLength);
                size_t defaultLength = 0;
                const char* defaultText = attribute->getArgumentValueString(1, &defaultLength);
                discoveredOptions.push_back({
                    function->getName(),
                    typeText ? AZStd::string(typeText, typeLength) : AZStd::string(),
                    defaultText ? AZStd::string(defaultText, defaultLength) : AZStd::string()});
            }
        }

        ASSERT_EQ(discoveredOptions.size(), 3);
        EXPECT_EQ(discoveredOptions[0].m_accessorName, "AtomOptionImpl_o_useTint");
        EXPECT_EQ(discoveredOptions[0].m_typeName, "bool");
        EXPECT_EQ(discoveredOptions[0].m_defaultValue, "true");
        EXPECT_EQ(discoveredOptions[1].m_accessorName, "AtomOptionImpl_o_iterations");
        EXPECT_EQ(discoveredOptions[1].m_typeName, "int");
        EXPECT_EQ(discoveredOptions[1].m_defaultValue, "4");
        EXPECT_EQ(discoveredOptions[2].m_accessorName, "AtomOptionImpl_o_quality");
        EXPECT_EQ(discoveredOptions[2].m_typeName, "QualityT");
        EXPECT_EQ(discoveredOptions[2].m_defaultValue, "QualityT.Medium");

        printf("[PROBE] enum cases via decl reflection: %zu found\n", enumCaseNames.size());
        ASSERT_EQ(enumCaseNames.size(), 3);
        EXPECT_EQ(enumCaseNames[0], "Low");
        EXPECT_EQ(enumCaseNames[1], "Medium");
        EXPECT_EQ(enumCaseNames[2], "High");
    }

    TEST_F(OptionsAccessorStyleProbeTests, AtomOptionMacro_DynamicFallbackBidirectionalLink)
    {
        // Dynamic mode's full shape: the authored module exports the fallback-key getter (reading
        // its own ParameterBlock, so the generated module needs no visibility into it) while the
        // generated module exports the accessors — extern/export in BOTH directions across the
        // same two components
        const AZStd::string useSiteSource = AZStd::string("\nmodule AccessorProbe;\n")
            + AZStd::string(AtomOptionProbePreamble)
            + R"(
struct ProbeShaderResourceGroup
{
    float4 m_color;
    uint4 m_shaderVariantKeyFallback;
};
ParameterBlock<ProbeShaderResourceGroup> ProbeSrg;

export uint4 Atom_GetShaderVariantKeyFallback() { return ProbeSrg.m_shaderVariantKeyFallback; }

ATOM_OPTION(bool, o_useTint, true)
ATOM_OPTION(int, o_iterations, 4)

RWStructuredBuffer<float4> Output;

[shader("compute")]
[numthreads(1, 1, 1)]
void MainCS(uint3 id : SV_DispatchThreadID)
{
    float4 value = ProbeSrg.m_color;
    if (o_useTint)
    {
        value *= 0.5;
    }
    for (int i = 0; i < o_iterations; ++i)
    {
        value.y += 0.125;
    }
    Output[id.x] = value;
}
)";
        constexpr AZStd::string_view dynamicImplementation = R"(
module AccessorImplementation;

extern uint4 Atom_GetShaderVariantKeyFallback();

export int AtomOptionImpl_o_useTint() { return int((Atom_GetShaderVariantKeyFallback()[0] >> 0u) & 0x1u); }
export int AtomOptionImpl_o_iterations() { return int((Atom_GetShaderVariantKeyFallback()[0] >> 1u) & 0x7u); }
)";

        AZStd::vector<uint8_t> byteCode;
        const bool compiled = CompileProgram(useSiteSource, dynamicImplementation, byteCode);
        printf("[PROBE] bidirectional extern/export link (fallback getter + accessors): %s\n",
            compiled ? "SUPPORTED" : "NOT SUPPORTED");
        EXPECT_TRUE(compiled);
    }
} // namespace UnitTest
