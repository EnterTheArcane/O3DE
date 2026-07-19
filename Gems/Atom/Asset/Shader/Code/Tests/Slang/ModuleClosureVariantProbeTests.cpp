/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// M12 probe: variant builds from the serialized module closure instead of a source recompile.
// The frontend run serializes every loaded module; a variant build restores them in a fresh
// session, generates a per-variant option-values module, and relinks. Questions answered
// against the pinned compiler before the production wiring is built on top:
//
// 1. Restore: do IR-serialized modules reload from raw bytes (the production bundle stores
//    plain byte vectors, not the live blobs Gate 4 kept) and still answer findAndCheckEntryPoint?
// 2. Import seam: can a NEW module loaded from source text (the generated values module)
//    `import` an IR-restored module — resolving in-session with no file system at all?
// 3. Discovery: does options discovery (module-layout classification + decl reflection walk)
//    still see interfaces, attributes, enum cases, aliases and the fallback member on
//    IR-restored modules?
// 4. Fidelity: is closure-relinked bytecode byte-identical to a full source recompile composed
//    with the same generated values module?

#include <AzTest/AzTest.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzCore/std/parallel/atomic.h>

#include <Common/ShaderBuilderTestFixture.h>

#include <Slang/SlangCompilerService.h>
#include <Slang/SlangOptionsModuleGenerator.h>

#include <Atom/RPI.Reflect/Shader/ShaderOptionGroup.h>

namespace UnitTest
{
    using namespace AZ;
    using namespace AZ::ShaderBuilder;

    static bool IsUuidEqual(const SlangUUID& lhs, const SlangUUID& rhs)
    {
        return memcmp(&lhs, &rhs, sizeof(SlangUUID)) == 0;
    }

    //! Wraps owned bytes as an ISlangBlob, mirroring how the production bundle feeds
    //! loadModuleFromIRBlob from deserialized byte vectors.
    class ByteBlob final : public ISlangBlob
    {
    public:
        explicit ByteBlob(AZStd::vector<uint8_t> bytes)
            : m_bytes(AZStd::move(bytes))
        {
        }

        SLANG_NO_THROW SlangResult SLANG_MCALL queryInterface(const SlangUUID& uuid, void** outObject) override
        {
            if (IsUuidEqual(uuid, ISlangUnknown::getTypeGuid()) || IsUuidEqual(uuid, ISlangBlob::getTypeGuid()))
            {
                addRef();
                *outObject = static_cast<ISlangBlob*>(this);
                return SLANG_OK;
            }
            *outObject = nullptr;
            return SLANG_E_NO_INTERFACE;
        }

        SLANG_NO_THROW uint32_t SLANG_MCALL addRef() override
        {
            return ++m_referenceCount;
        }

        SLANG_NO_THROW uint32_t SLANG_MCALL release() override
        {
            const uint32_t remaining = --m_referenceCount;
            if (remaining == 0)
            {
                delete this;
            }
            return remaining;
        }

        SLANG_NO_THROW const void* SLANG_MCALL getBufferPointer() override
        {
            return m_bytes.data();
        }

        SLANG_NO_THROW size_t SLANG_MCALL getBufferSize() override
        {
            return m_bytes.size();
        }

    private:
        AZStd::vector<uint8_t> m_bytes;
        AZStd::atomic<uint32_t> m_referenceCount{1};
    };

