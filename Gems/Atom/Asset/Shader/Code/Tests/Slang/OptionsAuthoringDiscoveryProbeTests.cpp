/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// M10 authoring-surface probe: can shader options be DECLARED in .slang source (LSP-visible,
// AZSL-like ergonomics) and discovered by the builder through declaration reflection — and what
// does the discovery pass cost?
//
// The candidate source form guards the declarations behind ATOM_OPTIONS_GENERATED so production
// compiles (which inject the generated per-mode options module) never see them; the builder's
// discovery pass compiles WITHOUT the define and walks IModule::getModuleReflection() for
// [AtomOption]-tagged variables.

#include <AzTest/AzTest.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzCore/std/chrono/chrono.h>

#include <Common/ShaderBuilderTestFixture.h>

#include <Slang/SlangCompilerService.h>

namespace UnitTest
{
    using namespace AZ;
    using ShaderBuilder::SlangCompilerService;

    class OptionsAuthoringDiscoveryProbeTests : public ShaderBuilderTestFixture
    {
    public:
        AZ::Test::ScopedAutoTempDirectory m_probeDirectory;

        //! The candidate authored form: options declared in source, attribute-tagged, guarded so
        //! generated-module compiles replace them.
        static constexpr AZStd::string_view AuthoredSource = R"(
module OptionsProbe;

[__AttributeUsage(_AttributeTargets.Var)]
struct AtomOptionAttribute
{
    string defaultValue;
};

#ifndef ATOM_OPTIONS_GENERATED
[AtomOption("false")]
public extern static const bool o_useTint;

[AtomOption("1")]
public extern static const int o_quality;
#endif

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

        //! Loads the authored source in a fresh session, optionally with the generated-mode
        //! define and an injected import of a generated options module.
        static slang::IModule* LoadProbeModule(
            slang::ISession* session,
            bool generatedMode)
        {
            AZStd::string sourceText;
            if (generatedMode)
            {
                sourceText += "import GeneratedOptions;\n";
            }
            sourceText += AuthoredSource;

            Slang::ComPtr<slang::IBlob> diagnostics;
            slang::IModule* probeModule = session->loadModuleFromSourceString(
                "OptionsProbe", "OptionsProbe.slang", sourceText.c_str(), diagnostics.writeRef());
            SlangCompilerService::ReportDiagnostics("OptionsProbe", diagnostics, !probeModule);
            return probeModule;
        }

        static SlangCompilerService::SessionDescriptor MakeSessionDescriptor(bool generatedMode, const char* searchPath)
        {
            SlangCompilerService::SessionDescriptor descriptor;
            descriptor.m_target = SLANG_SPIRV;
            descriptor.m_profile = "spirv_1_5";
            if (searchPath)
            {
                descriptor.m_searchPaths.push_back(searchPath);
            }
            if (generatedMode)
            {
                descriptor.m_preprocessorMacros.push_back({"ATOM_OPTIONS_GENERATED", "1"});
            }
            return descriptor;
        }
    };

    TEST_F(OptionsAuthoringDiscoveryProbeTests, DeclarationReflection_DiscoversAttributedOptions)
    {
        SlangCompilerService& service = SlangCompilerService::Get();
        const AZStd::unique_lock<AZStd::recursive_mutex> compilerLock = service.AcquireCompilerLock();

        auto sessionOutcome = service.CreateSession(MakeSessionDescriptor(false, nullptr));
        ASSERT_TRUE(sessionOutcome.IsSuccess());
        Slang::ComPtr<slang::ISession> session = sessionOutcome.TakeValue();

        slang::IModule* probeModule = LoadProbeModule(session, false);
        ASSERT_NE(probeModule, nullptr);

        // Walk the module declaration tree for [AtomOption]-tagged variables
        struct DiscoveredOption
        {
            AZStd::string m_name;
            AZStd::string m_defaultValue;
        };
        AZStd::vector<DiscoveredOption> discoveredOptions;

        slang::DeclReflection* moduleDecl = probeModule->getModuleReflection();
        ASSERT_NE(moduleDecl, nullptr);
        for (unsigned childIndex = 0; childIndex < moduleDecl->getChildrenCount(); ++childIndex)
        {
            slang::DeclReflection* child = moduleDecl->getChild(childIndex);
            slang::VariableReflection* variable = child ? child->asVariable() : nullptr;
            if (!variable)
            {
                continue;
            }
            for (unsigned attributeIndex = 0; attributeIndex < variable->getUserAttributeCount(); ++attributeIndex)
            {
                slang::UserAttribute* attribute = variable->getUserAttributeByIndex(attributeIndex);
                if (azstricmp(attribute->getName(), "AtomOption") != 0)
                {
                    continue;
                }
                size_t defaultLength = 0;
                const char* defaultText = attribute->getArgumentValueString(0, &defaultLength);
                discoveredOptions.push_back({
                    variable->getName(),
                    defaultText ? AZStd::string(defaultText, defaultLength) : AZStd::string()});
            }
        }

        ASSERT_EQ(discoveredOptions.size(), 2);
        EXPECT_EQ(discoveredOptions[0].m_name, "o_useTint");
        EXPECT_EQ(discoveredOptions[0].m_defaultValue, "false");
        EXPECT_EQ(discoveredOptions[1].m_name, "o_quality");
        EXPECT_EQ(discoveredOptions[1].m_defaultValue, "1");
    }

