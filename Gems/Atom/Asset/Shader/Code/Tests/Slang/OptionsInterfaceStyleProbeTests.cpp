/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// M10 authoring-surface probe, interface variant: options as a link-time TYPE. The authored
// shader declares an interface of static option properties and an `extern struct` conforming to
// it; the builder composes a generated conforming implementation per mode at link time.
// Use-sites read namespaced statics (Options.useTint) — no guard, no bare-identifier
// collision machinery, and the interface is the compiler-checked declaration of record that the
// .shader descriptor metadata can be validated against.
//
// Questions answered against the pinned compiler:
// 1. Does `extern struct X : I;` + linked `export struct X : I { ... }` work at all?
// 2. Do static property getters cover all three lowerings (baked constant folds into codegen,
//    specialization constant, fallback-buffer read)?
// 3. Are the interface's requirements discoverable via declaration reflection, so the builder
//    can cross-validate them against the .shader option metadata?

#include <AzTest/AzTest.h>
#include <AzCore/UnitTest/TestTypes.h>

#include <Common/ShaderBuilderTestFixture.h>

#include <Slang/SlangCompilerService.h>

namespace UnitTest
{
    using namespace AZ;
    using ShaderBuilder::SlangCompilerService;

    class OptionsInterfaceStyleProbeTests : public ShaderBuilderTestFixture
    {
    public:
        static constexpr AZStd::string_view UseSiteSource = R"(
module InterfaceProbe;

public interface IOptions
{
    static bool useTint();
    static int quality();
}

extern struct Options : IOptions;

RWStructuredBuffer<float4> Output;

[shader("compute")]
[numthreads(1, 1, 1)]
void MainCS(uint3 id : SV_DispatchThreadID)
{
    float4 value = float4(1.0, 1.0, 1.0, 1.0);
    if (Options.useTint())
    {
        value *= 0.5;
    }
    value.x += float(Options.quality());
    Output[id.x] = value;
}
)";

        static bool CompileProgram(
            AZStd::string_view moduleSource,
            AZStd::string_view implementationSource,
            AZStd::vector<uint8_t>& outByteCode,
            slang::IModule** outUseSiteModule = nullptr,
            Slang::ComPtr<slang::ISession>* outSession = nullptr)
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
                "InterfaceProbe", "InterfaceProbe.slang", AZStd::string(moduleSource).c_str(), diagnostics.writeRef());
            SlangCompilerService::ReportDiagnostics("InterfaceProbe", diagnostics, !useSiteModule);
            if (!useSiteModule)
            {
                return false;
            }

            Slang::ComPtr<slang::IEntryPoint> entryPoint;
            diagnostics = nullptr;
            useSiteModule->findAndCheckEntryPoint("MainCS", SLANG_STAGE_COMPUTE, entryPoint.writeRef(), diagnostics.writeRef());
            SlangCompilerService::ReportDiagnostics("InterfaceProbe", diagnostics, !entryPoint);
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
                    "OptionsImplementation", "OptionsImplementation.slang", AZStd::string(implementationSource).c_str(), diagnostics.writeRef());
                SlangCompilerService::ReportDiagnostics("OptionsImplementation", diagnostics, !implementationModule);
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
                SlangCompilerService::ReportDiagnostics("InterfaceProbe", diagnostics, true);
                return false;
            }

            Slang::ComPtr<slang::IComponentType> linkedProgram;
            diagnostics = nullptr;
            if (SLANG_FAILED(composedProgram->link(linkedProgram.writeRef(), diagnostics.writeRef())))
            {
                SlangCompilerService::ReportDiagnostics("InterfaceProbe", diagnostics, true);
                return false;
            }

            Slang::ComPtr<slang::IBlob> byteCode;
            diagnostics = nullptr;
            if (SLANG_FAILED(linkedProgram->getEntryPointCode(0, 0, byteCode.writeRef(), diagnostics.writeRef())) || !byteCode)
            {
                SlangCompilerService::ReportDiagnostics("InterfaceProbe", diagnostics, true);
                return false;
            }
            const uint8_t* bytes = static_cast<const uint8_t*>(byteCode->getBufferPointer());
            outByteCode.assign(bytes, bytes + byteCode->getBufferSize());
            if (outUseSiteModule)
            {
                *outUseSiteModule = useSiteModule;
            }
            if (outSession)
            {
                *outSession = AZStd::move(session);
            }
            return true;
        }
    };

    TEST_F(OptionsInterfaceStyleProbeTests, ExternStruct_BakedImplementation_SpecializesCodegen)
    {
        constexpr AZStd::string_view bakedTrueImplementation = R"(
module OptionsImplementation;
import InterfaceProbe;

export struct Options : IOptions
{
    static bool useTint() { return true; }
    static int quality() { return 2; }
}
)";
        constexpr AZStd::string_view bakedFalseImplementation = R"(
module OptionsImplementation;
import InterfaceProbe;

