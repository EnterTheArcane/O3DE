/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzTest/AzTest.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/StringFunc/StringFunc.h>
#include <AzCore/Utils/Utils.h>

#include <Common/ShaderBuilderTestFixture.h>

#include <Slang/SlangModuleResolver.h>

namespace UnitTest
{
    using namespace AZ;
    using ShaderBuilder::SlangModuleResolver;

    class SlangModuleResolverTests : public ShaderBuilderTestFixture
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

        //! Creates @relativePath (and parent folders) under the temp directory with trivial content.
        //! Returns the full path.
        static AZStd::string CreateFile(const AZStd::string_view& root, const AZStd::string_view& relativePath)
        {
            AZStd::string fullPath;
            AZ::StringFunc::Path::Join(root.data(), relativePath.data(), fullPath);

            AZStd::string directory;
            AZ::StringFunc::Path::GetFullPath(fullPath.c_str(), directory);
            AZ::IO::SystemFile::CreateDir(directory.c_str());

            AZ::IO::SystemFile file;
            EXPECT_TRUE(file.Open(fullPath.c_str(), AZ::IO::SystemFile::SF_OPEN_CREATE | AZ::IO::SystemFile::SF_OPEN_WRITE_ONLY));
            constexpr char content[] = "// test module\n";
            file.Write(content, sizeof(content) - 1);
            file.Close();
            return fullPath;
        }

        //! Returns a subdirectory path of the test temp directory (not created unless asked).
        AZStd::string Subdirectory(const AZStd::string_view& name, bool create = true)
        {
            AZStd::string path;
            AZ::StringFunc::Path::Join(m_tempDirectory->GetDirectory(), name.data(), path);
            if (create)
            {
                AZ::IO::SystemFile::CreateDir(path.c_str());
            }
            return path;
        }

