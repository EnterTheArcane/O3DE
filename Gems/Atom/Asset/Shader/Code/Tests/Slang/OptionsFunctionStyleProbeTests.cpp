/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// Authoring-surface probes for the production options form: options as flat
// [AtomOption]-attributed extern FUNCTIONS — AZSL's global option declarations, as functions —
// satisfied per lowering mode by a generated module of matching `export` functions composed at
// link time.
//
// Questions answered against the pinned compiler:
// 1. Does the per-function extern/export seam link at all, and do different linked values
//    specialize codegen (baked constants must fold)?
// 2. Does a bare [AtomOption] parse when the attribute struct's field carries a default
//    initializer, and what does attribute reflection report for the omitted argument? (The
//    generator treats getArgumentCount() == 0 as the default of 0.)
// 3. Are typed attribute arguments — bool and enum literals on the int field — readable through
//    declaration reflection as their int values?

#include <AzTest/AzTest.h>
#include <AzCore/UnitTest/TestTypes.h>

#include <Common/ShaderBuilderTestFixture.h>

#include <Slang/SlangCompilerService.h>

namespace UnitTest
{
    using namespace AZ;
    using ShaderBuilder::SlangCompilerService;

    class OptionsFunctionStyleProbeTests : public ShaderBuilderTestFixture
    {
    public:
        static constexpr AZStd::string_view UseSiteSource = R"(
module FunctionProbe;

public extern bool o_useTint();
public extern int o_quality();

RWStructuredBuffer<float4> Output;

[shader("compute")]
[numthreads(1, 1, 1)]
void MainCS(uint3 id : SV_DispatchThreadID)
{
    float4 value = float4(1.0, 1.0, 1.0, 1.0);
    if (o_useTint())
    {
        value *= 0.5;
    }
    value.x += float(o_quality());
    Output[id.x] = value;
}
)";

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
                "FunctionProbe", "FunctionProbe.slang", AZStd::string(moduleSource).c_str(), diagnostics.writeRef());
            SlangCompilerService::ReportDiagnostics("FunctionProbe", diagnostics, !useSiteModule);
            if (!useSiteModule)
            {
                return false;
            }

            Slang::ComPtr<slang::IEntryPoint> entryPoint;
            diagnostics = nullptr;
            useSiteModule->findAndCheckEntryPoint("MainCS", SLANG_STAGE_COMPUTE, entryPoint.writeRef(), diagnostics.writeRef());
            SlangCompilerService::ReportDiagnostics("FunctionProbe", diagnostics, !entryPoint);
            if (!entryPoint)
            {
                return false;
            }

            diagnostics = nullptr;
            slang::IModule* implementationModule = session->loadModuleFromSourceString(
                "OptionsImplementation", "OptionsImplementation.slang", AZStd::string(implementationSource).c_str(), diagnostics.writeRef());
            SlangCompilerService::ReportDiagnostics("OptionsImplementation", diagnostics, !implementationModule);
            if (!implementationModule)
            {
                return false;
            }

            AZStd::vector<slang::IComponentType*> components;
            components.push_back(useSiteModule);
            components.push_back(implementationModule);
            components.push_back(entryPoint.get());

            Slang::ComPtr<slang::IComponentType> composedProgram;
            diagnostics = nullptr;
            if (SLANG_FAILED(session->createCompositeComponentType(components.data(), components.size(), composedProgram.writeRef(), diagnostics.writeRef())))
            {
                SlangCompilerService::ReportDiagnostics("FunctionProbe", diagnostics, true);
                return false;
            }

            Slang::ComPtr<slang::IComponentType> linkedProgram;
            diagnostics = nullptr;
            if (SLANG_FAILED(composedProgram->link(linkedProgram.writeRef(), diagnostics.writeRef())))
            {
                SlangCompilerService::ReportDiagnostics("FunctionProbe", diagnostics, true);
                return false;
            }

            Slang::ComPtr<slang::IBlob> byteCode;
            diagnostics = nullptr;
            if (SLANG_FAILED(linkedProgram->getEntryPointCode(0, 0, byteCode.writeRef(), diagnostics.writeRef())) || !byteCode)
            {
                SlangCompilerService::ReportDiagnostics("FunctionProbe", diagnostics, true);
                return false;
            }
            const uint8_t* bytes = static_cast<const uint8_t*>(byteCode->getBufferPointer());
            outByteCode.assign(bytes, bytes + byteCode->getBufferSize());
            return true;
        }

        //! Loads a module standalone with diagnostics reported via printf only — for probes where
        //! a failure to parse is a finding, not a test error.
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
    };

    TEST_F(OptionsFunctionStyleProbeTests, ExternFunctions_LinkedValuesSpecializeCodegen)
    {
        constexpr AZStd::string_view bakedTrueImplementation = R"(
module OptionsImplementation;
import FunctionProbe;

export bool o_useTint() { return true; }
export int o_quality() { return 2; }
)";
        constexpr AZStd::string_view bakedFalseImplementation = R"(