export struct Options : IOptions
{
    static bool useTint() { return false; }
    static int quality() { return 0; }
}
)";
        AZStd::vector<uint8_t> byteCodeTrue;
        const bool compiled = CompileProgram(UseSiteSource, bakedTrueImplementation, byteCodeTrue);
        printf("[PROBE] extern struct + linked export struct implementation: %s\n", compiled ? "SUPPORTED" : "NOT SUPPORTED");
        ASSERT_TRUE(compiled);

        AZStd::vector<uint8_t> byteCodeFalse;
        ASSERT_TRUE(CompileProgram(UseSiteSource, bakedFalseImplementation, byteCodeFalse));

        const bool byteCodeDiffers =
            byteCodeTrue.size() != byteCodeFalse.size() ||
            memcmp(byteCodeTrue.data(), byteCodeFalse.data(), byteCodeTrue.size()) != 0;
        printf("[PROBE] baked interface values specialize codegen: %s\n", byteCodeDiffers ? "YES" : "NO");
        EXPECT_TRUE(byteCodeDiffers);
    }

    TEST_F(OptionsInterfaceStyleProbeTests, ExternStruct_SpecConstantAndDynamicImplementations)
    {
        constexpr AZStd::string_view specImplementation = R"(
module OptionsImplementation;
import InterfaceProbe;

[vk::constant_id(0)]
const bool s_useTint = false;

[vk::constant_id(1)]
const int s_quality = 1;

export struct Options : IOptions
{
    static bool useTint() { return s_useTint; }
    static int quality() { return s_quality; }
}
)";
        AZStd::vector<uint8_t> specByteCode;
        const bool specCompiled = CompileProgram(UseSiteSource, specImplementation, specByteCode);
        printf("[PROBE] spec-constant interface implementation: %s\n", specCompiled ? "SUPPORTED" : "NOT SUPPORTED");
        EXPECT_TRUE(specCompiled);

        constexpr AZStd::string_view dynamicImplementation = R"(
module OptionsImplementation;
import InterfaceProbe;

struct FallbackKey
{
    uint4 m_keyBits;
};

ConstantBuffer<FallbackKey> OptionsFallback;

export struct Options : IOptions
{
    static bool useTint() { return (OptionsFallback.m_keyBits.x & 1u) != 0u; }
    static int quality() { return int((OptionsFallback.m_keyBits.x >> 1u) & 3u); }
}
)";
        AZStd::vector<uint8_t> dynamicByteCode;
        const bool dynamicCompiled = CompileProgram(UseSiteSource, dynamicImplementation, dynamicByteCode);
        printf("[PROBE] dynamic fallback-read interface implementation: %s\n", dynamicCompiled ? "SUPPORTED" : "NOT SUPPORTED");
        EXPECT_TRUE(dynamicCompiled);
    }

    TEST_F(OptionsInterfaceStyleProbeTests, ExternStructWithoutInterface_ParenlessUseSites)
    {
        // The desugared form the builder generates from the annotated authoring sugar: a facade
        // struct keeps the authored name and PARENLESS member access via static properties,
        // forwarding to interface-backed extern functions the per-mode modules implement — so
        // variant builds still link against the cached module without re-running the frontend.
        constexpr AZStd::string_view desugaredUseSiteSource = R"(
module InterfaceProbe;

public interface IOptionsImplementation
{
    static bool useTint();
    static int quality();
}

extern struct OptionsImplementation : IOptionsImplementation;

struct Options
{
    static property bool useTint
    {
        get { return OptionsImplementation.useTint(); }
    }
    static property int quality
    {
        get { return OptionsImplementation.quality(); }
    }
}

RWStructuredBuffer<float4> Output;

[shader("compute")]
[numthreads(1, 1, 1)]
void MainCS(uint3 id : SV_DispatchThreadID)
{
    float4 value = float4(1.0, 1.0, 1.0, 1.0);
    if (Options.useTint)
    {
        value *= 0.5;
    }
    value.x += float(Options.quality);
    Output[id.x] = value;
}
)";
        constexpr AZStd::string_view bakedTrueImplementation = R"(
module OptionsImplementation;
import InterfaceProbe;

export struct OptionsImplementation : IOptionsImplementation
{
    static bool useTint() { return true; }
    static int quality() { return 2; }
}
)";
        constexpr AZStd::string_view bakedFalseImplementation = R"(
module OptionsImplementation;
import InterfaceProbe;

