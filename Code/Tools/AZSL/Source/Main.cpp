/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AZSL/Compiler.h>

#include "External/CLI11.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    using AZ::ShaderCompiler::ArtifactType;
    using AZ::ShaderCompiler::CompilationArtifact;

    std::string GetFileNameWithoutExtension(std::string_view fileName)
    {
        std::filesystem::path path{fileName};
        path.replace_extension();
        return path.generic_string();
    }

    std::string_view GetArtifactSuffix(ArtifactType type)
    {
        switch (type)
        {
        case ArtifactType::InputAssembler:
            return "ia";
        case ArtifactType::OutputMerger:
            return "om";
        case ArtifactType::ShaderResourceGroup:
            return "srg";
        case ArtifactType::Options:
            return "options";
        case ArtifactType::BindingDependencies:
            return "bindingdep";
        default:
            return {};
        }
    }

    void WriteFile(std::string_view fileName, std::string_view content)
    {
        std::ofstream output{std::string{fileName}};
        if (!output.good())
        {
            throw std::runtime_error("output file '" + std::string{fileName} + "' could not be opened");
        }
        output << content;
    }

    void WritePrimaryArtifact(
        const CompilationArtifact& artifact,
        std::string_view outputFile,
        bool honorOutputFile)
    {
        if (honorOutputFile && !outputFile.empty())
        {
            std::ofstream output{std::string{outputFile}};
            if (!output.good())
            {
                throw std::runtime_error("output file could not be opened");
            }
            output << artifact.m_content;
        }
        else
        {
            std::cout << artifact.m_content;
        }
    }
} // namespace

