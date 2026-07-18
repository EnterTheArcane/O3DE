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
// Use-sites read namespaced statics (ShaderOptions.useTint) — no guard, no bare-identifier
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

public interface IShaderOptions
{
    static bool useTint();
    static int quality();
}

extern struct ShaderOptions : IShaderOptions;

RWStructuredBuffer<float4> Output;

[shader("compute")]
[numthreads(1, 1, 1)]
void MainCS(uint3 id : SV_DispatchThreadID)
{
    float4 value = float4(1.0, 1.0, 1.0, 1.0);
    if (ShaderOptions.useTint())
    {
        value *= 0.5;
    }
    value.x += float(ShaderOptions.quality());
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

export struct ShaderOptions : IShaderOptions
{
    static bool useTint() { return true; }
    static int quality() { return 2; }
}
)";
        constexpr AZStd::string_view bakedFalseImplementation = R"(
module OptionsImplementation;
import InterfaceProbe;

export struct ShaderOptions : IShaderOptions
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

export struct ShaderOptions : IShaderOptions
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

export struct ShaderOptions : IShaderOptions
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

export struct ShaderOptions : IShaderOptions
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
            if (!child || !child->getName() || azstricmp(child->getName(), "IShaderOptions") != 0)
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
} // namespace UnitTest
