/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace AZ::ShaderCompiler
{
    //! This API is engine-internal and expected to evolve as AZSL is integrated with O3DE.
    enum class CompilationPhase
    {
        Syntax,
        Semantic,
        Emit,
    };

    enum class ArtifactType
    {
        Shader,
        InputAssembler,
        OutputMerger,
        ShaderResourceGroup,
        Options,
        BindingDependencies,
        Symbols,
        SyntaxTree,
        SymbolRelationships,
    };

    enum class MatrixOrder
    {
        Default,
        ColumnMajor,
        RowMajor,
    };

    enum class PackingMode
    {
        Default,
        DirectX,
        Vulkan,
        OpenGL,
    };

    enum class WarningLevel : uint8_t
    {
        Suppress,
        Level1,
        Level2,
        Level3,
    };

    enum class WarningsAsErrors : uint8_t
    {
        Disabled,
        EnabledWarnings,
        Level1,
        Level2,
        Level3,
    };

    enum class DiagnosticSeverity
    {
        Information,
        Warning,
        Error,
    };

    struct DescriptorLimits
    {
        int m_descriptorsTotal = -1;
        int m_spaces = -1;
        int m_samplers = -1;
        int m_textures = -1;
        int m_buffers = -1;
    };

    struct CompileOptions
    {
        CompilationPhase m_phase = CompilationPhase::Emit;
        std::vector<ArtifactType> m_artifacts{ArtifactType::Shader};

        bool m_useUniqueIndices = false;
        bool m_emitConstantBufferBody = false;
        bool m_emitRootSignature = false;
        std::optional<int> m_rootConstantsMaxSize;
        bool m_padRootConstantBuffer = false;
        MatrixOrder m_matrixOrder = MatrixOrder::Default;
        PackingMode m_packing = PackingMode::Default;
        std::vector<std::string> m_attributeNamespaces;
        bool m_stripUnusedShaderResourceGroups = false;
        bool m_removeMultisampling = false;
        bool m_skipAlignmentValidation = false;
        std::optional<int> m_maxSpaces;
        bool m_useSpecializationConstants = false;
        bool m_removeSubpassInputs = false;
        int32_t m_subpassInputOffset = 0;
        std::optional<DescriptorLimits> m_minDescriptorLimits;

        WarningLevel m_warningLevel = WarningLevel::Level1;
        WarningsAsErrors m_warningsAsErrors = WarningsAsErrors::Disabled;
        bool m_verbose = false;

        std::string m_symbolToVisit;
        bool m_visitDirectReferences = false;
        bool m_visitOverloadSet = false;
        bool m_visitFamily = false;
        bool m_visitRecursively = false;
    };

    struct CompileRequest
    {
        std::string_view m_source;
        std::string_view m_sourcePath;
        CompileOptions m_options;
    };

    struct CompilationArtifact
    {
        ArtifactType m_type = ArtifactType::Shader;
        std::string m_content;
    };

    struct Diagnostic
    {
        DiagnosticSeverity m_severity = DiagnosticSeverity::Error;
        std::optional<uint32_t> m_code;
        std::string m_category;
        std::string m_message;
        std::string m_sourcePath;
        std::optional<size_t> m_line;
        std::optional<size_t> m_column;
        std::string m_renderedText;
    };

    struct CompileResult
    {
        bool m_succeeded = false;
        std::vector<CompilationArtifact> m_artifacts;
        std::string m_informationalOutput;
        std::vector<Diagnostic> m_diagnostics;
        std::string m_legacyDiagnostics;
    };

    class Compiler
    {
    public:
        [[nodiscard]] CompileResult Compile(const CompileRequest& request) const;

        [[nodiscard]] static std::string_view GetVersion();
        [[nodiscard]] static std::string GetVersionString();
        [[nodiscard]] static std::string GetPredefinedVocabulary();
        [[nodiscard]] static std::vector<std::string> GetRegisteredPlatformEmitters();
    };
} // namespace AZ::ShaderCompiler