module OptionsImplementation;
import FunctionProbe;

export bool o_useTint() { return false; }
export int o_quality() { return 0; }
)";
        AZStd::vector<uint8_t> byteCodeTrue;
        const bool compiled = CompileProgram(UseSiteSource, bakedTrueImplementation, byteCodeTrue);
        printf("[PROBE] extern function + linked export function implementation: %s\n", compiled ? "SUPPORTED" : "NOT SUPPORTED");
        ASSERT_TRUE(compiled);

        AZStd::vector<uint8_t> byteCodeFalse;
        ASSERT_TRUE(CompileProgram(UseSiteSource, bakedFalseImplementation, byteCodeFalse));

        const bool byteCodeDiffers =
            byteCodeTrue.size() != byteCodeFalse.size() ||
            memcmp(byteCodeTrue.data(), byteCodeFalse.data(), byteCodeTrue.size()) != 0;
        printf("[PROBE] baked function values specialize codegen: %s\n", byteCodeDiffers ? "YES" : "NO");
        EXPECT_TRUE(byteCodeDiffers);
    }

    TEST_F(OptionsFunctionStyleProbeTests, ExternFunctions_DynamicImplementationReadsImportedParameterBlock)
    {
        // The dynamic lowering shape: the generated functions read the fallback member across the
        // module boundary through an import
        constexpr AZStd::string_view useSiteSource = R"(
module FunctionProbe;

public extern bool o_useTint();
public extern int o_quality();

public struct FallbackKey
{
    public uint4 m_keyBits;
};

public ParameterBlock<FallbackKey> OptionsFallback;

RWStructuredBuffer<float4> Output;

[shader("compute")]
[numthreads(1, 1, 1)]
void MainCS(uint3 id : SV_DispatchThreadID)
{
    float4 value = float4(1.0, 1.0, 1.0, 1.0);
    if (o_useTint())
    {
        value *= 0.5;
    }
    value.x += float(o_quality());
    Output[id.x] = value;
}
)";
        constexpr AZStd::string_view dynamicImplementation = R"(
module OptionsImplementation;
import FunctionProbe;