export struct OptionsImplementation : IOptionsImplementation
{
    static bool useTint() { return false; }
    static int quality() { return 0; }
}
)";
        AZ_UNUSED(bakedFalseImplementation);

        // CHARACTERIZATION: static `property` members are not supported by this compiler version
        // (E31201, in structs and interfaces alike), so parenless namespaced access
        // (Options.useTint) cannot be built by any composition. Parenless use-sites require
        // global properties (the accessor form); namespaced access requires function calls.
        AZStd::vector<uint8_t> bakedTrueByteCode;
        AZ_TEST_START_TRACE_SUPPRESSION;
        const bool facadeCompiled = CompileProgram(desugaredUseSiteSource, bakedTrueImplementation, bakedTrueByteCode);
        AZ_TEST_STOP_TRACE_SUPPRESSION(1);
        printf("[PROBE] facade struct with static properties (parenless namespaced use-sites): %s\n", facadeCompiled ? "SUPPORTED" : "NOT SUPPORTED");
        EXPECT_FALSE(facadeCompiled);
    }

    TEST_F(OptionsInterfaceStyleProbeTests, AnnotatedStruct_AuthoredSugarIsDiscoverable)
    {
        // The authored sugar the user proposed: one annotated struct with typed members and
        // in-source defaults, valid standalone Slang. The builder discovers members + defaults
        // through declaration reflection, then swaps the struct for `extern struct Options;` in
        // production compiles.
        constexpr AZStd::string_view authoredSource = R"(
module AnnotatedProbe;

[__AttributeUsage(_AttributeTargets.Struct)]
struct AtomOptionsAttribute {};

[AtomOptions]
struct Options
{
    static const bool useTint = false;
    static const int quality = 1;
}

RWStructuredBuffer<float4> Output;

[shader("compute")]
[numthreads(1, 1, 1)]
void MainCS(uint3 id : SV_DispatchThreadID)
{
    float4 value = float4(1.0, 1.0, 1.0, 1.0);
    if (Options.useTint)
    {
        value *= 0.5;
    }
    value.x += float(Options.quality);
    Output[id.x] = value;
}
)";
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
            "AnnotatedProbe", "AnnotatedProbe.slang", AZStd::string(authoredSource).c_str(), diagnostics.writeRef());
        SlangCompilerService::ReportDiagnostics("AnnotatedProbe", diagnostics, !probeModule);
        ASSERT_NE(probeModule, nullptr);
        printf("[PROBE] annotated options struct with defaults compiles standalone: SUPPORTED\n");

        // Discover the annotated struct and its members
        AZStd::vector<AZStd::string> memberNames;
        slang::DeclReflection* moduleDecl = probeModule->getModuleReflection();
        ASSERT_NE(moduleDecl, nullptr);
        for (unsigned childIndex = 0; childIndex < moduleDecl->getChildrenCount(); ++childIndex)
        {
            slang::DeclReflection* child = moduleDecl->getChild(childIndex);
            if (!child || !child->getName() || azstricmp(child->getName(), "Options") != 0)
            {
                continue;
            }
            for (unsigned memberIndex = 0; memberIndex < child->getChildrenCount(); ++memberIndex)
            {
                slang::DeclReflection* member = child->getChild(memberIndex);
                if (member && member->getName())
                {
                    memberNames.push_back(member->getName());
                }
            }
        }
        printf("[PROBE] annotated struct members via decl reflection: %zu found\n", memberNames.size());
        for (const AZStd::string& memberName : memberNames)
        {
            printf("[PROBE]   member: %s\n", memberName.c_str());
        }
        EXPECT_GE(memberNames.size(), 2);
    }

    TEST_F(OptionsInterfaceStyleProbeTests, InterfaceRequirements_DiscoverableForDescriptorValidation)
    {
        // The builder cross-validates the .shader option metadata against the authored
        // interface: every declared option must appear as an interface requirement. Walk the
        // module declaration tree to the interface and enumerate its members.
        constexpr AZStd::string_view bakedImplementation = R"(
module OptionsImplementation;
import InterfaceProbe;

export struct Options : IOptions
{
    static bool useTint() { return true; }
    static int quality() { return 2; }
}
)";
        AZStd::vector<uint8_t> byteCode;
        slang::IModule* useSiteModule = nullptr;
        Slang::ComPtr<slang::ISession> session;
        ASSERT_TRUE(CompileProgram(UseSiteSource, bakedImplementation, byteCode, &useSiteModule, AZStd::addressof(session)));

        SlangCompilerService& service = SlangCompilerService::Get();
        const AZStd::unique_lock<AZStd::recursive_mutex> compilerLock = service.AcquireCompilerLock();

        AZStd::vector<AZStd::string> requirementNames;
        slang::DeclReflection* moduleDecl = useSiteModule->getModuleReflection();
        ASSERT_NE(moduleDecl, nullptr);
        for (unsigned childIndex = 0; childIndex < moduleDecl->getChildrenCount(); ++childIndex)
        {
            slang::DeclReflection* child = moduleDecl->getChild(childIndex);
            if (!child || !child->getName() || azstricmp(child->getName(), "IOptions") != 0)
            {
                continue;
            }
            for (unsigned memberIndex = 0; memberIndex < child->getChildrenCount(); ++memberIndex)
            {
                slang::DeclReflection* member = child->getChild(memberIndex);
                if (member && member->getName())
                {
                    requirementNames.push_back(member->getName());
                }
            }
        }

        printf("[PROBE] interface requirements via decl reflection: %zu found\n", requirementNames.size());
        for (const AZStd::string& requirementName : requirementNames)
        {
            printf("[PROBE]   requirement: %s\n", requirementName.c_str());
        }
        EXPECT_GE(requirementNames.size(), 2);
    }

    //! Loads a module standalone with diagnostics reported via printf only — for probes where a
    //! failure to parse is a finding, not a test error.
    static slang::IModule* TryLoadProbeModule(slang::ISession* session, const char* moduleName, AZStd::string_view source)
    {
        Slang::ComPtr<slang::IBlob> diagnostics;
        slang::IModule* probeModule = session->loadModuleFromSourceString(
            moduleName,
            AZStd::string::format("%s.slang", moduleName).c_str(),
            AZStd::string(source).c_str(),
            diagnostics.writeRef());
        if (diagnostics && diagnostics->getBufferSize() > 0)
        {
            printf("[PROBE]   %s diagnostics: %.*s\n",
                moduleName,
                static_cast<int>(diagnostics->getBufferSize()),
                static_cast<const char*>(diagnostics->getBufferPointer()));
        }
        return probeModule;
    }

    TEST_F(OptionsInterfaceStyleProbeTests, DefaultValues_InterfaceMethodBodies)
    {
        // SP#030 (default implementations of interface methods) would let the option default live
        // as the method body — but reflection cannot evaluate bodies, so even if this parses, the
        // builder could not READ the default from it. This probe just records whether the pinned
        // compiler accepts the syntax at all.
        SlangCompilerService& service = SlangCompilerService::Get();
        const AZStd::unique_lock<AZStd::recursive_mutex> compilerLock = service.AcquireCompilerLock();

        SlangCompilerService::SessionDescriptor sessionDescriptor;
        sessionDescriptor.m_target = SLANG_SPIRV;
        sessionDescriptor.m_profile = "spirv_1_5";
        auto sessionOutcome = service.CreateSession(sessionDescriptor);
        ASSERT_TRUE(sessionOutcome.IsSuccess());
        Slang::ComPtr<slang::ISession> session = sessionOutcome.TakeValue();

        constexpr AZStd::string_view defaultBodySource = R"(
module DefaultBodyProbe;

public interface IOptions
{
    static int quality() { return 1; }
}
)";
        slang::IModule* probeModule = TryLoadProbeModule(session, "DefaultBodyProbe", defaultBodySource);
        printf("[PROBE] interface method with default body (SP#030): %s\n", probeModule ? "PARSES" : "NOT SUPPORTED");
    }

    TEST_F(OptionsInterfaceStyleProbeTests, DefaultValues_EmptyStructInheritsDefaultBody)
    {
        // If conformance accepts an empty linked struct and the default body supplies the value,
        // option defaults can live as interface method bodies and "all defaults" variants link an
        // empty struct — the builder still cannot READ the value via reflection, but the shader
        // side works.
        constexpr AZStd::string_view useSiteSource = R"(
module InterfaceProbe;

public interface IOptions
{
    static int quality() { return 1; }
}

extern struct Options : IOptions;

RWStructuredBuffer<float4> Output;

[shader("compute")]
[numthreads(1, 1, 1)]
void MainCS(uint3 id : SV_DispatchThreadID)
{
    float4 value = float4(1.0, 1.0, 1.0, 1.0);
    value.x += float(Options.quality());
    Output[id.x] = value;
}
)";
        constexpr AZStd::string_view emptyImplementation = R"(