        AZStd::unique_ptr<AZ::Test::ScopedAutoTempDirectory> m_tempDirectory;
    };

    TEST_F(SlangModuleResolverTests, RelativePathCandidates_DottedName_MapsToPath)
    {
        const AZStd::vector<AZStd::string> candidates = SlangModuleResolver::GetRelativePathCandidates("Atom.RPI.Prelude");
        ASSERT_EQ(candidates.size(), 1u);
        EXPECT_STREQ(candidates[0].c_str(), "Atom/RPI/Prelude.slang");
    }

    TEST_F(SlangModuleResolverTests, RelativePathCandidates_UnderscoreName_AlsoMatchesDashedFile)
    {
        const AZStd::vector<AZStd::string> candidates = SlangModuleResolver::GetRelativePathCandidates("Atom.my_module");
        ASSERT_EQ(candidates.size(), 2u);
        EXPECT_STREQ(candidates[0].c_str(), "Atom/my_module.slang");
        EXPECT_STREQ(candidates[1].c_str(), "Atom/my-module.slang");
    }

    TEST_F(SlangModuleResolverTests, RelativePathCandidates_StringForm_IsVerbatim)
    {
        const AZStd::vector<AZStd::string> candidates = SlangModuleResolver::GetRelativePathCandidates("Atom/RPI/file-name.slang");
        ASSERT_EQ(candidates.size(), 1u);
        EXPECT_STREQ(candidates[0].c_str(), "Atom/RPI/file-name.slang");
    }

    TEST_F(SlangModuleResolverTests, ParseModuleReferences_FindsAllForms)
    {
        constexpr AZStd::string_view source = R"(
// A comment mentioning import NotThisOne (no semicolon on this line, so it is not matched).
import Atom.RPI.Prelude;
import "Atom/RPI/file-name.slang";
__include Atom.Features.Helpers;
import First.Module, Second.Module;
void MainCS() {}
)";
        const AZStd::vector<AZStd::string> references = SlangModuleResolver::ParseModuleReferences(source);

        const AZStd::vector<AZStd::string> expected = {
            "Atom.RPI.Prelude",
            "Atom/RPI/file-name.slang",
            "Atom.Features.Helpers",
            "First.Module",
            "Second.Module",
        };
        for (const AZStd::string& expectedReference : expected)
        {
            EXPECT_TRUE(AZStd::find(references.begin(), references.end(), expectedReference) != references.end())
                << "missing reference: " << expectedReference.c_str();
        }
    }

    TEST_F(SlangModuleResolverTests, ResolveModule_FindsFileInSearchRoot)
    {
        const AZStd::string rootA = Subdirectory("RootA");
        const AZStd::string expectedPath = CreateFile(rootA, "Atom/RPI/Prelude.slang");

        SlangModuleResolver resolver({rootA});
        const SlangModuleResolver::Resolution resolution = resolver.ResolveModule("Atom.RPI.Prelude", {});

        ASSERT_TRUE(resolution.IsResolved());
        EXPECT_TRUE(AZ::StringFunc::Equal(resolution.m_resolvedPath, expectedPath, false));
        EXPECT_TRUE(resolution.m_shadowCandidates.empty());
    }

    TEST_F(SlangModuleResolverTests, ResolveModule_LowerPriorityMatch_ReportsHigherPriorityShadowCandidates)
    {
        const AZStd::string rootHigh = Subdirectory("RootHigh");
        const AZStd::string rootLow = Subdirectory("RootLow");
        CreateFile(rootLow, "Atom/Thing.slang");

        SlangModuleResolver resolver({rootHigh, rootLow});
        const SlangModuleResolver::Resolution resolution = resolver.ResolveModule("Atom.Thing", {});

        ASSERT_TRUE(resolution.IsResolved());
        EXPECT_TRUE(resolution.m_resolvedPath.contains("RootLow"));

        // The nonexistent higher-priority location must be reported: if Atom/Thing.slang appears
        // in RootHigh later, it shadows the resolved file and dependents must rebuild.
        ASSERT_EQ(resolution.m_shadowCandidates.size(), 1u);
        EXPECT_TRUE(resolution.m_shadowCandidates[0].contains("RootHigh"));
    }

    TEST_F(SlangModuleResolverTests, ResolveModule_Unresolved_ReportsAllFutureCandidates)
    {
        const AZStd::string rootA = Subdirectory("RootA");
        const AZStd::string rootB = Subdirectory("RootB");

        SlangModuleResolver resolver({rootA, rootB});
        const SlangModuleResolver::Resolution resolution = resolver.ResolveModule("Missing.Module", {});

        EXPECT_FALSE(resolution.IsResolved());
        // One candidate per root (no underscore variant for this name).
        EXPECT_EQ(resolution.m_shadowCandidates.size(), 2u);
    }

    TEST_F(SlangModuleResolverTests, ResolveModule_ImportingFileDirectory_HasHighestPriority)
    {
        const AZStd::string rootA = Subdirectory("RootA");
        CreateFile(rootA, "Helper.slang");

        const AZStd::string localDirectory = Subdirectory("Local");
        const AZStd::string localHelper = CreateFile(localDirectory, "Helper.slang");
        const AZStd::string importer = CreateFile(localDirectory, "Main.slang");

        SlangModuleResolver resolver({rootA});
        const SlangModuleResolver::Resolution resolution = resolver.ResolveModule("Helper", importer);

        ASSERT_TRUE(resolution.IsResolved());
        EXPECT_TRUE(AZ::StringFunc::Equal(resolution.m_resolvedPath, localHelper, false));
    }

    TEST_F(SlangModuleResolverTests, ResolveModule_DashedFileMatchesUnderscoreName)
    {
        const AZStd::string rootA = Subdirectory("RootA");
        const AZStd::string dashedFile = CreateFile(rootA, "my-module.slang");

        SlangModuleResolver resolver({rootA});
        const SlangModuleResolver::Resolution resolution = resolver.ResolveModule("my_module", {});

        ASSERT_TRUE(resolution.IsResolved());
        EXPECT_TRUE(AZ::StringFunc::Equal(resolution.m_resolvedPath, dashedFile, false));
        // The underscore spelling in the same root would shadow the dashed one.
        ASSERT_EQ(resolution.m_shadowCandidates.size(), 1u);
        EXPECT_TRUE(resolution.m_shadowCandidates[0].contains("my_module.slang"));
    }
} // namespace UnitTest
