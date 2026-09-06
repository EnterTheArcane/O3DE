/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzTest/AzTest.h>
#include <AzCore/UnitTest/TestTypes.h>

#include "Common/ShaderBuilderTestFixture.h"

#include <ShaderBuilderUtility.h>

namespace UnitTest
{
    using namespace AZ;

    // The main purpose of this class is to test ShaderBuilderUtility functions
    class ShaderBuilderUtilityTests : public ShaderBuilderTestFixture
    {
    }; // class ShaderBuilderUtilityTests

    void ExpectHasIncludeFile(const AZStd::vector<AZStd::string> fileList, bool shouldExist, const char* filePath)
    {
        //We must normalize because internally AZ::ShaderBuilder::ShaderBuilderUtility::IncludedFilesParser
        // always returns normalized paths.
        AZStd::string filePathNormalized(filePath);
        AzFramework::StringFunc::Path::Normalize(filePathNormalized);
        auto it = AZStd::find(fileList.begin(), fileList.end(), filePathNormalized);
        if (shouldExist)
        {
            EXPECT_TRUE(it != fileList.end()) << "Could not find path '" << filePath << "' in the include list.";
        }
        else
        {
            EXPECT_TRUE(it == fileList.end()) << "Path '" << filePath << "' should not be in the include list.";
        }
    }

    TEST_F(ShaderBuilderUtilityTests, IncludedFilesParser_ParseStringAndGetIncludedFiles)
    {
        AZStd::string haystack(
            R"(
                Some content to parse
                #include <valid_file1.azsli>
                // #include <valid_file2.azsli>
                blah # include "valid_file3.azsli"
                bar include <a\dire-ctory\invalid-file4.azsli>
                foo #   include "a/directory/valid-file5.azsli"
                # include <a\dire-ctory\valid-file6.azsli>
                #includ "a\dire-ctory\invalid-file7.azsli"
                #include <..\Relative\Path\To\File.azsi>
                #include <C:\Absolute\Path\To\File.azsi>
            )"
        );

        AZ::ShaderBuilder::ShaderBuilderUtility::IncludedFilesParser includedFilesParser;
        auto fileList = includedFilesParser.ParseStringAndGetIncludedFiles(haystack);
        EXPECT_EQ(fileList.size(), 7);

