/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Editor/ShaderCompilerBackend.h>
#include <Slang/SlangCompilerService.h>
#include <Slang/SlangModuleResolver.h>

namespace AZ::ShaderBuilder
{
    //! The Slang shader language behind the language-backend seam: .slang sources compile
    //! in-process (slang-compiler.dll) straight to the RHI's declared target IR — no HLSL
    //! intermediate, no compiler subprocess.
    //!
    //! Current state (plan milestone M7): the frontend pipeline — session from the target
    //! descriptor, prelude injection, module load, entry-point discovery, link — is complete and
    //! exercised by tests through CompileProgram. CompileFrontend and CompileStage report a clear
    //! not-yet-implemented failure until the reflection walker (M8) can fill ShaderReflectionData.
    class SlangBackend final : public IShaderCompilerBackend
    {
    public:
        AZStd::string_view GetName() const override;

        AZStd::span<const AZStd::string_view> GetSourceExtensions() const override;

        bool CanCompileTarget(const RHI::ShaderTargetDescriptor& targetDescriptor) const override;

        void EnumerateSourceDependencies(
            AZStd::string_view shaderSourceFullPath,
            AZStd::span<const AZStd::string> includePaths,
            AZStd::unordered_set<AZStd::string>& sourceDependencies) const override;

        AZ::Outcome<FrontendResult, AZStd::string> CompileFrontend(const FrontendInput& input) override;

        AZ::Outcome<StageResult, AZStd::string> CompileStage(const StageInput& input) override;

        //! One compiled and linked Slang program. Every member must stay alive for as long as any
        //! reflection or code queried from m_linkedProgram is used (reflection layouts are owned
        //! by the linked program). The linked program retains the composed entry points; their
        //! ComPtrs are not stored because AZStd containers cannot hold Slang::ComPtr (its unary
        //! operator& is deleted).
        struct ProgramCompilation
        {
            //! The file system hook the session loads sources through; injects the
            //! force-included prelude imports into every module.
            Slang::ComPtr<ISlangFileSystem> m_fileSystem;

            Slang::ComPtr<slang::ISession> m_session;
            Slang::ComPtr<slang::IModule> m_module;

            //! Entry names in composition order; the index is the entry-point index for
            //! IComponentType::getEntryPointCode.
            AZStd::vector<AZStd::string> m_entryPointNames;

            Slang::ComPtr<slang::IComponentType> m_linkedProgram;
        };

        //! Everything one program compilation needs besides the target descriptor.
        struct ProgramCompileRequest
        {
            //! Full path of the .slang source; also names the module in diagnostics.
            AZStd::string_view m_sourcePath;

            //! Entry points to find and check in the module.
            const MapOfStringToStageType* m_entryPoints = nullptr;

            //! Module/include search roots (ShaderLib directories etc.).
            AZStd::span<const AZStd::string> m_includePaths;

            //! Directory containing the per-API SlangApiPrelude.slang; empty skips the
            //! per-API prelude import.
            AZStd::string_view m_apiPreludeDirectory;

            //! Merged build arguments; -D definitions become session preprocessor macros.
            //! May be null.
            const RHI::ShaderBuildArguments* m_buildArguments = nullptr;
        };

        //! Runs the frontend pipeline: session from @targetDescriptor, source load with injected
        //! prelude imports, entry-point discovery, compose and link.
        //! The caller must hold SlangCompilerService::AcquireCompilerLock() for the whole
        //! lifetime of any use of the returned compilation.
        AZ::Outcome<ProgramCompilation, AZStd::string> CompileProgram(
            const RHI::ShaderTargetDescriptor& targetDescriptor,
            const ProgramCompileRequest& request) const;

        //! Maps the target descriptor onto a compile session descriptor: target format, profile,
        //! conventions, search paths and -D definitions.
        static AZ::Outcome<SlangCompilerService::SessionDescriptor, AZStd::string> BuildSessionDescriptor(
            const RHI::ShaderTargetDescriptor& targetDescriptor,
            const ProgramCompileRequest& request);
    };
} // namespace AZ::ShaderBuilder