    TEST_F(OptionsAuthoringDiscoveryProbeTests, GuardedDeclarations_CoexistWithGeneratedModule)
    {
        // Production compile shape: define set, declarations compiled out, identifiers provided
        // by the injected generated module (spec-constant mode here)
        ASSERT_TRUE(AZ::Test::CreateTestFile(
            m_probeDirectory,
            "GeneratedOptions.slang",
            R"(
module GeneratedOptions;

[vk::constant_id(0)]
public const bool o_useTint = false;

[vk::constant_id(1)]
public const int o_quality = 1;
)"));

        SlangCompilerService& service = SlangCompilerService::Get();
        const AZStd::unique_lock<AZStd::recursive_mutex> compilerLock = service.AcquireCompilerLock();

        auto sessionOutcome = service.CreateSession(MakeSessionDescriptor(true, m_probeDirectory.GetDirectory()));
        ASSERT_TRUE(sessionOutcome.IsSuccess());
        Slang::ComPtr<slang::ISession> session = sessionOutcome.TakeValue();

        slang::IModule* probeModule = LoadProbeModule(session, true);
        ASSERT_NE(probeModule, nullptr);

        Slang::ComPtr<slang::IEntryPoint> entryPoint;
        Slang::ComPtr<slang::IBlob> diagnostics;
        probeModule->findAndCheckEntryPoint("MainCS", SLANG_STAGE_COMPUTE, entryPoint.writeRef(), diagnostics.writeRef());
        SlangCompilerService::ReportDiagnostics("OptionsProbe", diagnostics, !entryPoint);
        ASSERT_NE(entryPoint, nullptr);

        slang::IComponentType* components[] = {probeModule, entryPoint.get()};
        Slang::ComPtr<slang::IComponentType> composedProgram;
        diagnostics = nullptr;
        ASSERT_TRUE(SLANG_SUCCEEDED(session->createCompositeComponentType(components, 2, composedProgram.writeRef(), diagnostics.writeRef())));

        Slang::ComPtr<slang::IComponentType> linkedProgram;
        diagnostics = nullptr;
        ASSERT_TRUE(SLANG_SUCCEEDED(composedProgram->link(linkedProgram.writeRef(), diagnostics.writeRef())));

        Slang::ComPtr<slang::IBlob> byteCode;
        diagnostics = nullptr;
        const SlangResult codeResult = linkedProgram->getEntryPointCode(0, 0, byteCode.writeRef(), diagnostics.writeRef());
        SlangCompilerService::ReportDiagnostics("OptionsProbe", diagnostics, SLANG_FAILED(codeResult));
        ASSERT_TRUE(SLANG_SUCCEEDED(codeResult));
        EXPECT_GT(byteCode->getBufferSize(), 0);
    }

    TEST_F(OptionsAuthoringDiscoveryProbeTests, DiscoveryPass_Timing)
    {
        SlangCompilerService& service = SlangCompilerService::Get();
        const AZStd::unique_lock<AZStd::recursive_mutex> compilerLock = service.AcquireCompilerLock();

        // Warm the global session so the measurement excludes one-time core-module loading
        {
            auto warmupOutcome = service.CreateSession(MakeSessionDescriptor(false, nullptr));
            ASSERT_TRUE(warmupOutcome.IsSuccess());
            Slang::ComPtr<slang::ISession> warmupSession = warmupOutcome.TakeValue();
            ASSERT_NE(LoadProbeModule(warmupSession, false), nullptr);
        }

        constexpr int iterationCount = 20;
        AZStd::chrono::steady_clock::duration totalDiscovery{};
        for (int iteration = 0; iteration < iterationCount; ++iteration)
        {
            const auto start = AZStd::chrono::steady_clock::now();

            auto sessionOutcome = service.CreateSession(MakeSessionDescriptor(false, nullptr));
            ASSERT_TRUE(sessionOutcome.IsSuccess());
            Slang::ComPtr<slang::ISession> session = sessionOutcome.TakeValue();
            slang::IModule* probeModule = LoadProbeModule(session, false);
            ASSERT_NE(probeModule, nullptr);

            int discoveredCount = 0;
            slang::DeclReflection* moduleDecl = probeModule->getModuleReflection();
            for (unsigned childIndex = 0; childIndex < moduleDecl->getChildrenCount(); ++childIndex)
            {
                slang::DeclReflection* child = moduleDecl->getChild(childIndex);
                slang::VariableReflection* variable = child ? child->asVariable() : nullptr;
                if (variable)
                {
                    for (unsigned attributeIndex = 0; attributeIndex < variable->getUserAttributeCount(); ++attributeIndex)
                    {
                        if (azstricmp(variable->getUserAttributeByIndex(attributeIndex)->getName(), "AtomOption") == 0)
                        {
                            ++discoveredCount;
                        }
                    }
                }
            }
            ASSERT_EQ(discoveredCount, 2);

            totalDiscovery += AZStd::chrono::steady_clock::now() - start;
        }

        const double averageMilliseconds =
            AZStd::chrono::duration_cast<AZStd::chrono::duration<double, AZStd::milli>>(totalDiscovery).count() / iterationCount;
        printf("[PROBE] options discovery pass (session + module load + decl walk): %.3f ms average over %d iterations\n",
            averageMilliseconds, iterationCount);
    }

} // namespace UnitTest
