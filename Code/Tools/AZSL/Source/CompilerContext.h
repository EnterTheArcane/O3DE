/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "DiagnosticStream.h"
#include "PreprocessorLineDirectiveFinder.h"

#include <sstream>
#include <string_view>

namespace AZ::ShaderCompiler
{
    struct CompilationContext
    {
        explicit CompilationContext(std::string_view sourcePath)
            : m_verboseStream(m_informationalOutput)
            , m_warningStream(m_warningOutput)
        {
            m_lineFinder.m_physicalSourceFileName = sourcePath.empty() ? "<memory>" : std::string{sourcePath};
        }

        std::ostringstream m_informationalOutput;
        std::ostringstream m_warningOutput;
        std::ostringstream m_errorOutput;
        DiagnosticStream m_verboseStream;
        DiagnosticStream m_warningStream;
        PreprocessorLineDirectiveFinder m_lineFinder;
    };

    class ScopedCompilationContext
    {
    public:
        explicit ScopedCompilationContext(CompilationContext& context);
        ~ScopedCompilationContext();

        ScopedCompilationContext(const ScopedCompilationContext&) = delete;
        ScopedCompilationContext& operator=(const ScopedCompilationContext&) = delete;

    private:
        CompilationContext* m_previousContext = nullptr;
    };

    CompilationContext& GetCompilationContext();
    DiagnosticStream& GetVerboseStream();
    DiagnosticStream& GetWarningStream();
    std::ostream& GetInformationalStream();
    PreprocessorLineDirectiveFinder& GetLineDirectiveFinder();
} // namespace AZ::ShaderCompiler
