/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include "SourceManager.h"

#include <functional>
#include <ostream>
#include <stdexcept>

namespace AZ::ShaderCompiler
{
    enum class PreprocessingKind : uint8_t
    {
        None = 0,
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
    };

    struct PreprocessingToken
    {
        TextSpan m_spelling;
        SourceLocationId m_location = 0;
        uint32_t m_suppression = 0; // Expansion-internal set ID; cleared in the published result.
        PreprocessingKind m_kind = PreprocessingKind::End;
        uint8_t m_flags = 0;
    };

    bool IsPreprocessingPunctuator(std::string_view spelling);

    struct MacroOperation
    {
        bool m_undefine = false;
        std::string m_definition;
    };

    struct PreprocessRequest
    {
        std::string m_sourcePath;
        BufferId m_source = 0; // Optional registered in-memory root, including stdin.
        std::vector<MacroOperation> m_macros;
        std::vector<std::string> m_includeDirectories;
        std::vector<std::string> m_forcedIncludes;
        std::function<void(std::string_view)> m_warning;
        bool m_preprocessed = false;
        bool m_preserveComments = true;
        size_t m_includeLimit = 256;
        size_t m_expansionLimit = 1000000;
        size_t m_tokenLimit = 4000000;
    };

    struct DependencyEvent
    {
        std::string m_resolvedPath;
    };

    struct PreprocessResult
    {
        AZStd::span<const PreprocessingToken> m_tokens;
        AZStd::span<const DependencyEvent> m_dependencies;
        AZStd::span<const std::string> m_diagnostics;
    };

    class PreprocessingError : public SourceError
    {
    public:
        PreprocessingError(SourceLocation location, const std::string& message);
    };

    class CompilationUnit
    {
    public:
        SourceManager m_sources;

        CompilationUnit() = default;

        CompilationUnit(const CompilationUnit&) = delete;

        CompilationUnit& operator=(const CompilationUnit&) = delete;

        PreprocessResult Preprocess(const PreprocessRequest& request);

        void WritePreprocessed(std::ostream& output) const;

        AZStd::span<const PreprocessingToken> Tokens() const
        {
            return m_tokens;
        }

        AZStd::span<const DependencyEvent> Dependencies() const
        {
            return m_dependencies;
        }

        AZStd::span<const std::string> Diagnostics() const
        {
            return m_diagnostics;
        }

    private:
        friend class Preprocessor;

        std::vector<PreprocessingToken> m_tokens;
        std::vector<DependencyEvent> m_dependencies;
        std::vector<std::string> m_diagnostics;
        bool m_started = false;
    };
} // namespace AZ::ShaderCompiler
