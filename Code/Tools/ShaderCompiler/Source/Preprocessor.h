/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include "SourceManager.h"

#include <AzCore/std/containers/span.h>

#include <ostream>
#include <stdexcept>
#include <string_view>

namespace AZ::ShaderCompiler
{
    enum class PreprocessingKind : uint8_t
    {
        Identifier,
        Number,
        String,
        Character,
        Punctuation,
        Space,
        Newline,
        Comment,
        Directive,
        HeaderName,
        Placemark,
        End,
    };

    enum PreprocessingFlags : uint8_t
    {
        LeadingSpace = 1,
        StartOfLine = 2,
    };

    struct PreprocessingToken
    {
        std::string spelling;
        SourceLocationId location = 0;
        uint32_t suppression = 0; // Expansion-internal set ID; cleared in the published result.
        PreprocessingKind kind = PreprocessingKind::End;
        uint8_t flags = 0;
    };

    bool IsPreprocessingPunctuator(std::string_view spelling);
    std::string_view CanonicalPunctuator(std::string_view spelling);

    struct MacroOperation
    {
        bool undefine = false;
        std::string definition;
    };

    struct PreprocessRequest
    {
        std::string sourcePath;
        BufferId source = 0; // Optional registered in-memory root, including stdin.
        std::vector<MacroOperation> macros;
        std::vector<std::string> includeDirectories;
        std::vector<std::string> forcedIncludes;
        bool preprocessed = false;
        bool preserveComments = true;
        size_t includeLimit = 256;
        size_t expansionLimit = 1000000;
        size_t tokenLimit = 4000000;
    };

    struct DependencyEvent
    {
        std::string spelling;
        std::string resolvedPath;
        SourceLocationId location;
    };

    struct PreprocessResult
    {
        AZStd::span<const PreprocessingToken> tokens;
        AZStd::span<const DependencyEvent> dependencies;
        AZStd::span<const std::string> diagnostics;
    };

    class PreprocessingError : public std::runtime_error
    {
    public:
        PreprocessingError(SourceLocation location, const std::string& message);
    };

    class CompilationUnit
    {
    public:
        SourceManager sources;

        CompilationUnit() = default;
        CompilationUnit(const CompilationUnit&) = delete;
        CompilationUnit& operator=(const CompilationUnit&) = delete;

        PreprocessResult Preprocess(const PreprocessRequest& request);
        void WritePreprocessed(std::ostream& output) const;

        AZStd::span<const PreprocessingToken> Tokens() const
        {
            return m_tokens;
        }

    private:
        friend class Preprocessor;

        std::vector<PreprocessingToken> m_tokens;
        std::vector<DependencyEvent> m_dependencies;
        std::vector<std::string> m_diagnostics;
        bool m_started = false;
    };
} // namespace AZ::ShaderCompiler
