/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// Feasibility gate 4 (SlangIntegrationPlan.md, Phase 0A): module-closure serialize/reload.
//
// A serialized Slang module does not contain its imports (verified with slangc during
// planning), so the variant-build cache must store the whole closure: every module the
// session loaded. This gate proves the closure round-trips: serialize all loaded modules,
// reload them into a fresh session with NO search paths (so source access is impossible),
// link, and produce bytecode identical to the source compile.

#include <AzTest/AzTest.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/StringFunc/StringFunc.h>
#include <AzCore/Utils/Utils.h>

#include <Common/ShaderBuilderTestFixture.h>

#include <Slang/SlangCompilerService.h>

namespace UnitTest
{
    using namespace AZ;
    using ShaderBuilder::SlangCompilerService;

    class Gate4_ModuleClosureReloadTests : public ShaderBuilderTestFixture
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

        static constexpr AZStd::string_view DependencyModuleSource = R"(
module DepModule;

public float4 MakeColor()
{
    return float4(0.25, 0.5, 0.75, 1.0);
}
)";

        static constexpr AZStd::string_view RootModuleSource = R"(
module RootModule;

import DepModule;

RWStructuredBuffer<float4> Output;

[shader("compute")]
[numthreads(1, 1, 1)]
void MainCS(uint3 id : SV_DispatchThreadID)
{
    Output[id.x] = MakeColor();
}
)";

        //! One serialized module of the closure.
        struct SerializedModule
        {
            AZStd::string m_name;
            Slang::ComPtr<slang::IBlob> m_blob;
        };

        static bool LinkAndGetEntryPointCode(slang::ISession* session, slang::IModule* rootModule, AZStd::vector<uint8_t>& outByteCode)
        {
            Slang::ComPtr<slang::IBlob> diagnostics;
            Slang::ComPtr<slang::IEntryPoint> entryPoint;
            rootModule->findAndCheckEntryPoint("MainCS", SLANG_STAGE_COMPUTE, entryPoint.writeRef(), diagnostics.writeRef());
            SlangCompilerService::ReportDiagnostics("Gate4", diagnostics, !entryPoint);
            if (!entryPoint)
            {
                return false;
            }

            slang::IComponentType* components[] = {rootModule, entryPoint.get()};
            Slang::ComPtr<slang::IComponentType> composite;
            diagnostics = nullptr;
            session->createCompositeComponentType(components, AZ_ARRAY_SIZE(components), composite.writeRef(), diagnostics.writeRef());
            Slang::ComPtr<slang::IComponentType> linkedProgram;
            if (composite)
            {
                composite->link(linkedProgram.writeRef(), diagnostics.writeRef());
            }
            SlangCompilerService::ReportDiagnostics("Gate4", diagnostics, !linkedProgram);
            if (!linkedProgram)
            {
                return false;
            }

            Slang::ComPtr<slang::IBlob> code;
            diagnostics = nullptr;
            const SlangResult result = linkedProgram->getEntryPointCode(0, 0, code.writeRef(), diagnostics.writeRef());
            SlangCompilerService::ReportDiagnostics("Gate4", diagnostics, SLANG_FAILED(result));
            if (SLANG_FAILED(result) || !code || code->getBufferSize() == 0)
            {
                return false;
            }

            const uint8_t* codeBegin = static_cast<const uint8_t*>(code->getBufferPointer());
            outByteCode.assign(codeBegin, codeBegin + code->getBufferSize());
            return true;
        }

        AZStd::unique_ptr<AZ::Test::ScopedAutoTempDirectory> m_tempDirectory;
    };

    TEST_F(Gate4_ModuleClosureReloadTests, SerializedClosure_ReloadsWithoutSourceAccess_ByteCodeMatches)
    {
        // The dependency lives on disk so the root import resolves through session search paths,
        // mirroring how ShaderLib modules will be found in production.
        ASSERT_TRUE(AZ::Test::CreateTestFile(*m_tempDirectory, "DepModule.slang", DependencyModuleSource));

        SlangCompilerService& service = SlangCompilerService::Get();
        const AZStd::unique_lock<AZStd::recursive_mutex> compilerLock = service.AcquireCompilerLock();

        SlangCompilerService::SessionDescriptor sourceSessionDescriptor;
        sourceSessionDescriptor.m_target = SLANG_SPIRV;
        sourceSessionDescriptor.m_profile = "spirv_1_5";
        sourceSessionDescriptor.m_searchPaths.push_back(m_tempDirectory->GetDirectory());

        // 1) Compile from source and capture the reference bytecode + the serialized closure.
        AZStd::vector<uint8_t> sourceByteCode;
        AZStd::vector<SerializedModule> serializedClosure;
        {
            AZ::Outcome<Slang::ComPtr<slang::ISession>, AZStd::string> sessionOutcome = service.CreateSession(sourceSessionDescriptor);
            ASSERT_TRUE(sessionOutcome.IsSuccess()) << sessionOutcome.GetError().c_str();
            Slang::ComPtr<slang::ISession> session = sessionOutcome.TakeValue();

            const AZStd::string rootSource(RootModuleSource);
            Slang::ComPtr<slang::IBlob> diagnostics;
            slang::IModule* rootModule = session->loadModuleFromSourceString("RootModule", "RootModule.slang", rootSource.c_str(), diagnostics.writeRef());
            SlangCompilerService::ReportDiagnostics("RootModule.slang", diagnostics, rootModule == nullptr);
            ASSERT_NE(rootModule, nullptr);

            ASSERT_TRUE(LinkAndGetEntryPointCode(session.get(), rootModule, sourceByteCode));

            // Serialize the whole closure: every module the session loaded, in load order.
            const SlangInt loadedModuleCount = session->getLoadedModuleCount();
            ASSERT_GE(loadedModuleCount, 2) << "expected at least the root module and its import";
            for (SlangInt moduleIndex = 0; moduleIndex < loadedModuleCount; ++moduleIndex)
            {
                slang::IModule* loadedModule = session->getLoadedModule(moduleIndex);
                SerializedModule serializedModule;
                serializedModule.m_name = loadedModule->getName();
                ASSERT_TRUE(SLANG_SUCCEEDED(loadedModule->serialize(serializedModule.m_blob.writeRef())));
                ASSERT_NE(serializedModule.m_blob, nullptr);
                serializedClosure.push_back(AZStd::move(serializedModule));
            }
        }

        // 2) Fresh session with NO search paths: source access is impossible; only the blobs exist.
        SlangCompilerService::SessionDescriptor reloadSessionDescriptor;
        reloadSessionDescriptor.m_target = SLANG_SPIRV;
        reloadSessionDescriptor.m_profile = "spirv_1_5";
        {
            AZ::Outcome<Slang::ComPtr<slang::ISession>, AZStd::string> sessionOutcome = service.CreateSession(reloadSessionDescriptor);
            ASSERT_TRUE(sessionOutcome.IsSuccess()) << sessionOutcome.GetError().c_str();
            Slang::ComPtr<slang::ISession> session = sessionOutcome.TakeValue();

            slang::IModule* reloadedRootModule = nullptr;
            for (const SerializedModule& serializedModule : serializedClosure)
            {
                Slang::ComPtr<slang::IBlob> diagnostics;
                slang::IModule* reloadedModule = session->loadModuleFromIRBlob(
                    serializedModule.m_name.c_str(),
                    serializedModule.m_name.c_str(),
                    serializedModule.m_blob,
                    diagnostics.writeRef());
                SlangCompilerService::ReportDiagnostics(serializedModule.m_name, diagnostics, reloadedModule == nullptr);
                ASSERT_NE(reloadedModule, nullptr) << "failed to reload module '" << serializedModule.m_name.c_str() << "' from its blob";
                if (serializedModule.m_name == "RootModule")
                {
                    reloadedRootModule = reloadedModule;
                }
            }
            ASSERT_NE(reloadedRootModule, nullptr);

            AZStd::vector<uint8_t> reloadedByteCode;
            ASSERT_TRUE(LinkAndGetEntryPointCode(session.get(), reloadedRootModule, reloadedByteCode));

            // 3) The closure round-trip must be lossless: identical bytecode.
            ASSERT_EQ(reloadedByteCode.size(), sourceByteCode.size());
            EXPECT_EQ(0, memcmp(reloadedByteCode.data(), sourceByteCode.data(), sourceByteCode.size()));
        }
    }
} // namespace UnitTest