    class ModuleClosureVariantProbeTests : public ShaderBuilderTestFixture
    {
    public:
        //! Same authoring form as the generator tests: attribute vocabulary inlined (production
        //! gets it from the force-included prelude), flat [AtomOption] extern functions — bool
        //! with the omitted-argument default + enum + ranged int — and a fallback-designated
        //! ShaderResourceGroup.
        static constexpr AZStd::string_view UseSiteSource = R"(
module ClosureProbe;

[__AttributeUsage(_AttributeTargets.Function)]
struct AtomOptionAttribute { int value = 0; };

[__AttributeUsage(_AttributeTargets.Function)]
struct AtomRangeAttribute { int min; int max; };

[__AttributeUsage(_AttributeTargets.Var)]
struct AtomVariantFallbackAttribute {};

public enum Quality
{
    Low,
    Medium,
    High,
}

[AtomOption]
public extern bool o_useTint();

[AtomOption(Quality.Medium)]
public extern Quality o_quality();

[AtomOption(4)] [AtomRange(1, 8)]
public extern int o_sampleCount();

public struct DrawShaderResourceGroup
{
    [AtomVariantFallback]
    public uint4 m_shaderVariantKeyFallback;
};
public ParameterBlock<DrawShaderResourceGroup> DrawSrg;

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
    value.x += float((int)o_quality());
    value.y += float(o_sampleCount());
    Output[id.x] = value;
}
)";

        static Slang::ComPtr<slang::ISession> MakeSession(SlangCompilerService& service)
        {
            SlangCompilerService::SessionDescriptor sessionDescriptor;
            sessionDescriptor.m_target = SLANG_SPIRV;
            sessionDescriptor.m_profile = "spirv_1_5";
            auto sessionOutcome = service.CreateSession(sessionDescriptor);
            if (!sessionOutcome.IsSuccess())
            {
                ADD_FAILURE() << sessionOutcome.GetError().c_str();
                return {};
            }
            return sessionOutcome.TakeValue();
        }

        //! Composes @rootModule + @implementationModule + the MainCS entry point, links, and
        //! returns the entry bytecode.
        static bool LinkAndGetByteCode(
            slang::ISession* session,
            slang::IModule* rootModule,
            slang::IModule* implementationModule,
            AZStd::vector<uint8_t>& outByteCode)
        {
            Slang::ComPtr<slang::IEntryPoint> entryPoint;
            Slang::ComPtr<slang::IBlob> diagnostics;
            rootModule->findAndCheckEntryPoint("MainCS", SLANG_STAGE_COMPUTE, entryPoint.writeRef(), diagnostics.writeRef());
            SlangCompilerService::ReportDiagnostics("ClosureProbe", diagnostics, !entryPoint);
            if (!entryPoint)
            {
                return false;
            }

            AZStd::vector<slang::IComponentType*> components;
            components.push_back(rootModule);
            components.push_back(implementationModule);
            components.push_back(entryPoint.get());

            Slang::ComPtr<slang::IComponentType> composedProgram;
            diagnostics = nullptr;
            if (SLANG_FAILED(session->createCompositeComponentType(
                    components.data(), components.size(), composedProgram.writeRef(), diagnostics.writeRef())))
            {
                SlangCompilerService::ReportDiagnostics("ClosureProbe", diagnostics, true);
                return false;
            }

            Slang::ComPtr<slang::IComponentType> linkedProgram;
            diagnostics = nullptr;
            if (SLANG_FAILED(composedProgram->link(linkedProgram.writeRef(), diagnostics.writeRef())))
            {
                SlangCompilerService::ReportDiagnostics("ClosureProbe", diagnostics, true);
                return false;
            }

            Slang::ComPtr<slang::IBlob> byteCode;
            diagnostics = nullptr;
            if (SLANG_FAILED(linkedProgram->getEntryPointCode(0, 0, byteCode.writeRef(), diagnostics.writeRef())) || !byteCode)
            {
                SlangCompilerService::ReportDiagnostics("ClosureProbe", diagnostics, true);
                return false;
            }
            const uint8_t* bytes = static_cast<const uint8_t*>(byteCode->getBufferPointer());
            outByteCode.assign(bytes, bytes + byteCode->getBufferSize());
            return true;
        }
    };

    TEST_F(ModuleClosureVariantProbeTests, IrRestoredClosure_DiscoveryGenerationAndRelink_MatchSourceCompile)
    {
        SlangCompilerService& service = SlangCompilerService::Get();
        const AZStd::unique_lock<AZStd::recursive_mutex> compilerLock = service.AcquireCompilerLock();

        // --- Frontend-equivalent session: source load + dynamic implementation module, then
        // serialize every module EXCEPT the generated one (a variant relink always regenerates it)
        struct SerializedModule
        {
            AZStd::string m_name;
            AZStd::vector<uint8_t> m_bytes;
        };
        AZStd::vector<SerializedModule> closure;
        AZStd::string generatedBakedModuleText;
        AZStd::vector<uint8_t> sourceCompiledByteCode;
        {
            Slang::ComPtr<slang::ISession> session = MakeSession(service);
            ASSERT_TRUE(session);

            Slang::ComPtr<slang::IBlob> diagnostics;
            slang::IModule* useSiteModule = session->loadModuleFromSourceString(
                "ClosureProbe", "ClosureProbe.slang", AZStd::string(UseSiteSource).c_str(), diagnostics.writeRef());
            SlangCompilerService::ReportDiagnostics("ClosureProbe", diagnostics, !useSiteModule);
            ASSERT_NE(useSiteModule, nullptr);

            auto discoveredOutcome = SlangOptionsModuleGenerator::DiscoverShaderOptions(session);
            ASSERT_TRUE(discoveredOutcome.IsSuccess()) << discoveredOutcome.GetError().c_str();
            const auto discovered = discoveredOutcome.TakeValue();
            auto layoutOutcome = SlangOptionsModuleGenerator::BuildShaderOptionGroupLayout(discovered.m_declarations);
            ASSERT_TRUE(layoutOutcome.IsSuccess()) << layoutOutcome.GetError().c_str();
            const RPI::Ptr<RPI::ShaderOptionGroupLayout> layout = layoutOutcome.TakeValue();

            // The dynamic implementation module is loaded exactly as the production frontend
            // does, so the serialized session state matches what a real bundle is built from
            const AZStd::string dynamicModuleText = SlangOptionsModuleGenerator::GenerateImplementationModule(
                ShaderOptionLoweringMode::DynamicFallback, RHI::ShaderTargetFormat::Spirv, "AtomGeneratedOptions", discovered, *layout);
            diagnostics = nullptr;
            slang::IModule* dynamicModule = session->loadModuleFromSourceString(
                "AtomGeneratedOptions", "AtomGeneratedOptions.slang", dynamicModuleText.c_str(), diagnostics.writeRef());
            SlangCompilerService::ReportDiagnostics("AtomGeneratedOptions", diagnostics, !dynamicModule);
            ASSERT_NE(dynamicModule, nullptr);

            const SlangInt moduleCount = session->getLoadedModuleCount();
            for (SlangInt moduleIndex = 0; moduleIndex < moduleCount; ++moduleIndex)
            {
                slang::IModule* module = session->getLoadedModule(moduleIndex);
                if (AZStd::string_view(module->getName()) == "AtomGeneratedOptions")
                {
                    continue;
                }
                Slang::ComPtr<ISlangBlob> moduleBlob;
                ASSERT_TRUE(SLANG_SUCCEEDED(module->serialize(moduleBlob.writeRef())) && moduleBlob);
                const uint8_t* bytes = static_cast<const uint8_t*>(moduleBlob->getBufferPointer());
                closure.push_back({module->getName(), {bytes, bytes + moduleBlob->getBufferSize()}});
            }
            ASSERT_FALSE(closure.empty());

            // Control leg: bake a fully-specified variant straight from source in this session
            RPI::ShaderOptionGroup bakedValues(layout);
            bakedValues.SetValue(Name{"o_useTint"}, Name{"true"});
            bakedValues.SetValue(Name{"o_quality"}, Name{"High"});
            bakedValues.SetValue(Name{"o_sampleCount"}, Name{"7"});
            generatedBakedModuleText =
                SlangOptionsModuleGenerator::GenerateBakedValuesModule("AtomGeneratedVariantOptions", discovered, bakedValues);

            diagnostics = nullptr;
            slang::IModule* bakedModule = session->loadModuleFromSourceString(
                "AtomGeneratedVariantOptions", "AtomGeneratedVariantOptions.slang", generatedBakedModuleText.c_str(), diagnostics.writeRef());
            SlangCompilerService::ReportDiagnostics("AtomGeneratedVariantOptions", diagnostics, !bakedModule);
            ASSERT_NE(bakedModule, nullptr);
            ASSERT_TRUE(LinkAndGetByteCode(session.get(), useSiteModule, bakedModule, sourceCompiledByteCode));
            ASSERT_FALSE(sourceCompiledByteCode.empty());
        }

        // --- Variant-build session: restore the closure from raw bytes with no file system,
        // re-discover, regenerate the values module from source text, relink
        {
            Slang::ComPtr<slang::ISession> session = MakeSession(service);
            ASSERT_TRUE(session);

            slang::IModule* restoredRootModule = nullptr;
            for (const SerializedModule& serializedModule : closure)
            {
                Slang::ComPtr<slang::IBlob> moduleBlob;
                moduleBlob.attach(new ByteBlob(serializedModule.m_bytes));
                Slang::ComPtr<slang::IBlob> diagnostics;
                slang::IModule* restoredModule = session->loadModuleFromIRBlob(
                    serializedModule.m_name.c_str(), serializedModule.m_name.c_str(), moduleBlob, diagnostics.writeRef());
                SlangCompilerService::ReportDiagnostics(serializedModule.m_name, diagnostics, !restoredModule);
                ASSERT_NE(restoredModule, nullptr) << "failed to restore module '" << serializedModule.m_name.c_str() << "'";
                if (serializedModule.m_name == "ClosureProbe")
                {
                    restoredRootModule = restoredModule;
                }
            }
            ASSERT_NE(restoredRootModule, nullptr);

            // Probe 3: discovery on IR-restored modules
            auto discoveredOutcome = SlangOptionsModuleGenerator::DiscoverShaderOptions(session);
            ASSERT_TRUE(discoveredOutcome.IsSuccess()) << discoveredOutcome.GetError().c_str();
            const auto discovered = discoveredOutcome.TakeValue();
            ASSERT_EQ(discovered.m_declarations.size(), 3);
            EXPECT_EQ(discovered.m_declarations[0].m_name, Name{"o_useTint"});
            EXPECT_EQ(discovered.m_declarations[0].m_type, RPI::ShaderOptionType::Boolean);
            EXPECT_EQ(discovered.m_declarations[0].m_defaultValue, Name{"false"});
            EXPECT_EQ(discovered.m_declarations[1].m_name, Name{"o_quality"});
            EXPECT_EQ(discovered.m_declarations[1].m_type, RPI::ShaderOptionType::Enumeration);
            EXPECT_EQ(discovered.m_declarations[1].m_typeText, "Quality");
            ASSERT_EQ(discovered.m_declarations[1].m_enumValues.size(), 3);
            EXPECT_EQ(discovered.m_declarations[1].m_enumValues[2], Name{"High"});
            EXPECT_EQ(discovered.m_declarations[1].m_defaultValue, Name{"Medium"});
            EXPECT_EQ(discovered.m_declarations[2].m_name, Name{"o_sampleCount"});
            EXPECT_EQ(discovered.m_declarations[2].m_type, RPI::ShaderOptionType::IntegerRange);
            EXPECT_EQ(discovered.m_declarations[2].m_minValue, 1);
            EXPECT_EQ(discovered.m_declarations[2].m_maxValue, 8);
            EXPECT_THAT(discovered.m_declaringModuleNames, ::testing::ElementsAre("ClosureProbe"));
            EXPECT_EQ(discovered.m_fallbackShaderResourceGroupName, "DrawSrg");
            EXPECT_EQ(discovered.m_fallbackMemberName, "m_shaderVariantKeyFallback");

            auto layoutOutcome = SlangOptionsModuleGenerator::BuildShaderOptionGroupLayout(discovered.m_declarations);
            ASSERT_TRUE(layoutOutcome.IsSuccess()) << layoutOutcome.GetError().c_str();
            const RPI::Ptr<RPI::ShaderOptionGroupLayout> layout = layoutOutcome.TakeValue();

            RPI::ShaderOptionGroup bakedValues(layout);
            bakedValues.SetValue(Name{"o_useTint"}, Name{"true"});
            bakedValues.SetValue(Name{"o_quality"}, Name{"High"});
            bakedValues.SetValue(Name{"o_sampleCount"}, Name{"7"});
            const AZStd::string relinkModuleText =
                SlangOptionsModuleGenerator::GenerateBakedValuesModule("AtomGeneratedVariantOptions", discovered, bakedValues);
            EXPECT_STREQ(relinkModuleText.c_str(), generatedBakedModuleText.c_str())
                << "IR-restored discovery generated a different values module than source discovery";

            // Probe 2: the generated module's `import ClosureProbe;` must resolve to the
            // IR-restored module — no file system exists in this session
            Slang::ComPtr<slang::IBlob> diagnostics;
            slang::IModule* bakedModule = session->loadModuleFromSourceString(
                "AtomGeneratedVariantOptions", "AtomGeneratedVariantOptions.slang", relinkModuleText.c_str(), diagnostics.writeRef());
            SlangCompilerService::ReportDiagnostics("AtomGeneratedVariantOptions", diagnostics, !bakedModule);
            ASSERT_NE(bakedModule, nullptr);

            // Probes 1 + 4: relink over the restored root module, byte-identical to the control
            AZStd::vector<uint8_t> relinkedByteCode;
            ASSERT_TRUE(LinkAndGetByteCode(session.get(), restoredRootModule, bakedModule, relinkedByteCode));
            ASSERT_EQ(relinkedByteCode.size(), sourceCompiledByteCode.size());
            EXPECT_EQ(memcmp(relinkedByteCode.data(), sourceCompiledByteCode.data(), relinkedByteCode.size()), 0)
                << "closure-relinked bytecode diverged from the source recompile";
        }
    }
} // namespace UnitTest