int main(int argc, const char* argv[])
{
    using namespace AZ::ShaderCompiler;

    CLI::App cli{"Amazon Shader Language Compiler"};

    bool printVersion = false;
    cli.add_flag("--version", printVersion, "Prints version information");

    std::string inputFile;
    cli.add_option("FILE", inputFile, "Input file (pass - to read from stdin).");

    std::string outputFile;
    cli.add_option("-o", outputFile, "Output file (writes to stdout if omitted).");

    bool uniqueIdx = false;
    cli.add_flag("--unique-idx", uniqueIdx, "Use unique indices for all registers. e.g. b0, t0, u0, s0 becomes b0, t1, u2, s3. Use on platforms that don't differentiate registers by resource type.");

    bool cbBody = false;
    cli.add_flag("--cb-body", cbBody, "Emit ConstantBuffer body rather than using <T>.");

    bool rootSig = false;
    cli.add_flag("--root-sig", rootSig, "Emit RootSignature for parameter binding in the shader. --namespace must also be used to select a specific API.");

    int rootConst = 0;
    auto rootConstOption = cli.add_option("--root-const", rootConst, "Maximum size in bytes of the root constants buffer.");

    bool padRootConst = false;
    cli.add_flag("--pad-root-const", padRootConst, "Automatically append padding data to the root constant CB to keep it aligned to a 16-byte boundary.");

    bool columnMajor = false;
    cli.add_flag("--Zpc", columnMajor, "Pack matrices in column-major order (default). Cannot be specified together with -Zpr.");

    bool rowMajor = false;
    cli.add_flag("--Zpr", rowMajor, "Pack matrices in row-major order. Cannot be specified together with -Zpc.");

    bool packDx12 = false;
    cli.add_flag("--pack-dx12", packDx12, "Pack buffers using strict DX12 packing rules. If not specified AZSLC will use relaxed packing rules.");

    bool packVulkan = false;
    cli.add_flag("--pack-vulkan", packVulkan, "Pack buffers using strict Vulkan packing rules (Vector-relaxed std140 for uniforms and std430 for storage buffers).");

    bool packOpenGL = false;
    cli.add_flag("--pack-opengl", packOpenGL, "Pack buffers using strict OpenGL packing rules (Vector-strict std140 for uniforms and std430 for storage buffers).");

    std::vector<std::string> attributeNamespaces;
    cli.add_option(
        "--namespace",
        attributeNamespaces,
        "Activate an attribute namespace. May be used multiple times to activate multiple namespaces. "
        "Activating a namespace may also activate corresponding API-specific features, like dx for DirectX 12, vk for Vulkan, and mt for Metal.");

    bool inputAssembler = false;
    cli.add_flag("--ia", inputAssembler, "Output a list of vs entries with their Input Assembler layouts *and* a list of CS entries and their numthreads.");

    bool outputMerger = false;
    cli.add_flag("--om", outputMerger, "Output the Output Merger layout instead of the shader code.");

    bool shaderResourceGroup = false;
    cli.add_flag("--srg", shaderResourceGroup, "Output the Shader Resource Group layout instead of the shader code.");

    bool shaderOptions = false;
    cli.add_flag("--options", shaderOptions, "Output the list of available shader options for this shader.");

    bool dumpSymbols = false;
    cli.add_flag("--dumpsym", dumpSymbols, "Dump symbols.");

    bool syntax = false;
    cli.add_flag("--syntax", syntax, "Check syntax (no output means no complaints).");

    bool semantic = false;
    cli.add_flag("--semantic", semantic, "Check semantics (no output means no complaints).");

    bool syntaxTree = false;
    cli.add_flag("--ast", syntaxTree, "Output the abstract syntax tree.");

    bool bindingDependencies = false;
    cli.add_flag("--bindingdep", bindingDependencies, "Output binding dependencies (what entry points access what external resources).");

    std::string visitName;
    cli.add_option("--visitsym", visitName, "Output the locations of all relationships of the supplied symbol name.");

    bool full = false;
    cli.add_flag("--full", full, "Output the shader code, IA layout, OM layout, SRG layout, the list of available shader options, and the binding dependencies.");

    bool stripUnusedSrgs = false;
    cli.add_flag("--strip-unused-srgs", stripUnusedSrgs, "Strips unused SRGs.");

    bool noMultisampling = false;
    cli.add_flag(
        "--no-ms",
        noMultisampling,
        "Transforms usage of Texture2DMS/Texture2DMSArray and related functions and semantics into plain Texture2D/Texture2DArray "
        "equivalents. This is useful for allowing shader authors to easily write AZSL code that can be compiled into alternatives to work with both a "
        "multisample render pipeline and a non-MS render pipeline.");

    bool noAlignmentValidation = false;
    cli.add_flag(
        "--no-alignment-validation",
        noAlignmentValidation,
        "Skips checking for potential alignment issues related to differences between dxil and spirv."
        "By default, potential alignment discrepancies will fail compilation.");

    bool visitDirectReferences = false;
    cli.add_flag("-d", visitDirectReferences, "(Option of --visitsym) Visit direct references.");

    bool visitOverloadSet = false;
    cli.add_flag("-v", visitOverloadSet, "(Option of --visitsym) Visit overload-set.");

    bool visitFamily = false;
    cli.add_flag("-f", visitFamily, "(Option of --visitsym) Visit family.");

    bool visitRecursively = false;
    cli.add_flag("-r", visitRecursively, "(Option of --visitsym) Visit recursively.");

    bool listPredefined = false;
    cli.add_flag("--listpredefined", listPredefined, "Output a list of all predefined types in AZSLang.");

    int maxSpaces = std::numeric_limits<int>::max();
    auto maxSpacesOption = cli.add_option("--max-spaces", maxSpaces, "Will choose register spaces that do not extend past this limit.");

    bool useSpecializationConstants = false;
    cli.add_flag("--sc-options", useSpecializationConstants, "Use specialization constants for shader options.");

    bool noSubpassInput = false;
    cli.add_flag("--no-subpass-input", noSubpassInput, "Transform usage of SubpassInput/SubpassInputMS into Texture2D/Texture2DMS");

    int32_t subpassInputOffset = 0;
    cli.add_option("--subpass-input-offset", subpassInputOffset, "Offset to apply to the subpass index attribute.");

    std::array<bool, 8> warningOptions{};
    cli.add_flag("--W0", warningOptions[0], "Suppresses all warnings.");
    cli.add_flag("--W1", warningOptions[1], "Activate severe warnings (default).");
    cli.add_flag("--W2", warningOptions[2], "Activate warnings that may be significant.");
    cli.add_flag("--W3", warningOptions[3], "Activate low-confidence diagnostic warnings.");
    cli.add_flag("--Wx", warningOptions[4], "Treat activated warnings as errors.");
    cli.add_flag("--Wx1", warningOptions[5], "Treat level-1 warnings as errors.");
    cli.add_flag("--Wx2", warningOptions[6], "Treat level-2 and below warnings as errors.");
    cli.add_flag("--Wx3", warningOptions[7], "Treat level-3 and below warnings as errors.");

    std::string minDescriptors;
    cli.add_option(
        "--min-descriptors",
        minDescriptors,
        "Comma-separated list of limits corresponding to "
        "<set,space,sampler,texture,buffer> descriptors. Emits a warning if a count overshoots a limit. Use -1 to specify \"no limit\".");

    bool verbose = false;
    cli.add_flag("--verbose", verbose);

    CLI11_PARSE(cli, argc, argv);

    try
    {
        if (printVersion)
        {
            std::cout << Compiler::GetVersionString() << std::endl;
            return 0;
        }
        if (listPredefined)
        {
            std::cout << Compiler::GetPredefinedVocabulary();
            return 0;
        }

        const bool useStdin = inputFile == "-";
        std::ifstream inputFileStream;
        std::istream* input = &std::cin;
        if (!useStdin)
        {
            inputFileStream.open(inputFile);
            input = &inputFileStream;
        }
        if (!input->good())
        {
            throw std::runtime_error("input file could not be opened");
        }
        const std::string source{
            std::istreambuf_iterator<char>{*input},
            std::istreambuf_iterator<char>{}};

        CompileOptions options;
        options.m_useUniqueIndices = uniqueIdx;
        options.m_emitConstantBufferBody = cbBody;
        options.m_emitRootSignature = rootSig;
        options.m_padRootConstantBuffer = padRootConst;
        options.m_attributeNamespaces = attributeNamespaces;
        options.m_stripUnusedShaderResourceGroups = stripUnusedSrgs;
        options.m_removeMultisampling = noMultisampling;
        options.m_skipAlignmentValidation = noAlignmentValidation;
        options.m_useSpecializationConstants = useSpecializationConstants;
        options.m_removeSubpassInputs = noSubpassInput;
        options.m_subpassInputOffset = subpassInputOffset;
        options.m_verbose = verbose;
        options.m_symbolToVisit = visitName;
        options.m_visitDirectReferences = visitDirectReferences;
        options.m_visitOverloadSet = visitOverloadSet;
        options.m_visitFamily = visitFamily;
        options.m_visitRecursively = visitRecursively;

        if (*rootConstOption)
        {
            options.m_rootConstantsMaxSize = rootConst;
        }
        if (*maxSpacesOption)
        {
            options.m_maxSpaces = maxSpaces;
        }

        if (columnMajor && rowMajor)
        {
            throw std::runtime_error("Cannot specify --Zpr and --Zpc together, use --help to get usage information");
        }
        if (rowMajor)
        {
            options.m_matrixOrder = MatrixOrder::RowMajor;
        }
        else if (columnMajor)
        {
            options.m_matrixOrder = MatrixOrder::ColumnMajor;
        }

        if (packDx12)
        {
            options.m_packing = PackingMode::DirectX;
        }
        if (packVulkan)
        {
            options.m_packing = PackingMode::Vulkan;
        }
        if (packOpenGL)
        {
            options.m_packing = PackingMode::OpenGL;
        }

        if (!minDescriptors.empty())
        {
            DescriptorLimits descriptorLimits;
            const int parsedDescriptorCount =
#if defined(_MSC_VER)
                sscanf_s(
#else
                sscanf(
#endif
                    minDescriptors.c_str(),
                    "%d,%d,%d,%d,%d",
                    &descriptorLimits.m_descriptorsTotal,
                    &descriptorLimits.m_spaces,
                    &descriptorLimits.m_samplers,
                    &descriptorLimits.m_textures,
                    &descriptorLimits.m_buffers);
            if (parsedDescriptorCount != 5)
            {
                throw std::runtime_error("Invalid --min-descriptors value, expected total,spaces,samplers,textures,buffers");
            }
            options.m_minDescriptorLimits = descriptorLimits;
        }

        for (size_t warningIndex = 0; warningIndex < 4; ++warningIndex)
        {
            if (warningOptions[warningIndex])
            {
                options.m_warningLevel = static_cast<WarningLevel>(warningIndex);
            }
        }
        for (size_t warningIndex = 4; warningIndex < warningOptions.size(); ++warningIndex)
        {
            if (warningOptions[warningIndex])
            {
                options.m_warningsAsErrors = static_cast<WarningsAsErrors>(warningIndex - 3);
            }
        }

        options.m_artifacts.clear();
        if (syntaxTree)
        {
            options.m_phase = CompilationPhase::Syntax;
            options.m_artifacts.push_back(ArtifactType::SyntaxTree);
        }
        else if (syntax)
        {
            options.m_phase = CompilationPhase::Syntax;
        }
        else if (dumpSymbols)
        {
            options.m_phase = CompilationPhase::Semantic;
            options.m_artifacts.push_back(ArtifactType::Symbols);
        }
        else if (!visitName.empty())
        {
            options.m_phase = CompilationPhase::Semantic;
            options.m_artifacts.push_back(ArtifactType::SymbolRelationships);
        }
        else if (semantic)
        {
            options.m_phase = CompilationPhase::Semantic;
        }
        else
        {
            options.m_phase = CompilationPhase::Emit;
            if (full)
            {
                options.m_artifacts = {
                    ArtifactType::Shader,
                    ArtifactType::InputAssembler,
                    ArtifactType::OutputMerger,
                    ArtifactType::ShaderResourceGroup,
                    ArtifactType::Options,
                    ArtifactType::BindingDependencies,
                };
            }
            else if (inputAssembler)
            {
                options.m_artifacts.push_back(ArtifactType::InputAssembler);
            }
            else if (outputMerger)
            {
                options.m_artifacts.push_back(ArtifactType::OutputMerger);
            }
            else if (shaderResourceGroup)
            {
                options.m_artifacts.push_back(ArtifactType::ShaderResourceGroup);
            }
            else if (shaderOptions)
            {
                options.m_artifacts.push_back(ArtifactType::Options);
            }
            else if (bindingDependencies)
            {
                options.m_artifacts.push_back(ArtifactType::BindingDependencies);
            }
            else
            {
                options.m_artifacts.push_back(ArtifactType::Shader);
            }
        }

        const std::string logicalSourcePath = useStdin ? "stdin" : inputFile;
        const CompileResult result = Compiler{}.Compile({source, logicalSourcePath, std::move(options)});
        std::cout << result.m_informationalOutput;
        std::cerr << result.m_legacyDiagnostics;
        if (!result.m_succeeded)
        {
            return 1;
        }

        if (full)
        {
            const std::string outputBase = GetFileNameWithoutExtension(outputFile.empty() ? inputFile : outputFile);
            for (const CompilationArtifact& artifact : result.m_artifacts)
            {
                if (artifact.m_type == ArtifactType::Shader)
                {
                    WritePrimaryArtifact(artifact, outputFile, true);
                }
                else
                {
                    const std::string_view suffix = GetArtifactSuffix(artifact.m_type);
                    if (!suffix.empty())
                    {
                        WriteFile(outputBase + "." + std::string{suffix} + ".json", artifact.m_content);
                    }
                }
            }
        }
        else if (!result.m_artifacts.empty())
        {
            const ArtifactType type = result.m_artifacts.front().m_type;
            const bool honorOutputFile = type != ArtifactType::SyntaxTree &&
                type != ArtifactType::Symbols &&
                type != ArtifactType::SymbolRelationships;
            WritePrimaryArtifact(result.m_artifacts.front(), outputFile, honorOutputFile);
        }
    }
    catch (const std::exception& exception)
    {
        std::cerr << exception.what() << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cerr << "Unknown exception" << std::endl;
        return 1;
    }

    return 0;
}