module OptionsImplementation;
import InterfaceProbe;

export struct Options : IOptions
{
}
)";
        constexpr AZStd::string_view overridingImplementation = R"(
module OptionsImplementation;
import InterfaceProbe;

export struct Options : IOptions
{
    override static int quality() { return 2; }
}
)";

        AZStd::vector<uint8_t> emptyByteCode;
        const bool emptyCompiled = CompileProgram(useSiteSource, emptyImplementation, emptyByteCode);
        printf("[PROBE] empty linked struct inherits interface default body: %s\n",
            emptyCompiled ? "SUPPORTED" : "NOT SUPPORTED");

        AZStd::vector<uint8_t> overridingByteCode;
        const bool overridingCompiled = CompileProgram(useSiteSource, overridingImplementation, overridingByteCode);
        printf("[PROBE] linked struct overriding the default body (override keyword): %s\n",
            overridingCompiled ? "SUPPORTED" : "NOT SUPPORTED");

        if (emptyCompiled && overridingCompiled)
        {
            const bool byteCodeDiffers =
                emptyByteCode.size() != overridingByteCode.size() ||
                memcmp(emptyByteCode.data(), overridingByteCode.data(), emptyByteCode.size()) != 0;
            printf("[PROBE] override vs inherited default specializes codegen: %s\n", byteCodeDiffers ? "YES" : "NO");
        }
    }

    TEST_F(OptionsInterfaceStyleProbeTests, DefaultValues_BareStaticConstRequirements)
    {
        // E30623 established that interface requirements cannot carry initializers, so defaults
        // cannot live there — but BARE static-const requirements are the structural-option form
        // (parenless use-sites, link-time constants usable as layout keys). Characterize: linked
        // structs providing different values must both conform and specialize codegen.
        constexpr AZStd::string_view useSiteSource = R"(
module InterfaceProbe;

public enum QualityT
{
    Low,
    Medium,
    High,
}

public interface IOptions
{
    static const int iterations;
    static const QualityT quality;
}

extern struct Options : IOptions;

RWStructuredBuffer<float4> Output;

[shader("compute")]
[numthreads(1, 1, 1)]
void MainCS(uint3 id : SV_DispatchThreadID)
{
    float4 value = float4(1.0, 1.0, 1.0, 1.0);
    for (int i = 0; i < Options.iterations; ++i)
    {
        value.y += 0.125;
    }
    if (Options.quality == QualityT.High)
    {
        value.z = 0.0;
    }
    Output[id.x] = value;
}
)";
        constexpr AZStd::string_view defaultsImplementation = R"(
