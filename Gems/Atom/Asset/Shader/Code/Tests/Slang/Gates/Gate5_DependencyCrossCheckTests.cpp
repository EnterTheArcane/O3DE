/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// Feasibility gate 5 (SlangIntegrationPlan.md, Phase 0A): dependency scanning/invalidation.
//
// The resolver unit tests cover the name-mapping and shadowing rules; this cross-check closes
// the loop against the compiler itself: every source file Slang actually loaded for a module
// (IModule::getDependencyFilePath) must be predicted by SlangModuleResolver — the scanner that
// declares CreateJobs source dependencies. A file the compiler read but the scanner missed
// would mean stale-asset bugs, so production fails such jobs loudly (D9); here it fails the gate.

#include <AzTest/AzTest.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/StringFunc/StringFunc.h>
#include <AzCore/Utils/Utils.h>

#include <Common/ShaderBuilderTestFixture.h>

#include <Slang/SlangCompilerService.h>
#include <Slang/SlangModuleResolver.h>

namespace UnitTest
{
    using namespace AZ;
    using ShaderBuilder::SlangCompilerService;
    using ShaderBuilder::SlangModuleResolver;

    class Gate5_DependencyCrossCheckTests : public ShaderBuilderTestFixture
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

        static AZStd::string NormalizePath(AZStd::string_view path)
        {
            AZStd::string normalized(path);
            AZ::StringFunc::Path::Normalize(normalized);
            AZStd::to_lower(normalized.begin(), normalized.end());
            return normalized;
        }

        AZStd::unique_ptr<AZ::Test::ScopedAutoTempDirectory> m_tempDirectory;
    };

    TEST_F(Gate5_DependencyCrossCheckTests, CompilerLoadedFiles_AreAllPredictedByResolver)
    {
        constexpr AZStd::string_view helperSource = R"(
module Helpers;

public float4 Tint()
{
    return float4(0.5, 0.5, 0.5, 1.0);
}
)";
        constexpr AZStd::string_view rootSource = R"(
module Gate5Root;

import Atom.Test.Helpers;

RWStructuredBuffer<float4> Output;

[shader("compute")]
[numthreads(1, 1, 1)]
void MainCS(uint3 id : SV_DispatchThreadID)
{
    Output[id.x] = Tint();
}
)";
        ASSERT_TRUE(AZ::Test::CreateTestFile(*m_tempDirectory, "Atom/Test/Helpers.slang", helperSource));
        ASSERT_TRUE(AZ::Test::CreateTestFile(*m_tempDirectory, "Gate5Root.slang", rootSource));

        AZStd::string rootPath;
        AZ::StringFunc::Path::Join(m_tempDirectory->GetDirectory(), "Gate5Root.slang", rootPath);

        // 1) The scanner's view: parse the references and resolve each one.
        SlangModuleResolver resolver({m_tempDirectory->GetDirectory()});
        AZStd::vector<AZStd::string> predictedFiles;
        predictedFiles.push_back(NormalizePath(rootPath));
        AZ::IO::SystemFile rootFile;
        ASSERT_TRUE(rootFile.Open(rootPath.c_str(), AZ::IO::SystemFile::SF_OPEN_READ_ONLY));
        AZStd::string rootContent;
        rootContent.resize_no_construct(rootFile.Length());
        rootFile.Read(rootContent.size(), rootContent.data());
        rootFile.Close();
        for (const AZStd::string& reference : SlangModuleResolver::ParseModuleReferences(rootContent))
        {
            const SlangModuleResolver::Resolution resolution = resolver.ResolveModule(reference, rootPath);
            if (resolution.IsResolved())
            {
                predictedFiles.push_back(NormalizePath(resolution.m_resolvedPath));
            }
        }

        // 2) The compiler's view: load the root module from disk and enumerate what it read.
        SlangCompilerService& service = SlangCompilerService::Get();
        const AZStd::unique_lock<AZStd::recursive_mutex> compilerLock = service.AcquireCompilerLock();

        SlangCompilerService::SessionDescriptor sessionDescriptor;
        sessionDescriptor.m_target = SLANG_SPIRV;
        sessionDescriptor.m_profile = "spirv_1_5";
        sessionDescriptor.m_searchPaths.push_back(m_tempDirectory->GetDirectory());

        AZ::Outcome<Slang::ComPtr<slang::ISession>, AZStd::string> sessionOutcome = service.CreateSession(sessionDescriptor);
        ASSERT_TRUE(sessionOutcome.IsSuccess()) << sessionOutcome.GetError().c_str();
        Slang::ComPtr<slang::ISession> session = sessionOutcome.TakeValue();

        Slang::ComPtr<slang::IBlob> diagnostics;
        slang::IModule* rootModule = session->loadModule("Gate5Root", diagnostics.writeRef());
        SlangCompilerService::ReportDiagnostics("Gate5Root", diagnostics, rootModule == nullptr);
        ASSERT_NE(rootModule, nullptr);

        // 3) Every file the compiler loaded from our temp root must be a predicted dependency.
        const AZStd::string normalizedTempRoot = NormalizePath(m_tempDirectory->GetDirectory());
        const SlangInt32 dependencyCount = rootModule->getDependencyFileCount();
        ASSERT_GE(dependencyCount, 2);
        for (SlangInt32 dependencyIndex = 0; dependencyIndex < dependencyCount; ++dependencyIndex)
        {
            const AZStd::string dependencyPath = NormalizePath(rootModule->getDependencyFilePath(dependencyIndex));
            if (!dependencyPath.starts_with(normalizedTempRoot))
            {
                continue; // Core-module or other compiler-internal files are not source dependencies.
            }
            EXPECT_TRUE(AZStd::find(predictedFiles.begin(), predictedFiles.end(), dependencyPath) != predictedFiles.end())
                << "compiler loaded a file the scanner did not predict: " << dependencyPath.c_str();
        }
    }
} // namespace UnitTest
