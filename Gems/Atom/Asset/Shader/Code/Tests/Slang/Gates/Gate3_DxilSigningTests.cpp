/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// Feasibility gate 3 (SlangIntegrationPlan.md, Phase 0A): DXIL validation/signing.
//
// Slang emits DXIL through an embedded DXC. Non-dev-mode D3D12 rejects DXIL whose container
// hash was not filled in by the validator (dxil.dll), so the emitted container must carry a
// populated digest. The escape hatch, if this ever regresses, is
// IGlobalSession::setDownstreamCompilerPath at an external DXC we deploy.

#include <AzTest/AzTest.h>
#include <AzCore/UnitTest/TestTypes.h>

#include <Common/ShaderBuilderTestFixture.h>

#include <Slang/SlangCompilerService.h>

namespace UnitTest
{
    using namespace AZ;
    using ShaderBuilder::SlangCompilerService;

    class Gate3_DxilSigningTests : public ShaderBuilderTestFixture
    {
    };

    TEST_F(Gate3_DxilSigningTests, EmittedDxilContainer_HasPopulatedValidationDigest)
    {
        constexpr AZStd::string_view source = R"(
RWStructuredBuffer<float4> Output;

[shader("compute")]
[numthreads(1, 1, 1)]
void MainCS(uint3 id : SV_DispatchThreadID)
{
    Output[id.x] = float4(1.0, 2.0, 3.0, 4.0);
}
)";
        SlangCompilerService::SessionDescriptor sessionDescriptor;
        sessionDescriptor.m_target = SLANG_DXIL;
        sessionDescriptor.m_profile = "sm_6_2";

        SlangCompilerService::EntryPointCompileRequest request;
        request.m_sourceCode = source;
        request.m_moduleName = "Gate3Signing";
        request.m_entryPointName = "MainCS";
        request.m_stage = SLANG_STAGE_COMPUTE;

        const AZ::Outcome<AZStd::vector<uint8_t>, AZStd::string> outcome =
            SlangCompilerService::Get().CompileEntryPointFromSource(sessionDescriptor, request);
        ASSERT_TRUE(outcome.IsSuccess()) << outcome.GetError().c_str();

        // DXIL container layout: 'DXBC' magic (4 bytes), then a 16-byte validation digest.
        // An unvalidated/unsigned container leaves the digest zeroed.
        const AZStd::vector<uint8_t>& byteCode = outcome.GetValue();
        ASSERT_GE(byteCode.size(), 20u);
        ASSERT_EQ(0, memcmp(byteCode.data(), "DXBC", 4));

        bool digestIsPopulated = false;
        for (size_t digestIndex = 4; digestIndex < 20; ++digestIndex)
        {
            if (byteCode[digestIndex] != 0)
            {
                digestIsPopulated = true;
                break;
            }
        }
        EXPECT_TRUE(digestIsPopulated) << "DXIL container digest is zeroed: the emitted DXIL is not validated/signed";
    }
} // namespace UnitTest