module OptionsImplementation;
import InterfaceProbe;

export struct Options : IOptions
{
    static const int iterations = 4;
    static const QualityT quality = QualityT.Medium;
}
)";
        constexpr AZStd::string_view overridingImplementation = R"(
module OptionsImplementation;
import InterfaceProbe;

export struct Options : IOptions
{
    static const int iterations = 7;
    static const QualityT quality = QualityT.High;
}
)";

        AZStd::vector<uint8_t> defaultsByteCode;
        const bool defaultsCompiled = CompileProgram(useSiteSource, defaultsImplementation, defaultsByteCode);
        printf("[PROBE] bare static-const requirements + linked value struct (parenless use-sites): %s\n",
            defaultsCompiled ? "SUPPORTED" : "NOT SUPPORTED");

        AZStd::vector<uint8_t> overridingByteCode;
        const bool overridingCompiled = CompileProgram(useSiteSource, overridingImplementation, overridingByteCode);
        printf("[PROBE] second value struct with different constants: %s\n",
            overridingCompiled ? "SUPPORTED" : "NOT SUPPORTED");

        if (defaultsCompiled && overridingCompiled)
        {
            const bool byteCodeDiffers =
                defaultsByteCode.size() != overridingByteCode.size() ||
                memcmp(defaultsByteCode.data(), overridingByteCode.data(), defaultsByteCode.size()) != 0;
            printf("[PROBE] different constant values specialize codegen: %s\n", byteCodeDiffers ? "YES" : "NO");
        }

        // Reflection: bare requirements carry no initializer, so hasDefaultValue is expected
        // false — this characterizes that the requirements ARE enumerable as variables (names +
        // types for discovery), and what getDefaultValueInt does without an initializer
        SlangCompilerService& service = SlangCompilerService::Get();
        const AZStd::unique_lock<AZStd::recursive_mutex> compilerLock = service.AcquireCompilerLock();

        SlangCompilerService::SessionDescriptor sessionDescriptor;
        sessionDescriptor.m_target = SLANG_SPIRV;
        sessionDescriptor.m_profile = "spirv_1_5";
        auto sessionOutcome = service.CreateSession(sessionDescriptor);
        ASSERT_TRUE(sessionOutcome.IsSuccess());
        Slang::ComPtr<slang::ISession> session = sessionOutcome.TakeValue();

        slang::IModule* probeModule = TryLoadProbeModule(session, "InterfaceProbe", useSiteSource);
        ASSERT_NE(probeModule, nullptr);
        slang::DeclReflection* moduleDecl = probeModule->getModuleReflection();
        ASSERT_NE(moduleDecl, nullptr);

        for (unsigned childIndex = 0; childIndex < moduleDecl->getChildrenCount(); ++childIndex)
        {
            slang::DeclReflection* child = moduleDecl->getChild(childIndex);
            if (!child || !child->getName() || azstricmp(child->getName(), "IOptions") != 0)
            {
                continue;
            }
            printf("[PROBE] interface decl kind: %d\n", static_cast<int>(child->getKind()));
            for (unsigned memberIndex = 0; memberIndex < child->getChildrenCount(); ++memberIndex)
            {
                slang::DeclReflection* member = child->getChild(memberIndex);
                if (!member || !member->getName() || member->getName()[0] == '$')
                {
                    continue;
                }
                slang::VariableReflection* variable = member->asVariable();
                if (!variable)
                {
                    printf("[PROBE]   member %s: kind=%d, not a variable\n",
                        member->getName(), static_cast<int>(member->getKind()));
                    continue;
                }
                int64_t defaultValue = 0;
                const bool hasDefault = variable->hasDefaultValue();
                const SlangResult defaultResult = variable->getDefaultValueInt(&defaultValue);
                printf("[PROBE]   member %s: hasDefaultValue=%s, getDefaultValueInt=%s value=%lld\n",
                    member->getName(),
                    hasDefault ? "true" : "false",
                    SLANG_SUCCEEDED(defaultResult) ? "OK" : "FAILED",
                    static_cast<long long>(defaultValue));
            }
        }
    }
    TEST_F(OptionsInterfaceStyleProbeTests, InterfaceOnlySurface_FullDiscoveryAndDynamicRead)
    {
        // The production interface-only surface, end to end: [AtomOptions]-marked interface with
        // typed [AtomOption] defaults on its requirements, an extern conforming struct, and a
        // [AtomVariantFallback]-attributed ShaderResourceGroup member the generated dynamic
        // implementation reads DIRECTLY via import (no exported getter, no bidirectional link).
        constexpr AZStd::string_view useSiteSource = R"(
module InterfaceProbe;

public enum QualityT
{
    Low,
    Medium,
    High,
}

[__AttributeUsage(_AttributeTargets.Struct)]
struct AtomOptionsAttribute {};

[__AttributeUsage(_AttributeTargets.Function)]
struct AtomOptionAttribute { int defaultValue; };

[__AttributeUsage(_AttributeTargets.Function)]
struct AtomOptionRangeAttribute { int minValue; int maxValue; };

[__AttributeUsage(_AttributeTargets.Var)]
struct AtomVariantFallbackAttribute {};

public interface IOptions
{
    [AtomOption(true)]
    static bool useTint();

    [AtomOption(QualityT.Medium)]
    static QualityT quality();

    [AtomOption(4)] [AtomOptionRange(1, 8)]
    static int sampleCount();
}

[AtomOptions]
public extern struct Options : IOptions;

public struct DrawShaderResourceGroup
{
    public float4 m_color;

    [AtomVariantFallback]
    public uint4 m_shaderVariantKey;
};
public ParameterBlock<DrawShaderResourceGroup> DrawSrg;

RWStructuredBuffer<float4> Output;

[shader("compute")]
[numthreads(1, 1, 1)]
void MainCS(uint3 id : SV_DispatchThreadID)
{
    float4 value = DrawSrg.m_color;
    if (Options.useTint())
    {
        value *= 0.5;
    }
    for (int i = 0; i < Options.sampleCount(); ++i)
    {
        value.y += 0.125;
    }
    if (Options.quality() == QualityT.High)
    {
        value.z = 0.0;
    }
    Output[id.x] = value;
}
)";
        constexpr AZStd::string_view dynamicImplementation = R"(
