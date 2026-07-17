/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// Feasibility gate 6 (SlangIntegrationPlan.md, Phase 0A): thread-safety characterization.
//
// slang.h documents the contract (IGlobalSession comment): a single global session and the
// objects created from it are NOT thread-safe and must be externally synchronized; distinct
// global sessions may be used from different threads in parallel. This gate exercises both
// halves so the SlangCompilerService mutex policy rests on observed behavior, not only docs:
// - the service's shared global session under its compiler lock, hammered from worker threads;
// - fully independent global sessions running compiles concurrently with no locking.

#include <AzTest/AzTest.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzCore/std/parallel/atomic.h>
#include <AzCore/std/parallel/thread.h>

#include <Common/ShaderBuilderTestFixture.h>

#include <Slang/SlangCompilerService.h>

namespace UnitTest
{
    using namespace AZ;
    using ShaderBuilder::SlangCompilerService;

    class Gate6_ThreadSafetyCharacterizationTests : public ShaderBuilderTestFixture
    {
    public:
        static constexpr AZStd::string_view TrivialComputeSource = R"(
RWStructuredBuffer<float4> Output;

[shader("compute")]
[numthreads(1, 1, 1)]
void MainCS(uint3 id : SV_DispatchThreadID)
{
    Output[id.x] = float4(1.0, 2.0, 3.0, 4.0);
}
)";
        static constexpr int ThreadCount = 4;
        static constexpr int CompilesPerThread = 3;
    };

    TEST_F(Gate6_ThreadSafetyCharacterizationTests, SharedGlobalSession_UnderServiceLock_CompilesFromManyThreads)
    {
        AZStd::atomic_int successCount = 0;

        AZStd::vector<AZStd::thread> threads;
        threads.reserve(ThreadCount);
        for (int threadIndex = 0; threadIndex < ThreadCount; ++threadIndex)
        {
            threads.emplace_back(
                [threadIndex, &successCount]()
                {
                    for (int compileIndex = 0; compileIndex < CompilesPerThread; ++compileIndex)
                    {
                        SlangCompilerService::SessionDescriptor sessionDescriptor;
                        sessionDescriptor.m_target = SLANG_SPIRV;
                        sessionDescriptor.m_profile = "spirv_1_5";

                        SlangCompilerService::EntryPointCompileRequest request;
                        request.m_sourceCode = TrivialComputeSource;
                        const AZStd::string moduleName = AZStd::string::format("Gate6Shared_%d_%d", threadIndex, compileIndex);
                        request.m_moduleName = moduleName;
                        request.m_entryPointName = "MainCS";
                        request.m_stage = SLANG_STAGE_COMPUTE;

                        // CompileEntryPointFromSource acquires the service's compiler lock internally.
                        const AZ::Outcome<AZStd::vector<uint8_t>, AZStd::string> outcome =
                            SlangCompilerService::Get().CompileEntryPointFromSource(sessionDescriptor, request);
                        if (outcome.IsSuccess() && !outcome.GetValue().empty())
                        {
                            ++successCount;
                        }
                    }
                });
        }
        for (AZStd::thread& thread : threads)
        {
            thread.join();
        }

        EXPECT_EQ(successCount.load(), ThreadCount * CompilesPerThread);
    }

    TEST_F(Gate6_ThreadSafetyCharacterizationTests, DistinctGlobalSessions_CompileInParallel_NoLocking)
    {
        AZStd::atomic_int successCount = 0;

        AZStd::vector<AZStd::thread> threads;
        threads.reserve(ThreadCount);
        for (int threadIndex = 0; threadIndex < ThreadCount; ++threadIndex)
        {
            threads.emplace_back(
                [threadIndex, &successCount]()
                {
                    // Each thread owns an entire compiler universe: global session, session, compile.
                    Slang::ComPtr<slang::IGlobalSession> globalSession;
                    if (SLANG_FAILED(slang::createGlobalSession(globalSession.writeRef())) || !globalSession)
                    {
                        return;
                    }

                    slang::TargetDesc targetDesc = {};
                    targetDesc.format = SLANG_SPIRV;
                    targetDesc.profile = globalSession->findProfile("spirv_1_5");

                    slang::SessionDesc sessionDesc = {};
                    sessionDesc.targets = &targetDesc;
                    sessionDesc.targetCount = 1;

                    Slang::ComPtr<slang::ISession> session;
                    if (SLANG_FAILED(globalSession->createSession(sessionDesc, session.writeRef())) || !session)
                    {
                        return;
                    }

                    const AZStd::string moduleName = AZStd::string::format("Gate6Distinct_%d", threadIndex);
                    const AZStd::string source(TrivialComputeSource);
                    Slang::ComPtr<slang::IBlob> diagnostics;
                    slang::IModule* module = session->loadModuleFromSourceString(moduleName.c_str(), moduleName.c_str(), source.c_str(), diagnostics.writeRef());
                    if (!module)
                    {
                        return;
                    }

                    Slang::ComPtr<slang::IEntryPoint> entryPoint;
                    module->findAndCheckEntryPoint("MainCS", SLANG_STAGE_COMPUTE, entryPoint.writeRef(), diagnostics.writeRef());
                    if (!entryPoint)
                    {
                        return;
                    }

                    slang::IComponentType* components[] = {module, entryPoint.get()};
                    Slang::ComPtr<slang::IComponentType> composite;
                    session->createCompositeComponentType(components, AZ_ARRAY_SIZE(components), composite.writeRef(), diagnostics.writeRef());
                    Slang::ComPtr<slang::IComponentType> linkedProgram;
                    if (composite)
                    {
                        composite->link(linkedProgram.writeRef(), diagnostics.writeRef());
                    }
                    if (!linkedProgram)
                    {
                        return;
                    }

                    Slang::ComPtr<slang::IBlob> code;
                    if (SLANG_SUCCEEDED(linkedProgram->getEntryPointCode(0, 0, code.writeRef(), diagnostics.writeRef())) &&
                        code && code->getBufferSize() > 0)
                    {
                        ++successCount;
                    }
                });
        }
        for (AZStd::thread& thread : threads)
        {
            thread.join();
        }

        EXPECT_EQ(successCount.load(), ThreadCount);
    }
} // namespace UnitTest
