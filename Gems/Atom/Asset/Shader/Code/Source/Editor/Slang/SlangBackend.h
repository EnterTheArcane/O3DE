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
#include <Slang/SlangOptionsModuleGenerator.h>

namespace AZ::ShaderBuilder
{
    struct SlangModuleClosureBundle;

    //! The Slang shader language behind the language-backend seam: .slang sources compile
    //! in-process (slang-compiler.dll) straight to the RHI's declared target IR — no HLSL
    //! intermediate, no compiler subprocess. The frontend emits the language-neutral reflection
    //! contract plus two cached products: the AZ-serialized reflection and the module closure,
    //! from which variant builds relink without re-running the source frontend.
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

        AZ::Outcome<VariantCompilationInputs, AZStd::string> LoadVariantCompilationInputs(
            const VariantCompilationInputsRequest& request) override;

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

            //! Shader options discovered from the [AtomOption] extern function declarations
            //! across the loaded modules, and the generated implementation module that satisfies
            //! them (null when the shader declares no options).
            SlangOptionsModuleGenerator::DiscoveredShaderOptions m_discoveredOptions;
            RPI::Ptr<RPI::ShaderOptionGroupLayout> m_shaderOptionLayout;
            Slang::ComPtr<slang::IModule> m_optionsImplementationModule;

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

            //! How discovered ATOM_OPTION declarations lower for this compile.
            ShaderOptionLoweringMode m_optionsLoweringMode = ShaderOptionLoweringMode::DynamicFallback;

            //! One variant's option values for Baked lowering; ignored in the other modes.
            //! Options the values leave unpinned keep dynamic fallback reads, so a partially
            //! specified variant needs the [AtomVariantFallback] designation.
            const RPI::ShaderOptionGroup* m_bakedOptionValues = nullptr;
        };

        //! Runs the frontend pipeline: session from @targetDescriptor, source load with injected
        //! prelude imports, entry-point discovery, compose and link.
        //! The caller must hold SlangCompilerService::AcquireCompilerLock() for the whole
        //! lifetime of any use of the returned compilation.
        AZ::Outcome<ProgramCompilation, AZStd::string> CompileProgram(
            const RHI::ShaderTargetDescriptor& targetDescriptor,
            const ProgramCompileRequest& request) const;

        //! Like CompileProgram, but restores the modules from a serialized closure bundle
        //! instead of running the source frontend — no file is read, no preprocessor or type
        //! checker runs on user code. Fails (for the caller to fall back to CompileProgram)
        //! when the bundle does not match the running compiler, target or schema.
        //! The caller must hold the compiler lock for the whole lifetime of any use of the
        //! returned compilation.
        AZ::Outcome<ProgramCompilation, AZStd::string> CompileProgramFromClosure(
            const RHI::ShaderTargetDescriptor& targetDescriptor,
            const ProgramCompileRequest& request,
            const SlangModuleClosureBundle& bundle) const;

        //! Maps the target descriptor onto a compile session descriptor: target format, profile,
        //! conventions, search paths and -D definitions.
        static AZ::Outcome<SlangCompilerService::SessionDescriptor, AZStd::string> BuildSessionDescriptor(
            const RHI::ShaderTargetDescriptor& targetDescriptor,
            const ProgramCompileRequest& request);
    };
} // namespace AZ::ShaderBuilder
