/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Outcome/Outcome.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/parallel/mutex.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/string/string_view.h>

#include <slang.h>
#include <slang-com-ptr.h>

namespace AZ::ShaderBuilder
{
    //! Owns the process-wide Slang global session and provides synchronized helpers for
    //! creating compile sessions and bridging Slang diagnostics into AZ tracing.
        //!
        //! The Slang compiler runs in-process (slang-compiler.dll via 3rdParty::Slang) — there is
        //! no compiler subprocess. The global session is expensive (it loads Slang's core module),
        //! so exactly one is created lazily per builder process and reused by every job.
        //!
        //! Thread safety: until the compiler's thread-safety guarantees are characterized
        //! (feasibility gate 6), all access to the Slang API must be serialized through
        //! AcquireCompilerLock(). This mirrors how the MCPP preprocessor is serialized in
        //! CommonFiles/Preprocessor.cpp. Asset Processor parallelism is process-based, so the
        //! practical cost is low.
    class SlangCompilerService final
    {
    public:
        static constexpr char LogName[] = "SlangCompilerService";

        //! Returns the process-wide service instance, creating it (and the Slang global
        //! session) on first use.
        static SlangCompilerService& Get();

        SlangCompilerService(const SlangCompilerService&) = delete;
        SlangCompilerService& operator=(const SlangCompilerService&) = delete;

        //! Describes one compile session. A session corresponds to one frontend run —
        //! one (RHI API, supervariant) combination — and is cheap relative to the global session.
        struct SessionDescriptor
        {
            //! Target IR to generate (SLANG_DXIL, SLANG_SPIRV, ...).
            SlangCompileTarget m_target = SLANG_TARGET_UNKNOWN;

            //! Slang profile name for the target, e.g. "sm_6_2" or "spirv_1_5".
            AZStd::string m_profile;

            //! Preprocessor macros visible to every module compiled in the session.
            AZStd::vector<AZStd::pair<AZStd::string, AZStd::string>> m_preprocessorMacros;

            //! Search roots for module/include resolution (ShaderLib directories etc.).
            AZStd::vector<AZStd::string> m_searchPaths;

            //! Extra compiler options applied to the session, in addition to the ones
            //! derived from the fields above. Names/values follow slang::CompilerOptionName.
            AZStd::vector<slang::CompilerOptionEntry> m_extraOptions;

            //! Row-major matrix layout (Atom builds shaders row-major everywhere: --Zpr/-Zpr).
            bool m_matrixLayoutRow = true;

            //! Emit debug information.
            bool m_generateDebugInfo = false;
        };

        //! Creates a compile session from @descriptor.
        //! The caller must hold the compiler lock for the whole lifetime of any use of the
        //! returned session (see AcquireCompilerLock()).
        AZ::Outcome<Slang::ComPtr<slang::ISession>, AZStd::string> CreateSession(const SessionDescriptor& descriptor);

        //! Convenience for the common "compile one entry point of one source string" flow:
        //! load module -> find entry point -> compose -> link -> get entry point code.
        //! Intended for tests and small probes; production paths drive the session API directly.
        //! Acquires the compiler lock internally.
        struct EntryPointCompileRequest
        {
            AZStd::string_view m_sourceCode;
            AZStd::string_view m_moduleName;      //!< Logical module name, also used in diagnostics.
            AZStd::string_view m_entryPointName;
            SlangStage m_stage = SLANG_STAGE_NONE;
        };
        AZ::Outcome<AZStd::vector<uint8_t>, AZStd::string> CompileEntryPointFromSource(
            const SessionDescriptor& sessionDescriptor,
            const EntryPointCompileRequest& request);

        //! Identity of the compiler actually loaded in this process (not the headers we built
        //! against): IGlobalSession::getBuildTagString(). Feeds builder fingerprints and
        //! cached-product manifests so a compiler upgrade invalidates stale products.
        AZStd::string_view GetCompilerBuildTag() const;

        //! Serializes access to the Slang compiler. Hold the returned lock across every
        //! sequence of Slang API calls (session creation through bytecode retrieval).
        [[nodiscard]] AZStd::unique_lock<AZStd::recursive_mutex> AcquireCompilerLock();

        //! Routes a Slang diagnostics blob into AZ tracing. @contextPath names the shader
        //! (or module) being compiled so crashes and errors are easy to triage.
        //! Diagnostics are reported as errors when @asError is true, as warnings otherwise.
        static void ReportDiagnostics(
            AZStd::string_view contextPath,
            slang::IBlob* diagnostics,
            bool asError);

    private:
        SlangCompilerService();
        ~SlangCompilerService();

        Slang::ComPtr<slang::IGlobalSession> m_globalSession;
        AZStd::string m_buildTag;
        AZStd::recursive_mutex m_mutex;
    };
} // namespace AZ::ShaderBuilder