module OptionsImplementation;
import InterfaceProbe;

export struct Options : IOptions
{
    static bool useTint() { return (DrawSrg.m_shaderVariantKey.x & 1u) != 0u; }
    static QualityT quality() { return (QualityT)((DrawSrg.m_shaderVariantKey.x >> 1u) & 3u); }
    static int sampleCount() { return int((DrawSrg.m_shaderVariantKey.x >> 3u) & 7u); }
}
)";

        // Standalone load first so parse diagnostics reach the log verbatim
        {
            SlangCompilerService& diagnosticsService = SlangCompilerService::Get();
            const AZStd::unique_lock<AZStd::recursive_mutex> diagnosticsLock = diagnosticsService.AcquireCompilerLock();
            SlangCompilerService::SessionDescriptor diagnosticsSessionDescriptor;
            diagnosticsSessionDescriptor.m_target = SLANG_SPIRV;
            diagnosticsSessionDescriptor.m_profile = "spirv_1_5";
            auto diagnosticsSessionOutcome = diagnosticsService.CreateSession(diagnosticsSessionDescriptor);
            ASSERT_TRUE(diagnosticsSessionOutcome.IsSuccess());
            Slang::ComPtr<slang::ISession> diagnosticsSession = diagnosticsSessionOutcome.TakeValue();
            ASSERT_NE(TryLoadProbeModule(diagnosticsSession, "InterfaceProbe", useSiteSource), nullptr);
        }

        AZStd::vector<uint8_t> byteCode;
        slang::IModule* useSiteModule = nullptr;
        Slang::ComPtr<slang::ISession> session;
        const bool compiled = CompileProgram(useSiteSource, dynamicImplementation, byteCode, &useSiteModule, AZStd::addressof(session));
        printf("[PROBE] interface-only surface, dynamic impl reads imported public ParameterBlock member: %s\n",
            compiled ? "SUPPORTED" : "NOT SUPPORTED");
        ASSERT_TRUE(compiled);

        SlangCompilerService& service = SlangCompilerService::Get();
        const AZStd::unique_lock<AZStd::recursive_mutex> compilerLock = service.AcquireCompilerLock();

        // Discovery path 1: enumerate top-level decl names, resolve through the module layout's
        // findTypeByName, identify the [AtomOptions] interface and the conforming struct via
        // isSubType
        slang::ProgramLayout* moduleLayout = useSiteModule->getLayout();
        ASSERT_NE(moduleLayout, nullptr);

        slang::TypeReflection* interfaceType = moduleLayout->findTypeByName("IOptions");
        printf("[PROBE] findTypeByName(IOptions): %s, kind=%d\n",
            interfaceType ? "FOUND" : "NOT FOUND",
            interfaceType ? static_cast<int>(interfaceType->getKind()) : -1);
        ASSERT_NE(interfaceType, nullptr);

        slang::TypeReflection* optionsStructType = moduleLayout->findTypeByName("Options");
        if (optionsStructType)
        {
            unsigned structAttributeCount = optionsStructType->getUserAttributeCount();
            bool hasAtomOptionsMarker = false;
            for (unsigned attributeIndex = 0; attributeIndex < structAttributeCount; ++attributeIndex)
            {
                if (azstricmp(optionsStructType->getUserAttributeByIndex(attributeIndex)->getName(), "AtomOptions") == 0)
                {
                    hasAtomOptionsMarker = true;
                }
            }
            printf("[PROBE] [AtomOptions] marker on the extern struct via TypeReflection: %s (%u attributes)\n",
                hasAtomOptionsMarker ? "READABLE" : "NOT READABLE", structAttributeCount);
        }
        printf("[PROBE] findTypeByName(Options extern struct): %s, kind=%d\n",
            optionsStructType ? "FOUND" : "NOT FOUND",
            optionsStructType ? static_cast<int>(optionsStructType->getKind()) : -1);
        if (optionsStructType)
        {
            printf("[PROBE] isSubType(Options, IOptions): %s\n",
                moduleLayout->isSubType(optionsStructType, interfaceType) ? "TRUE" : "FALSE");
        }

        // Discovery path 2: interface requirements as functions — names, typed [AtomOption]
        // defaults, [AtomOptionRange], and REAL return types (no stringized type names)
        slang::DeclReflection* moduleDecl = useSiteModule->getModuleReflection();
        ASSERT_NE(moduleDecl, nullptr);
        for (unsigned childIndex = 0; childIndex < moduleDecl->getChildrenCount(); ++childIndex)
        {
            slang::DeclReflection* child = moduleDecl->getChild(childIndex);
            if (!child || !child->getName() || azstricmp(child->getName(), "IOptions") != 0)
            {
                continue;
            }
            for (unsigned memberIndex = 0; memberIndex < child->getChildrenCount(); ++memberIndex)
            {
                slang::DeclReflection* member = child->getChild(memberIndex);
                if (!member || !member->getName() || member->getName()[0] == '$'
                    || azstricmp(member->getName(), "This") == 0)
                {
                    continue;
                }
                slang::FunctionReflection* requirement = member->asFunction();
                if (!requirement)
                {
                    printf("[PROBE]   requirement %s: kind=%d, asFunction FAILED\n",
                        member->getName(), static_cast<int>(member->getKind()));
                    continue;
                }
                slang::TypeReflection* returnType = requirement->getReturnType();
                int defaultValue = 0;
                bool hasDefault = false;
                int minValue = 0;
                int maxValue = 0;
                bool hasRange = false;
                for (unsigned attributeIndex = 0; attributeIndex < requirement->getUserAttributeCount(); ++attributeIndex)
                {
                    slang::UserAttribute* attribute = requirement->getUserAttributeByIndex(attributeIndex);
                    if (azstricmp(attribute->getName(), "AtomOption") == 0)
                    {
                        hasDefault = SLANG_SUCCEEDED(attribute->getArgumentValueInt(0, &defaultValue));
                    }
                    else if (azstricmp(attribute->getName(), "AtomOptionRange") == 0)
                    {
                        hasRange = SLANG_SUCCEEDED(attribute->getArgumentValueInt(0, &minValue))
                            && SLANG_SUCCEEDED(attribute->getArgumentValueInt(1, &maxValue));
                    }
                }
                printf("[PROBE]   requirement %s: returnType=%s (kind=%d), default=%s(%d), range=%s(%d..%d)\n",
                    requirement->getName(),
                    returnType && returnType->getName() ? returnType->getName() : "<null>",
                    returnType ? static_cast<int>(returnType->getKind()) : -1,
                    hasDefault ? "OK" : "MISSING", defaultValue,
                    hasRange ? "OK" : "none", minValue, maxValue);
            }
        }

        // Discovery path 3: the [AtomVariantFallback] member attribute through the SRG element
        // type's fields
        slang::TypeReflection* srgElementType = moduleLayout->findTypeByName("DrawShaderResourceGroup");
        ASSERT_NE(srgElementType, nullptr);
        for (unsigned fieldIndex = 0; fieldIndex < srgElementType->getFieldCount(); ++fieldIndex)
        {
            slang::VariableReflection* field = srgElementType->getFieldByIndex(fieldIndex);
            for (unsigned attributeIndex = 0; field && attributeIndex < field->getUserAttributeCount(); ++attributeIndex)
            {
                if (azstricmp(field->getUserAttributeByIndex(attributeIndex)->getName(), "AtomVariantFallback") == 0)
                {
                    printf("[PROBE] [AtomVariantFallback] member attribute readable: field %s\n", field->getName());
                }
            }
        }
    }

    TEST_F(OptionsInterfaceStyleProbeTests, TypedAttributeArguments_Characterization)
    {
        // Can option defaults be TYPED attribute arguments instead of strings —
        // [AtomOption(QualityT.Medium)]? Four variants, loaded separately so one failing form
        // does not mask the others: enum-typed field, bool field, (int)-cast enum expression on
        // an int field, and a generic attribute struct.
        SlangCompilerService& service = SlangCompilerService::Get();
        const AZStd::unique_lock<AZStd::recursive_mutex> compilerLock = service.AcquireCompilerLock();

        SlangCompilerService::SessionDescriptor sessionDescriptor;
        sessionDescriptor.m_target = SLANG_SPIRV;
        sessionDescriptor.m_profile = "spirv_1_5";
        auto sessionOutcome = service.CreateSession(sessionDescriptor);
        ASSERT_TRUE(sessionOutcome.IsSuccess());
        Slang::ComPtr<slang::ISession> session = sessionOutcome.TakeValue();

        // Reads back argument 0 of the single attribute on the single attributed function
        auto readFirstAttributeArgument = [](slang::IModule* probeModule, int64_t& outValue) -> bool
        {
            slang::DeclReflection* moduleDecl = probeModule->getModuleReflection();
            for (unsigned childIndex = 0; moduleDecl && childIndex < moduleDecl->getChildrenCount(); ++childIndex)
            {
                slang::DeclReflection* child = moduleDecl->getChild(childIndex);
                slang::FunctionReflection* function =
                    (child && child->getKind() == slang::DeclReflection::Kind::Func) ? child->asFunction() : nullptr;
                if (!function || function->getUserAttributeCount() == 0)
                {
                    continue;
                }
                int intValue = 0;
                if (SLANG_SUCCEEDED(function->getUserAttributeByIndex(0)->getArgumentValueInt(0, &intValue)))
                {
                    outValue = intValue;
                    return true;
                }
            }
            return false;
        };

        struct AttributeVariant
        {
            const char* m_label;
            const char* m_moduleName;
            AZStd::string_view m_source;
            int64_t m_expectedValue;
        };
        const AttributeVariant variants[] = {
            {"enum-typed field, enum literal arg [Attr(QualityT.Medium)]", "EnumFieldProbe", R"(
module EnumFieldProbe;
public enum QualityT { Low, Medium, High }
[__AttributeUsage(_AttributeTargets.Function)]
struct AtomEnumDefaultAttribute { QualityT defaultValue; };
[AtomEnumDefault(QualityT.Medium)]
extern int AtomOptionImpl_o_quality();
)", 1},
            {"bool field, bool literal arg [Attr(true)]", "BoolFieldProbe", R"(
module BoolFieldProbe;
[__AttributeUsage(_AttributeTargets.Function)]
struct AtomBoolDefaultAttribute { bool defaultValue; };
[AtomBoolDefault(true)]
extern int AtomOptionImpl_o_useTint();
)", 1},
            {"int field, cast enum expression arg [Attr((int)QualityT.High)]", "CastArgProbe", R"(
module CastArgProbe;
public enum QualityT { Low, Medium, High }
[__AttributeUsage(_AttributeTargets.Function)]
struct AtomIntDefaultAttribute { int defaultValue; };
[AtomIntDefault((int)QualityT.High)]
extern int AtomOptionImpl_o_quality();
)", 2},
            {"generic attribute struct [Attr<T>(...)]", "GenericAttributeProbe", R"(
module GenericAttributeProbe;
public enum QualityT { Low, Medium, High }
[__AttributeUsage(_AttributeTargets.Function)]
struct AtomTypedOptionAttribute<T> { T defaultValue; };
[AtomTypedOption(QualityT.Medium)]
extern int AtomOptionImpl_o_quality();
)", 1},
            {"int field, UNCAST enum literal arg [Attr(QualityT.Medium)] (implicit conversion)", "ImplicitConversionProbe", R"(
module ImplicitConversionProbe;
public enum QualityT { Low, Medium, High }
[__AttributeUsage(_AttributeTargets.Function)]
struct AtomIntDefaultAttribute { int defaultValue; };
[AtomIntDefault(QualityT.Medium)]
extern int AtomOptionImpl_o_quality();
)", 1},
            {"int field, bool literal arg [Attr(true)] (implicit conversion)", "BoolToIntProbe", R"(
module BoolToIntProbe;
[__AttributeUsage(_AttributeTargets.Function)]
struct AtomIntDefaultAttribute { int defaultValue; };
[AtomIntDefault(true)]
extern int AtomOptionImpl_o_useTint();
)", 1},
        };

        for (const AttributeVariant& variant : variants)
        {
            slang::IModule* probeModule = TryLoadProbeModule(session, variant.m_moduleName, variant.m_source);
            if (!probeModule)
            {
                printf("[PROBE] %s: DOES NOT PARSE\n", variant.m_label);
                continue;
            }
            int64_t value = 0;
            if (readFirstAttributeArgument(probeModule, value))
            {
                printf("[PROBE] %s: PARSES, getArgumentValueInt=%lld (expected %lld) %s\n",
                    variant.m_label,
                    static_cast<long long>(value),
                    static_cast<long long>(variant.m_expectedValue),
                    value == variant.m_expectedValue ? "MATCH" : "MISMATCH");
            }
            else
            {
                printf("[PROBE] %s: PARSES, but argument value NOT READABLE as int\n", variant.m_label);
            }
        }
    }
} // namespace UnitTest
