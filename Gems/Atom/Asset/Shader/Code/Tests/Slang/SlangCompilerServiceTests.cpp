/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzTest/AzTest.h>
#include <AzCore/UnitTest/TestTypes.h>

#include <Common/ShaderBuilderTestFixture.h>

#include <Slang/SlangCompilerService.h>

namespace UnitTest
{
    using namespace AZ;

    //! Exercises the in-process Slang compiler service: session creation, entry point
    //! compilation to both DXIL and SPIR-V, and diagnostics reporting on invalid source.
    class SlangCompilerServiceTests : public ShaderBuilderTestFixture
    {
    public:
        static constexpr AZStd::string_view TrivialComputeSource = R"(
RWStructuredBuffer<float4> Output;

[shader("compute")]
[numthreads(1, 1, 1)]
void MainCS(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    Output[dispatchThreadId.x] = float4(1.0, 2.0, 3.0, 4.0);
}
)";

        static ShaderBuilder::SlangCompilerService::SessionDescriptor MakeSessionDescriptor(
            SlangCompileTarget target,
            AZStd::string_view profile)
        {
            ShaderBuilder::SlangCompilerService::SessionDescriptor descriptor;
            descriptor.m_target = target;
            descriptor.m_profile = profile;
            return descriptor;
        }

        static ShaderBuilder::SlangCompilerService::EntryPointCompileRequest MakeComputeRequest(
            AZStd::string_view sourceCode,
            AZStd::string_view moduleName)
        {
            ShaderBuilder::SlangCompilerService::EntryPointCompileRequest request;
            request.m_sourceCode = sourceCode;
            request.m_moduleName = moduleName;
            request.m_entryPointName = "MainCS";
            request.m_stage = SLANG_STAGE_COMPUTE;
            return request;
        }
    };

    TEST_F(SlangCompilerServiceTests, CompilerBuildTag_IsAvailable)
    {
        const ShaderBuilder::SlangCompilerService& service = ShaderBuilder::SlangCompilerService::Get();
        EXPECT_FALSE(service.GetCompilerBuildTag().empty());
    }

    TEST_F(SlangCompilerServiceTests, CompileTrivialCompute_ToDxil_ProducesBytecode)
    {
        ShaderBuilder::SlangCompilerService& service = ShaderBuilder::SlangCompilerService::Get();
        const AZ::Outcome<AZStd::vector<uint8_t>, AZStd::string> outcome = service.CompileEntryPointFromSource(
            MakeSessionDescriptor(SLANG_DXIL, "sm_6_2"),
            MakeComputeRequest(TrivialComputeSource, "TrivialComputeDxil"));

        ASSERT_TRUE(outcome.IsSuccess()) << outcome.GetError().c_str();
        const AZStd::vector<uint8_t>& byteCode = outcome.GetValue();
        ASSERT_GT(byteCode.size(), 4u);
        // DXIL ships in a DXBC container.
        EXPECT_EQ(0, memcmp(byteCode.data(), "DXBC", 4));
    }

    TEST_F(SlangCompilerServiceTests, CompileTrivialCompute_ToSpirv_ProducesBytecode)
    {
        ShaderBuilder::SlangCompilerService& service = ShaderBuilder::SlangCompilerService::Get();
        const AZ::Outcome<AZStd::vector<uint8_t>, AZStd::string> outcome = service.CompileEntryPointFromSource(
            MakeSessionDescriptor(SLANG_SPIRV, "spirv_1_5"),
            MakeComputeRequest(TrivialComputeSource, "TrivialComputeSpirv"));

        ASSERT_TRUE(outcome.IsSuccess()) << outcome.GetError().c_str();
        const AZStd::vector<uint8_t>& byteCode = outcome.GetValue();
        ASSERT_GE(byteCode.size(), 4u);
        // SPIR-V magic number, little endian.
        constexpr uint32_t spirvMagic = 0x07230203u;
        EXPECT_EQ(0, memcmp(byteCode.data(), &spirvMagic, sizeof(spirvMagic)));
    }

    TEST_F(SlangCompilerServiceTests, CompileInvalidSource_FailsWithDiagnostics_NoCrash)
    {
        ShaderBuilder::SlangCompilerService& service = ShaderBuilder::SlangCompilerService::Get();

        // The diagnostics bridge reports through AZ_Error; capture it so the test run stays green.
        AZ_TEST_START_TRACE_SUPPRESSION;
        const AZ::Outcome<AZStd::vector<uint8_t>, AZStd::string> outcome = service.CompileEntryPointFromSource(
            MakeSessionDescriptor(SLANG_SPIRV, "spirv_1_5"),
            MakeComputeRequest("this is not valid slang source !!!", "InvalidSource"));
        AZ_TEST_STOP_TRACE_SUPPRESSION_NO_COUNT;

        EXPECT_FALSE(outcome.IsSuccess());
        EXPECT_FALSE(outcome.GetError().empty());
    }

    TEST_F(SlangCompilerServiceTests, SequentialSessions_ShareOneGlobalSession)
    {
        ShaderBuilder::SlangCompilerService& service = ShaderBuilder::SlangCompilerService::Get();

        for (int iteration = 0; iteration < 2; ++iteration)
        {
            const AZStd::unique_lock<AZStd::recursive_mutex> compilerLock = service.AcquireCompilerLock();
            const AZ::Outcome<Slang::ComPtr<slang::ISession>, AZStd::string> sessionOutcome = service.CreateSession(MakeSessionDescriptor(SLANG_SPIRV, "spirv_1_5"));
            ASSERT_TRUE(sessionOutcome.IsSuccess()) << sessionOutcome.GetError().c_str();
            EXPECT_NE(sessionOutcome.GetValue().get(), nullptr);
        }
    }
} // namespace UnitTest