export bool o_useTint() { return (OptionsFallback.m_keyBits.x & 1u) != 0u; }
export int o_quality() { return int((OptionsFallback.m_keyBits.x >> 1u) & 3u); }
)";
        AZStd::vector<uint8_t> byteCode;
        const bool compiled = CompileProgram(useSiteSource, dynamicImplementation, byteCode);
        printf("[PROBE] dynamic implementation reads imported public ParameterBlock member: %s\n",
            compiled ? "SUPPORTED" : "NOT SUPPORTED");
        EXPECT_TRUE(compiled);
    }

    TEST_F(OptionsFunctionStyleProbeTests, BareAtomOption_DefaultedAttributeArgument)
    {
        // A field initializer on the attribute struct makes the argument optional — bare
        // [AtomOption] must parse — and reflection's view of the omitted argument is
        // characterized here: the generator reads an explicit argument, and treats
        // getArgumentCount() == 0 as the default of 0.
        SlangCompilerService& service = SlangCompilerService::Get();
        const AZStd::unique_lock<AZStd::recursive_mutex> compilerLock = service.AcquireCompilerLock();

        SlangCompilerService::SessionDescriptor sessionDescriptor;
        sessionDescriptor.m_target = SLANG_SPIRV;
        sessionDescriptor.m_profile = "spirv_1_5";
        auto sessionOutcome = service.CreateSession(sessionDescriptor);
        ASSERT_TRUE(sessionOutcome.IsSuccess());
        Slang::ComPtr<slang::ISession> session = sessionOutcome.TakeValue();

        constexpr AZStd::string_view probeSource = R"(
module BareAttributeProbe;

[__AttributeUsage(_AttributeTargets.Function)]
struct AtomOptionAttribute { int value = 0; };

[AtomOption]
public extern bool o_useTint();

[AtomOption(4)]
public extern int o_quality();
)";
        slang::IModule* probeModule = TryLoadProbeModule(session, "BareAttributeProbe", probeSource);
        printf("[PROBE] bare [AtomOption] with defaulted attribute field: %s\n", probeModule ? "PARSES" : "DOES NOT PARSE");
        ASSERT_NE(probeModule, nullptr);

        slang::DeclReflection* moduleDecl = probeModule->getModuleReflection();
        ASSERT_NE(moduleDecl, nullptr);
        unsigned attributedFunctionCount = 0;
        for (unsigned childIndex = 0; childIndex < moduleDecl->getChildrenCount(); ++childIndex)
        {
            slang::DeclReflection* child = moduleDecl->getChild(childIndex);
            slang::FunctionReflection* function =
                (child && child->getKind() == slang::DeclReflection::Kind::Func) ? child->asFunction() : nullptr;
            if (!function || function->getUserAttributeCount() == 0)
            {
                continue;
            }
            ++attributedFunctionCount;
            slang::UserAttribute* attribute = function->getUserAttributeByIndex(0);
            int argumentValue = 0;
            const SlangResult readResult = attribute->getArgumentValueInt(0, &argumentValue);
            printf("[PROBE]   %s: argumentCount=%u, getArgumentValueInt=%s value=%d\n",
                function->getName(),
                attribute->getArgumentCount(),
                SLANG_SUCCEEDED(readResult) ? "OK" : "FAILED",
                argumentValue);

            if (AZStd::string_view(function->getName()) == "o_quality")
            {
                // The explicit argument must be readable — the generator depends on it
                ASSERT_EQ(attribute->getArgumentCount(), 1);
                EXPECT_TRUE(SLANG_SUCCEEDED(readResult));
                EXPECT_EQ(argumentValue, 4);
            }
            else
            {
                // The omitted argument must be distinguishable from an unreadable one: either
                // reflection reports no arguments (the generator's default-0 path) or it reports
                // the field default itself
                EXPECT_TRUE(attribute->getArgumentCount() == 0 || (SLANG_SUCCEEDED(readResult) && argumentValue == 0));
            }
        }
        EXPECT_EQ(attributedFunctionCount, 2);
    }

    TEST_F(OptionsFunctionStyleProbeTests, TypedAttributeArguments_Characterization)
    {
        // Can option defaults be TYPED attribute arguments instead of strings —
        // [AtomOption(QualityT.Medium)]? Variants loaded separately so one failing form does not
        // mask the others: enum-typed field, bool field, (int)-cast enum expression on an int
        // field, and implicit conversions onto the int field (the production vocabulary).
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
struct AtomEnumDefaultAttribute { QualityT value; };
[AtomEnumDefault(QualityT.Medium)]
extern int o_quality();
)", 1},
            {"bool field, bool literal arg [Attr(true)]", "BoolFieldProbe", R"(
module BoolFieldProbe;
[__AttributeUsage(_AttributeTargets.Function)]
struct AtomBoolDefaultAttribute { bool value; };
[AtomBoolDefault(true)]
extern int o_useTint();
)", 1},
            {"int field, cast enum expression arg [Attr((int)QualityT.High)]", "CastArgProbe", R"(
module CastArgProbe;
public enum QualityT { Low, Medium, High }
[__AttributeUsage(_AttributeTargets.Function)]
struct AtomIntDefaultAttribute { int value; };
[AtomIntDefault((int)QualityT.High)]
extern int o_quality();
)", 2},
            {"int field, UNCAST enum literal arg [Attr(QualityT.Medium)] (implicit conversion)", "ImplicitConversionProbe", R"(
module ImplicitConversionProbe;
public enum QualityT { Low, Medium, High }
[__AttributeUsage(_AttributeTargets.Function)]
struct AtomIntDefaultAttribute { int value; };
[AtomIntDefault(QualityT.Medium)]
extern int o_quality();
)", 1},
            {"int field, bool literal arg [Attr(true)] (implicit conversion)", "BoolToIntProbe", R"(
module BoolToIntProbe;
[__AttributeUsage(_AttributeTargets.Function)]
struct AtomIntDefaultAttribute { int value; };
[AtomIntDefault(true)]
extern int o_useTint();
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