        ExpectHasIncludeFile(fileList, true, "valid_file1.azsli");
        ExpectHasIncludeFile(fileList, true, "valid_file2.azsli");
        ExpectHasIncludeFile(fileList, true, "valid_file3.azsli");
        ExpectHasIncludeFile(fileList, false, "a\\dire-ctory\\invalid-file4.azsli");
        ExpectHasIncludeFile(fileList, true, "a\\directory\\valid-file5.azsli");
        ExpectHasIncludeFile(fileList, true, "a\\dire-ctory\\valid-file6.azsli");
        ExpectHasIncludeFile(fileList, false, "a\\dire-ctory\\invalid-file7.azsli");
        ExpectHasIncludeFile(fileList, true, "C:\\Absolute\\Path\\To\\File.azsi");
        ExpectHasIncludeFile(fileList, true, "..\\Relative\\Path\\To\\File.azsi");
    }

    TEST_F(ShaderBuilderUtilityTests, IncludedFilesParserPreservesSpacesInDependencyPaths)
    {
        AZ::ShaderBuilder::ShaderBuilderUtility::IncludedFilesParser parser;
        const AZStd::vector<AZStd::string> files = parser.ParseStringAndGetIncludedFiles(R"(#include "folder with spaces/file.azsli"
#include <another folder/file.azsli>
)");
        ASSERT_EQ(files.size(), 2);
        ExpectHasIncludeFile(files, true, "folder with spaces/file.azsli");
        ExpectHasIncludeFile(files, true, "another folder/file.azsli");
    }

    TEST_F(ShaderBuilderUtilityTests, IncludedFilesParserTracksHeaderQueriesAndDigraphs)
    {
        AZ::ShaderBuilder::ShaderBuilderUtility::IncludedFilesParser parser;
        const auto files = parser.ParseStringAndGetIncludedFiles(R"(
#if __has_include("optional header.azsli") && __has_include(<another/header.azsli>)
#endif
%:include"digraph.azsli"
#include"adjacent.azsli"
)");
        ASSERT_EQ(files.size(), 4);
        ExpectHasIncludeFile(files, true, "optional header.azsli");
        ExpectHasIncludeFile(files, true, "another/header.azsli");
        ExpectHasIncludeFile(files, true, "digraph.azsli");
        ExpectHasIncludeFile(files, true, "adjacent.azsli");
    }

    TEST_F(ShaderBuilderUtilityTests, IncludedFilesParserHandlesLargeFilesAndOperatorBoundaries)
    {
        AZ::ShaderBuilder::ShaderBuilderUtility::IncludedFilesParser parser;
        AZStd::string haystack(256 * 1024, ' ');
        for (size_t i = 0; i < 8192; ++i)
        {
            haystack += "static float shader_value = 0;\n";
        }
        EXPECT_TRUE(parser.ParseStringAndGetIncludedFiles(haystack).empty());
        haystack += R"(
name__has_include("not-a-header.azsli")
#include "first.azsli"
#if __has_include("optional.azsli")
#endif
%:include "last.azsli"
)";
        const auto files = parser.ParseStringAndGetIncludedFiles(haystack);
        ASSERT_EQ(files.size(), 3);
        EXPECT_EQ(files[0], "first.azsli");
        EXPECT_EQ(files[1], "optional.azsli");
        EXPECT_EQ(files[2], "last.azsli");
    }

    TEST_F(ShaderBuilderUtilityTests, IncludedFilesParserHandlesCommentsContinuationsAndInactiveBranches)
    {
        AZ::ShaderBuilder::ShaderBuilderUtility::IncludedFilesParser parser;
        const auto files = parser.ParseStringAndGetIncludedFiles(
            "\xef\xbb\xbf#inc\\\r\nlude/**/\"spliced/hea\\\nder.azsli\"\r\n"
            "#include /* gap */ <angled.azsli>\n"
            "#if __has_\\\ninclude/**/(/* gap */\"query.azsli\")\n#endif\n"
            "%\\\n:define MATERIAL_TYPE_AZSLI_FILE_PATH \\\n\"material.azsli\"\n"
            "// #include \"comment.azsli\"\n"
            "#if 0\n#include \"inactive.azsli\"\n#endif\n");
        ASSERT_EQ(files.size(), 6);
        ExpectHasIncludeFile(files, true, "spliced/header.azsli");
        ExpectHasIncludeFile(files, true, "angled.azsli");
        ExpectHasIncludeFile(files, true, "query.azsli");
        ExpectHasIncludeFile(files, true, "material.azsli");
        ExpectHasIncludeFile(files, true, "comment.azsli");
        ExpectHasIncludeFile(files, true, "inactive.azsli");
    }

    TEST_F(ShaderBuilderUtilityTests, IncludedFilesParserRequiresCompleteNamesAndHeaderDelimiters)
    {
        AZ::ShaderBuilder::ShaderBuilderUtility::IncludedFilesParser parser;
        EXPECT_TRUE(parser.ParseStringAndGetIncludedFiles(
            "#include_next \"suffix.azsli\"\n"
            "__has_include_next(\"suffix.azsli\")\n"
            "#include <mismatched.azsli\"\n"
            "#include \"mismatched.azsli>\n"
            "#include \"\"\n").empty());
        const AZStd::string source = "#include/* gap */\"complete.azsli\"";
        for (size_t length = 0; length < source.size(); ++length)
        {
            EXPECT_TRUE(parser.ParseStringAndGetIncludedFiles(AZStd::string_view(source.data(), length)).empty());
        }
        const auto files = parser.ParseStringAndGetIncludedFiles(source);
        ASSERT_EQ(files.size(), 1);
        EXPECT_EQ(files[0], "complete.azsli");
    }

    TEST_F(ShaderBuilderUtilityTests, IncludedFilesParser_HandleMaterialPipelineMacro)
    {
        // This is a temporary solution to support material pipeline where the include path is specified in a #define and
        // later included like #include MATERIAL_TYPE_AZSLI_FILE_PATH

        AZStd::string haystack(
            R"(
                #define MATERIAL_TYPE_AZSLI_FILE_PATH "D:\o3de\Gems\Atom\TestData\TestData\Materials\Types\MaterialPipelineTest_Animated.azsli" 
                #include "D:\o3de\Gems\Atom\Feature\Common\Assets\Materials\Pipelines\LowEndPipeline\ForwardPass_BaseLighting.azsli" 
            )"
        );

        AZ::ShaderBuilder::ShaderBuilderUtility::IncludedFilesParser includedFilesParser;
        auto fileList = includedFilesParser.ParseStringAndGetIncludedFiles(haystack);
        EXPECT_EQ(fileList.size(), 2);

        ExpectHasIncludeFile(fileList, true, R"(D:\o3de\Gems\Atom\TestData\TestData\Materials\Types\MaterialPipelineTest_Animated.azsli)");
        ExpectHasIncludeFile(fileList, true, R"(D:\o3de\Gems\Atom\Feature\Common\Assets\Materials\Pipelines\LowEndPipeline\ForwardPass_BaseLighting.azsli)");
    }

} //namespace UnitTest

//AZ_UNIT_TEST_HOOK(DEFAULT_UNIT_TEST_ENV);
