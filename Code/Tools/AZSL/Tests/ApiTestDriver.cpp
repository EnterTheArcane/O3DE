/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AZSL/Compiler.h>

#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

namespace
{
    bool ConfigureMode(std::string_view mode, AZ::ShaderCompiler::CompileOptions& options)
    {
        using namespace AZ::ShaderCompiler;

        options.m_artifacts.clear();
        if (mode == "shader")
        {
            options.m_phase = CompilationPhase::Emit;
            options.m_artifacts.push_back(ArtifactType::Shader);
        }
        else if (mode == "ia")
        {
            options.m_phase = CompilationPhase::Emit;
            options.m_artifacts.push_back(ArtifactType::InputAssembler);
        }
        else if (mode == "om")
        {
            options.m_phase = CompilationPhase::Emit;
            options.m_artifacts.push_back(ArtifactType::OutputMerger);
        }
        else if (mode == "srg")
        {
            options.m_phase = CompilationPhase::Emit;
            options.m_artifacts.push_back(ArtifactType::ShaderResourceGroup);
        }
        else if (mode == "options")
        {
            options.m_phase = CompilationPhase::Emit;
            options.m_artifacts.push_back(ArtifactType::Options);
        }
        else if (mode == "bindingdep")
        {
            options.m_phase = CompilationPhase::Emit;
            options.m_artifacts.push_back(ArtifactType::BindingDependencies);
        }
        else if (mode == "ast")
        {
            options.m_phase = CompilationPhase::Syntax;
            options.m_artifacts.push_back(ArtifactType::SyntaxTree);
        }
        else if (mode == "symbols")
        {
            options.m_phase = CompilationPhase::Semantic;
            options.m_artifacts.push_back(ArtifactType::Symbols);
        }
        else if (mode == "syntax")
        {
            options.m_phase = CompilationPhase::Syntax;
        }
        else if (mode == "semantic")
        {
            options.m_phase = CompilationPhase::Semantic;
        }
        else
        {
            return false;
        }
        return true;
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: AZSL.ApiTestDriver <mode>\n";
        return 2;
    }

    AZ::ShaderCompiler::CompileOptions options;
    if (!ConfigureMode(argv[1], options))
    {
        std::cerr << "unknown mode: " << argv[1] << '\n';
        return 2;
    }

    const std::string source{
        std::istreambuf_iterator<char>{std::cin},
        std::istreambuf_iterator<char>{}};
    const AZ::ShaderCompiler::CompileResult result =
        AZ::ShaderCompiler::Compiler{}.Compile({source, "stdin", std::move(options)});

    std::cout << result.m_informationalOutput;
    std::cerr << result.m_legacyDiagnostics;
    if (!result.m_succeeded)
    {
        return 1;
    }

    for (const AZ::ShaderCompiler::CompilationArtifact& artifact : result.m_artifacts)
    {
        std::cout << artifact.m_content;
    }
    return 0;
}
