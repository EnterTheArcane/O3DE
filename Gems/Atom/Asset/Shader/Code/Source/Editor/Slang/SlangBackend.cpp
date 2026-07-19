/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "SlangBackend.h"

#include <AzCore/IO/Path/Path.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/StringFunc/StringFunc.h>
#include <AzCore/Utils/Utils.h>
#include <AzCore/std/limits.h>
#include <AzCore/std/optional.h>

#include <AzFramework/StringFunc/StringFunc.h>

#include <AzCore/Serialization/Utils.h>

#include <Editor/ShaderBuilderUtility.h>
#include <Slang/SlangModuleClosure.h>
#include <Slang/SlangReflectionWalker.h>
#include <Slang/SlangSourceFileSystem.h>

namespace AZ::ShaderBuilder
{
    static constexpr AZStd::string_view SlangBackendName = "slang";

    //! Name of the argument group in shader_build_options.settings whose entries this backend
    //! consumes (currently -D definitions; opaque to the RHI).
    static constexpr AZStd::string_view SlangArgumentGroupName = "slang";

    //! The force-included module: injected into every file of every compile, never imported by
    //! authors. It re-exports the ShaderResourceGroup authoring attributes, so one injection
    //! carries the whole Atom vocabulary.
    static constexpr AZStd::string_view ForceIncludedModuleReference = "Atom.RPI.Prelude";

    //! Module name of the per-API prelude deployed beside the AzslcHeaders.
    static constexpr AZStd::string_view ApiPreludeModuleName = "ApiPrelude";

    //! Module name of the generated shader-options accessor-implementation module.
    static constexpr AZStd::string_view GeneratedOptionsModuleName = "AtomGeneratedOptions";

    //! Files served without prelude injection so force-inclusion cannot create import cycles.
    static constexpr const char* InjectionExemptFileNames[] = {
        "Prelude.slang",
        "ShaderResourceGroup.slang",
        "ApiPrelude.slang",
    };

    AZStd::string_view SlangBackend::GetName() const
    {
        return SlangBackendName;
    }

    AZStd::span<const AZStd::string_view> SlangBackend::GetSourceExtensions() const
    {
        static constexpr AZStd::string_view sourceExtensions[] = {
            ".slang",
        };
        return sourceExtensions;
    }

    bool SlangBackend::CanCompileTarget(const RHI::ShaderTargetDescriptor& targetDescriptor) const
    {
        return targetDescriptor.m_format == RHI::ShaderTargetFormat::Dxil
            || targetDescriptor.m_format == RHI::ShaderTargetFormat::Spirv;
    }

    static SlangStage ToSlangStage(RPI::ShaderStageType stageType)
    {
        switch (stageType)
        {
        case RPI::ShaderStageType::Vertex:
            return SLANG_STAGE_VERTEX;
        case RPI::ShaderStageType::Geometry:
            return SLANG_STAGE_GEOMETRY;
        case RPI::ShaderStageType::TessellationControl:
            return SLANG_STAGE_HULL;
        case RPI::ShaderStageType::TessellationEvaluation:
            return SLANG_STAGE_DOMAIN;
        case RPI::ShaderStageType::Fragment:
            return SLANG_STAGE_FRAGMENT;
        case RPI::ShaderStageType::Compute:
            return SLANG_STAGE_COMPUTE;
        default:
            return SLANG_STAGE_NONE;
        }
    }

    //! Whether the merged build arguments request specialization-constant option lowering: the
    //! engine-default shader_build_options put "-sc-options" in the "slang" argument group,
    //! mirroring the azslc flag, so supervariants opt out per scope the same way in both
    //! languages.
    static bool UseSpecializationConstantsRequested(const RHI::ShaderBuildArguments* buildArguments)
    {
        if (!buildArguments)
        {
            return false;
        }
        for (const AZStd::string& argument : buildArguments->GetNamedArgumentGroup(SlangArgumentGroupName))
        {
            if (argument == "-sc-options")
            {
                return true;
            }
        }
        return false;
    }

    //! Splits a "-DNAME" or "-DNAME=VALUE" argument into a session preprocessor macro.
    static void AppendDefinitionAsMacro(
        AZStd::string_view argument,
        AZStd::vector<AZStd::pair<AZStd::string, AZStd::string>>& macros)
    {
        if (!argument.starts_with("-D") || argument.size() <= 2)
        {
            return;
        }
        const AZStd::string_view definition = argument.substr(2);
        const size_t equalsPosition = definition.find('=');
        if (equalsPosition == AZStd::string_view::npos)
        {
            macros.push_back({AZStd::string(definition), ""});
        }
        else
        {
            macros.push_back({AZStd::string(definition.substr(0, equalsPosition)), AZStd::string(definition.substr(equalsPosition + 1))});
        }
    }

    AZ::Outcome<SlangCompilerService::SessionDescriptor, AZStd::string> SlangBackend::BuildSessionDescriptor(
        const RHI::ShaderTargetDescriptor& targetDescriptor,
        const ProgramCompileRequest& request)
    {
        SlangCompilerService::SessionDescriptor sessionDescriptor;

        switch (targetDescriptor.m_format)
        {
        case RHI::ShaderTargetFormat::Dxil:
            sessionDescriptor.m_target = SLANG_DXIL;
            sessionDescriptor.m_profile = "sm_6_2";
            {
                // The embedded DXC stamps validator version 1.10; the dxsc.exe specialization
                // patch tool accepts at most 1.9 (feasibility gate 2)
                slang::CompilerOptionEntry downstreamArguments = {};
                downstreamArguments.name = slang::CompilerOptionName::DownstreamArgs;
                downstreamArguments.value.kind = slang::CompilerOptionValueKind::String;
                downstreamArguments.value.stringValue0 = "dxc";
                downstreamArguments.value.stringValue1 = "-validator-version 1.9";
                sessionDescriptor.m_extraOptions.push_back(downstreamArguments);
            }
            break;
        case RHI::ShaderTargetFormat::Spirv:
            sessionDescriptor.m_target = SLANG_SPIRV;
            sessionDescriptor.m_profile = "spirv_1_5";
            break;
        default:
            return AZ::Failure(AZStd::string::format(
                "The Slang backend cannot produce target format %u", static_cast<uint32_t>(targetDescriptor.m_format)));
        }

        // Conventions map onto session options. Binding-index uniqueness per set needs no option:
        // Slang assigns SPIR-V bindings from one counter per descriptor set already.
        if (targetDescriptor.m_conventions.m_invertY)
        {
            slang::CompilerOptionEntry invertY = {};
            invertY.name = slang::CompilerOptionName::VulkanInvertY;
            invertY.value.kind = slang::CompilerOptionValueKind::Int;
            invertY.value.intValue0 = 1;
            sessionDescriptor.m_extraOptions.push_back(invertY);
        }
        if (targetDescriptor.m_conventions.m_useDxPositionW)
        {
            slang::CompilerOptionEntry useDxPositionW = {};
            useDxPositionW.name = slang::CompilerOptionName::VulkanUseDxPositionW;
            useDxPositionW.value.kind = slang::CompilerOptionValueKind::Int;
            useDxPositionW.value.intValue0 = 1;
            sessionDescriptor.m_extraOptions.push_back(useDxPositionW);
        }
        if (targetDescriptor.m_format == RHI::ShaderTargetFormat::Spirv)
        {
            // Atom lays out Vulkan structured/constant buffers with DirectX memory rules (as AZSLc
            // does), so one CPU-side struct and one shared-SRG layout serve both APIs. Without this,
            // Slang emits std430 for SPIR-V and diverges on any buffer element containing a float3
            // (16-byte-aligned under std430, tightly packed under DX rules).
            slang::CompilerOptionEntry forceDxLayout = {};
            forceDxLayout.name = slang::CompilerOptionName::ForceDXLayout;
            forceDxLayout.value.kind = slang::CompilerOptionValueKind::Int;
            forceDxLayout.value.intValue0 = 1;
            sessionDescriptor.m_extraOptions.push_back(forceDxLayout);
        }

        // Search order mirrors the module resolver: the importing file's directory first, then
        // the project include roots, then the per-API prelude directory.
        AZStd::string sourceDirectory;
        AzFramework::StringFunc::Path::GetFolderPath(AZStd::string(request.m_sourcePath).c_str(), sourceDirectory);
        if (!sourceDirectory.empty())
        {
            sessionDescriptor.m_searchPaths.push_back(sourceDirectory);
        }
        for (const AZStd::string& includePath : request.m_includePaths)
        {
            sessionDescriptor.m_searchPaths.push_back(includePath);
        }
        if (!request.m_apiPreludeDirectory.empty())
        {
            sessionDescriptor.m_searchPaths.push_back(AZStd::string(request.m_apiPreludeDirectory));
        }

        if (request.m_buildArguments)
        {
            for (const AZStd::string& argument : request.m_buildArguments->m_preprocessorArguments)
            {
                AppendDefinitionAsMacro(argument, sessionDescriptor.m_preprocessorMacros);
            }
            for (const AZStd::string& argument : request.m_buildArguments->GetNamedArgumentGroup(SlangArgumentGroupName))
            {
                AppendDefinitionAsMacro(argument, sessionDescriptor.m_preprocessorMacros);
            }
            sessionDescriptor.m_generateDebugInfo = request.m_buildArguments->m_generateDebugInfo;
        }

        return AZ::Success(AZStd::move(sessionDescriptor));
    }

    //! The shared back half of a program compilation, common to the source and module-closure
    //! paths: options discovery over the loaded modules, the per-mode generated implementation
    //! module, entry-point discovery, composition and link. @compilation must arrive with
    //! m_session and m_module set.
    static AZ::Outcome<void, AZStd::string> ComposeAndLinkProgram(
        SlangBackend::ProgramCompilation& compilation,
        const RHI::ShaderTargetDescriptor& targetDescriptor,
        const SlangBackend::ProgramCompileRequest& request,
        const AZStd::string& sourcePath)
    {
        // Shader options: discover the option declarations across the loaded modules and
        // satisfy their accessor externs with the generated implementation module for the
        // requested lowering mode.
        auto discoveredOutcome = SlangOptionsModuleGenerator::DiscoverShaderOptions(compilation.m_session);
        if (!discoveredOutcome.IsSuccess())
        {
            return AZ::Failure(AZStd::string::format("%s: %s", sourcePath.c_str(), discoveredOutcome.GetError().c_str()));
        }
        compilation.m_discoveredOptions = discoveredOutcome.TakeValue();
        if (!compilation.m_discoveredOptions.m_declarations.empty())
        {
            auto layoutOutcome = SlangOptionsModuleGenerator::BuildShaderOptionGroupLayout(compilation.m_discoveredOptions.m_declarations);
            if (!layoutOutcome.IsSuccess())
            {
                return AZ::Failure(AZStd::string::format("%s: %s", sourcePath.c_str(), layoutOutcome.GetError().c_str()));
            }
            compilation.m_shaderOptionLayout = layoutOutcome.TakeValue();

            AZStd::string implementationSource;
            switch (request.m_optionsLoweringMode)
            {
            case ShaderOptionLoweringMode::Baked:
                if (!request.m_bakedOptionValues)
                {
                    return AZ::Failure(AZStd::string::format(
                        "%s: Baked option lowering was requested without option values", sourcePath.c_str()));
                }
                if (request.m_bakedOptionValues->GetShaderOptionLayout()->GetHash() != compilation.m_shaderOptionLayout->GetHash())
                {
                    return AZ::Failure(AZStd::string::format(
                        "%s: the variant's option values were resolved against a different option layout than the "
                        "source declares — the shader and its variant list are out of sync",
                        sourcePath.c_str()));
                }
                if (!request.m_bakedOptionValues->IsFullySpecified()
                    && compilation.m_discoveredOptions.m_fallbackMemberName.empty())
                {
                    return AZ::Failure(AZStd::string::format(
                        "%s: a partially specified variant leaves options reading the ShaderVariantKey fallback at "
                        "runtime: designate a public uint4 ShaderResourceGroup member with [AtomVariantFallback]",
                        sourcePath.c_str()));
                }
                implementationSource = SlangOptionsModuleGenerator::GenerateBakedValuesModule(
                    GeneratedOptionsModuleName, compilation.m_discoveredOptions, *request.m_bakedOptionValues);
                break;
            case ShaderOptionLoweringMode::SpecializationConstant:
                implementationSource = SlangOptionsModuleGenerator::GenerateImplementationModule(
                    request.m_optionsLoweringMode, targetDescriptor.m_format, GeneratedOptionsModuleName,
                    compilation.m_discoveredOptions, *compilation.m_shaderOptionLayout);
                break;
            case ShaderOptionLoweringMode::DynamicFallback:
                if (compilation.m_discoveredOptions.m_fallbackMemberName.empty())
                {
                    return AZ::Failure(AZStd::string::format(
                        "%s declares shader options, which need a ShaderVariantKey fallback: designate a public uint4 "
                        "ShaderResourceGroup member with [AtomVariantFallback]",
                        sourcePath.c_str()));
                }
                implementationSource = SlangOptionsModuleGenerator::GenerateImplementationModule(
                    request.m_optionsLoweringMode, targetDescriptor.m_format, GeneratedOptionsModuleName,
                    compilation.m_discoveredOptions, *compilation.m_shaderOptionLayout);
                break;
            }

            Slang::ComPtr<slang::IBlob> diagnostics;
            compilation.m_optionsImplementationModule = compilation.m_session->loadModuleFromSourceString(
                AZStd::string(GeneratedOptionsModuleName).c_str(),
                AZStd::string::format("%.*s.slang", AZ_STRING_ARG(GeneratedOptionsModuleName)).c_str(),
                implementationSource.c_str(),
                diagnostics.writeRef());
            SlangCompilerService::ReportDiagnostics(sourcePath, diagnostics, !compilation.m_optionsImplementationModule);
            if (!compilation.m_optionsImplementationModule)
            {
                return AZ::Failure(AZStd::string::format(
                    "Failed to load the generated shader-options module for %s", sourcePath.c_str()));
            }
        }

        // Entry-point references are held raw with manual release: AZStd containers cannot hold
        // Slang::ComPtr (deleted unary operator&). The composed program retains them afterwards.
        AZStd::vector<slang::IEntryPoint*> ownedEntryPoints;
        auto releaseEntryPoints = [&ownedEntryPoints]()
        {
            for (slang::IEntryPoint* ownedEntryPoint : ownedEntryPoints)
            {
                ownedEntryPoint->release();
            }
            ownedEntryPoints.clear();
        };

        Slang::ComPtr<slang::IBlob> diagnostics;
        AZStd::vector<slang::IComponentType*> components;
        components.push_back(compilation.m_module);
        if (compilation.m_optionsImplementationModule)
        {
            components.push_back(compilation.m_optionsImplementationModule);
        }
        for (const auto& [entryPointName, stageType] : *request.m_entryPoints)
        {
            const SlangStage slangStage = ToSlangStage(stageType);
            if (slangStage == SLANG_STAGE_NONE)
            {
                releaseEntryPoints();
                return AZ::Failure(AZStd::string::format(
                    "Entry point %s has a stage the Slang backend does not support yet", entryPointName.c_str()));
            }

            Slang::ComPtr<slang::IEntryPoint> entryPoint;
            diagnostics = nullptr;
            compilation.m_module->findAndCheckEntryPoint(entryPointName.c_str(), slangStage, entryPoint.writeRef(), diagnostics.writeRef());
            SlangCompilerService::ReportDiagnostics(sourcePath, diagnostics, !entryPoint);
            if (!entryPoint)
            {
                releaseEntryPoints();
                return AZ::Failure(AZStd::string::format("Entry point %s was not found in %s", entryPointName.c_str(), sourcePath.c_str()));
            }
            ownedEntryPoints.push_back(entryPoint.detach());
            components.push_back(ownedEntryPoints.back());
            compilation.m_entryPointNames.push_back(entryPointName);
        }

        Slang::ComPtr<slang::IComponentType> composedProgram;
        diagnostics = nullptr;
        const SlangResult composeResult = compilation.m_session->createCompositeComponentType(
            components.data(),
            components.size(),
            composedProgram.writeRef(),
            diagnostics.writeRef());
        releaseEntryPoints();
        SlangCompilerService::ReportDiagnostics(sourcePath, diagnostics, SLANG_FAILED(composeResult));
        if (SLANG_FAILED(composeResult))
        {
            return AZ::Failure(AZStd::string::format("Failed to compose the Slang program for %s", sourcePath.c_str()));
        }

        diagnostics = nullptr;
        const SlangResult linkResult = composedProgram->link(compilation.m_linkedProgram.writeRef(), diagnostics.writeRef());
        SlangCompilerService::ReportDiagnostics(sourcePath, diagnostics, SLANG_FAILED(linkResult));
        if (SLANG_FAILED(linkResult))
        {
            return AZ::Failure(AZStd::string::format("Failed to link the Slang program for %s", sourcePath.c_str()));
        }

        return AZ::Success();
    }

    AZ::Outcome<SlangBackend::ProgramCompilation, AZStd::string> SlangBackend::CompileProgram(
        const RHI::ShaderTargetDescriptor& targetDescriptor,
        const ProgramCompileRequest& request) const
    {
        if (!request.m_entryPoints || request.m_entryPoints->empty())
        {
            return AZ::Failure(AZStd::string::format("No entry points were provided for %.*s", AZ_STRING_ARG(request.m_sourcePath)));
        }

        auto sessionDescriptorOutcome = BuildSessionDescriptor(targetDescriptor, request);
        if (!sessionDescriptorOutcome.IsSuccess())
        {
            return AZ::Failure(sessionDescriptorOutcome.TakeError());
        }

        const AZStd::string sourcePath(request.m_sourcePath);
        auto sourceOutcome = AZ::Utils::ReadFile(sourcePath, AZStd::numeric_limits<size_t>::max());
        if (!sourceOutcome.IsSuccess())
        {
            return AZ::Failure(AZStd::string::format("Failed to read %s. [%s]", sourcePath.c_str(), sourceOutcome.GetError().c_str()));
        }

        // The preamble is force-included: the file system hook injects it into every module the
        // session loads, and the root source below gets the same injection, so the Atom
        // vocabulary needs no per-file imports anywhere.
        AZStd::vector<AZStd::string> preambleLines;
        preambleLines.push_back(AZStd::string::format("import %.*s;", AZ_STRING_ARG(ForceIncludedModuleReference)));
        if (!request.m_apiPreludeDirectory.empty())
        {
            preambleLines.push_back(AZStd::string::format("import %.*s;", AZ_STRING_ARG(ApiPreludeModuleName)));
        }

        ProgramCompilation compilation;
        compilation.m_fileSystem.attach(new SlangSourceFileSystem(
            preambleLines,
            AZStd::vector<AZStd::string>(AZStd::begin(InjectionExemptFileNames), AZStd::end(InjectionExemptFileNames))));

        // Restore diagnostics locations to the original file with a #line directive.
        AZStd::string forwardSlashedPath = sourcePath;
        AZStd::replace(forwardSlashedPath.begin(), forwardSlashedPath.end(), '\\', '/');
        AZStd::string sourceText;
        for (const AZStd::string& preambleLine : preambleLines)
        {
            sourceText += preambleLine;
            sourceText += '\n';
        }
        sourceText += AZStd::string::format("#line 1 \"%s\"\n", forwardSlashedPath.c_str());
        sourceText += sourceOutcome.GetValue();

        SlangCompilerService::SessionDescriptor sessionDescriptor = sessionDescriptorOutcome.TakeValue();
        sessionDescriptor.m_fileSystem = compilation.m_fileSystem.get();

        SlangCompilerService& compilerService = SlangCompilerService::Get();
        auto sessionOutcome = compilerService.CreateSession(sessionDescriptor);
        if (!sessionOutcome.IsSuccess())
        {
            return AZ::Failure(sessionOutcome.TakeError());
        }

        compilation.m_session = sessionOutcome.TakeValue();

        AZStd::string moduleName = ShaderBuilderUtility::ExtractStemName(sourcePath.c_str());

        Slang::ComPtr<slang::IBlob> diagnostics;
        compilation.m_module = compilation.m_session->loadModuleFromSourceString(
            moduleName.c_str(),
            forwardSlashedPath.c_str(),
            sourceText.c_str(),
            diagnostics.writeRef());
        SlangCompilerService::ReportDiagnostics(sourcePath, diagnostics, !compilation.m_module);
        if (!compilation.m_module)
        {
            return AZ::Failure(AZStd::string::format("Failed to load Slang module %s", sourcePath.c_str()));
        }

        auto composeOutcome = ComposeAndLinkProgram(compilation, targetDescriptor, request, sourcePath);
        if (!composeOutcome.IsSuccess())
        {
            return AZ::Failure(composeOutcome.TakeError());
        }

        return AZ::Success(AZStd::move(compilation));
    }

    AZ::Outcome<SlangBackend::ProgramCompilation, AZStd::string> SlangBackend::CompileProgramFromClosure(
        const RHI::ShaderTargetDescriptor& targetDescriptor,
        const ProgramCompileRequest& request,
        const SlangModuleClosureBundle& bundle) const
    {
        if (!request.m_entryPoints || request.m_entryPoints->empty())
        {
            return AZ::Failure(AZStd::string::format("No entry points were provided for %.*s", AZ_STRING_ARG(request.m_sourcePath)));
        }

        SlangCompilerService& compilerService = SlangCompilerService::Get();
        auto validationOutcome = ValidateModuleClosureBundle(
            bundle, compilerService.GetCompilerBuildTag(), static_cast<uint32_t>(targetDescriptor.m_format));
        if (!validationOutcome.IsSuccess())
        {
            return AZ::Failure(validationOutcome.TakeError());
        }

        auto sessionDescriptorOutcome = BuildSessionDescriptor(targetDescriptor, request);
        if (!sessionDescriptorOutcome.IsSuccess())
        {
            return AZ::Failure(sessionDescriptorOutcome.TakeError());
        }

        // No file system hook: a restore reads no sources, and the generated options module
        // resolves its imports against the restored in-session modules.
        auto sessionOutcome = compilerService.CreateSession(sessionDescriptorOutcome.TakeValue());
        if (!sessionOutcome.IsSuccess())
        {
            return AZ::Failure(sessionOutcome.TakeError());
        }

        ProgramCompilation compilation;
        compilation.m_session = sessionOutcome.TakeValue();

        auto restoreOutcome = RestoreModuleClosure(compilation.m_session, bundle);
        if (!restoreOutcome.IsSuccess())
        {
            return AZ::Failure(restoreOutcome.TakeError());
        }
        compilation.m_module = restoreOutcome.TakeValue();

        const AZStd::string sourcePath(request.m_sourcePath);
        auto composeOutcome = ComposeAndLinkProgram(compilation, targetDescriptor, request, sourcePath);
        if (!composeOutcome.IsSuccess())
        {
            return AZ::Failure(composeOutcome.TakeError());
        }

        return AZ::Success(AZStd::move(compilation));
    }

    void SlangBackend::EnumerateSourceDependencies(
        AZStd::string_view shaderSourceFullPath,
        AZStd::span<const AZStd::string> includePaths,
        AZStd::unordered_set<AZStd::string>& sourceDependencies) const
    {
        const SlangModuleResolver resolver(AZStd::vector<AZStd::string>(includePaths.begin(), includePaths.end()));

        const AZStd::string sourceFullPath(shaderSourceFullPath);
        AZStd::vector<AZStd::string> pendingFiles;
        AZStd::unordered_set<AZStd::string> visitedFiles;
        pendingFiles.push_back(sourceFullPath);
        visitedFiles.insert(sourceFullPath);

        // The force-included module never appears in a source scan; its reference resolves
        // exactly as if written at the top of the shader source, and its own imports (the
        // re-exported ShaderResourceGroup module) follow transitively.
        auto resolveReference = [&](AZStd::string_view moduleReference, const AZStd::string& importingFilePath)
        {
            const SlangModuleResolver::Resolution resolution = resolver.ResolveModule(moduleReference, importingFilePath);
            for (const AZStd::string& shadowCandidate : resolution.m_shadowCandidates)
            {
                sourceDependencies.insert(shadowCandidate);
            }
            if (resolution.IsResolved() && !visitedFiles.contains(resolution.m_resolvedPath))
            {
                sourceDependencies.insert(resolution.m_resolvedPath);
                visitedFiles.insert(resolution.m_resolvedPath);
                pendingFiles.push_back(resolution.m_resolvedPath);
            }
        };

        resolveReference(ForceIncludedModuleReference, sourceFullPath);

        while (!pendingFiles.empty())
        {
            const AZStd::string currentFile = AZStd::move(pendingFiles.back());
            pendingFiles.pop_back();

            auto contentOutcome = AZ::Utils::ReadFile(currentFile, AZStd::numeric_limits<size_t>::max());
            if (!contentOutcome.IsSuccess())
            {
                continue;
            }

            for (const AZStd::string& moduleReference : SlangModuleResolver::ParseModuleReferences(contentOutcome.GetValue()))
            {
                resolveReference(moduleReference, currentFile);
            }
        }
    }

    //! The per-API prelude deploys beside the AzslcHeader of the same RHI; empty when absent.
    static AZStd::string GetApiPreludeDirectory(
        RHI::ShaderPlatformInterface& shaderPlatformInterface,
        const AssetBuilderSDK::PlatformInfo& platformInfo,
        [[maybe_unused]] AZStd::string_view builderName)
    {
        AZ::IO::FixedMaxPath azslHeaderPath = AZ::Utils::GetExecutableDirectory();
        azslHeaderPath /= shaderPlatformInterface.GetAzslHeader(platformInfo);
        AZ::IO::FixedMaxPath preludePath = azslHeaderPath.ParentPath();
        preludePath /= "ApiPrelude.slang";
        if (AZ::IO::SystemFile::Exists(preludePath.c_str()))
        {
            return AZStd::string(azslHeaderPath.ParentPath().String());
        }
        AZ_Warning(
            AZStd::string(builderName).c_str(), false, "No per-API Slang prelude found at %s; compiling without it",
            preludePath.c_str());
        return "";
    }

    AZ::Outcome<FrontendResult, AZStd::string> SlangBackend::CompileFrontend(const FrontendInput& input)
    {
        const RHI::ShaderTargetDescriptor targetDescriptor = input.m_shaderPlatformInterface->GetShaderTargetDescriptor(*input.m_platformInfo);
        if (!CanCompileTarget(targetDescriptor))
        {
            // Skip-vs-fail rule 3: an enabled RHI whose declared target no backend can produce
            // fails loudly; silence would hide missing platform support behind a green build
            return AZ::Failure(AZStd::string::format(
                "The Slang backend cannot produce the target the %s RHI declares",
                input.m_shaderPlatformInterface->GetAPIName().GetCStr()));
        }

        ProgramCompileRequest request;
        request.m_sourcePath = input.m_shaderSourceFullPath;
        request.m_entryPoints = input.m_entryPoints;
        request.m_includePaths = input.m_includePaths;
        const AZStd::string apiPreludeDirectory = GetApiPreludeDirectory(*input.m_shaderPlatformInterface, *input.m_platformInfo, input.m_builderName);
        request.m_apiPreludeDirectory = apiPreludeDirectory;
        request.m_buildArguments = input.m_buildArguments;
        request.m_optionsLoweringMode = UseSpecializationConstantsRequested(input.m_buildArguments)
            ? ShaderOptionLoweringMode::SpecializationConstant
            : ShaderOptionLoweringMode::DynamicFallback;

        SlangCompilerService& compilerService = SlangCompilerService::Get();
        auto compilerLock = compilerService.AcquireCompilerLock();
        auto compilationOutcome = CompileProgram(targetDescriptor, request);
        if (!compilationOutcome.IsSuccess())
        {
            return AZ::Failure(compilationOutcome.TakeError());
        }
        const ProgramCompilation compilation = compilationOutcome.TakeValue();

        auto reflectionOutcome = SlangReflectionWalker::BuildReflectionData(
            compilation.m_linkedProgram,
            targetDescriptor.m_format,
            compilation.m_entryPointNames);
        if (!reflectionOutcome.IsSuccess())
        {
            return AZ::Failure(reflectionOutcome.TakeError());
        }

        FrontendResult result;
        result.m_reflection = reflectionOutcome.TakeValue();

        // Discovered options replace the walker's DefaultOption placeholder, and the
        // [AtomVariantFallback] designation lands on its ShaderResourceGroup, checked against
        // what reflection actually grouped.
        if (compilation.m_shaderOptionLayout)
        {
            const auto& layoutOptions = compilation.m_shaderOptionLayout->GetShaderOptions();
            result.m_reflection.m_shaderOptions.assign(layoutOptions.begin(), layoutOptions.end());
            result.m_reflection.m_usesSpecializationConstants =
                request.m_optionsLoweringMode == ShaderOptionLoweringMode::SpecializationConstant;
        }
        if (!compilation.m_discoveredOptions.m_fallbackMemberName.empty())
        {
            const AZStd::string& fallbackGroupName = compilation.m_discoveredOptions.m_fallbackShaderResourceGroupName;
            auto groupIterator = AZStd::find_if(
                result.m_reflection.m_shaderResourceGroups.begin(),
                result.m_reflection.m_shaderResourceGroups.end(),
                [&fallbackGroupName](const ShaderResourceGroupReflection& groupReflection)
                {
                    return groupReflection.m_name.GetStringView() == fallbackGroupName;
                });
            if (groupIterator == result.m_reflection.m_shaderResourceGroups.end())
            {
                return AZ::Failure(AZStd::string::format(
                    "[AtomVariantFallback] is declared in ParameterBlock %s, which is not a ShaderResourceGroup of %.*s",
                    fallbackGroupName.c_str(), AZ_STRING_ARG(input.m_shaderSourceFullPath)));
            }
            const Name fallbackConstantName(compilation.m_discoveredOptions.m_fallbackMemberName);
            const bool fallbackConstantFound = AZStd::any_of(
                groupIterator->m_constants.begin(), groupIterator->m_constants.end(),
                [&fallbackConstantName](const RHI::ShaderInputConstantDescriptor& constant)
                {
                    return constant.m_name == fallbackConstantName;
                });
            if (!fallbackConstantFound)
            {
                return AZ::Failure(AZStd::string::format(
                    "[AtomVariantFallback] designates %s.%s, which reflection does not list among the ShaderResourceGroup constants",
                    fallbackGroupName.c_str(), fallbackConstantName.GetCStr()));
            }
            groupIterator->m_shaderVariantKeyFallbackName = fallbackConstantName;
            groupIterator->m_shaderVariantKeyFallbackSize = RPI::ShaderVariantKeyBitCount;
        }

        // Cached products for downstream builders: the AZ-serialized reflection contract and the
        // module closure (a serialized Slang module excludes its imports, so variant builds need
        // the whole closure to relink without source — consumed at M12).
        const AZStd::string apiName(input.m_shaderPlatformInterface->GetAPIName().GetCStr());
        {
            AZStd::string reflectionPath;
            AzFramework::StringFunc::Path::Join(
                AZStd::string(input.m_tempDirPath).c_str(),
                AZStd::string::format("%.*s_%s.reflectiondata", AZ_STRING_ARG(input.m_stemName), apiName.c_str()).c_str(),
                reflectionPath);
            if (!AZ::Utils::SaveObjectToFile(reflectionPath, AZ::DataStream::ST_BINARY, &result.m_reflection))
            {
                return AZ::Failure(AZStd::string::format("Failed to write the reflection product to %s", reflectionPath.c_str()));
            }
            result.m_subProducts.push_back({reflectionPath, aznumeric_cast<uint32_t>(RPI::ShaderAssetSubId::ReflectionData)});
        }
        {
            // The generated options module is excluded: variant relinks regenerate it for their
            // own values, and the bundled copy must not shadow the replacement
            static constexpr AZStd::string_view excludedModuleNames[] = {GeneratedOptionsModuleName};
            auto bundleOutcome = BuildModuleClosureBundle(
                compilation.m_session,
                compilerService.GetCompilerBuildTag(),
                static_cast<uint32_t>(targetDescriptor.m_format),
                compilation.m_module->getName(),
                excludedModuleNames);
            if (!bundleOutcome.IsSuccess())
            {
                return AZ::Failure(bundleOutcome.TakeError());
            }
            AZStd::string closurePath;
            AzFramework::StringFunc::Path::Join(
                AZStd::string(input.m_tempDirPath).c_str(),
                AZStd::string::format("%.*s_%s.moduleclosure", AZ_STRING_ARG(input.m_stemName), apiName.c_str()).c_str(),
                closurePath);
            const SlangModuleClosureBundle bundle = bundleOutcome.TakeValue();
            if (!AZ::Utils::SaveObjectToFile(closurePath, AZ::DataStream::ST_BINARY, &bundle))
            {
                return AZ::Failure(AZStd::string::format("Failed to write the module closure product to %s", closurePath.c_str()));
            }
            result.m_subProducts.push_back({closurePath, aznumeric_cast<uint32_t>(RPI::ShaderAssetSubId::ModuleClosureBundle)});
        }

        // Stage compiles receive the original source; when a module-closure product is also
        // handed to them they relink from it and keep the source as the fallback.
        result.m_targetSourcePath = input.m_shaderSourceFullPath;
        auto sourceCodeOutcome = AZ::Utils::ReadFile(AZStd::string(input.m_shaderSourceFullPath), AZStd::numeric_limits<size_t>::max());
        if (sourceCodeOutcome.IsSuccess())
        {
            result.m_targetSourceCode = sourceCodeOutcome.TakeValue();
        }
        return AZ::Success(AZStd::move(result));
    }

    static RPI::ShaderStageType ToStageType(RHI::ShaderHardwareStage hardwareStage)
    {
        switch (hardwareStage)
        {
        case RHI::ShaderHardwareStage::Vertex:
            return RPI::ShaderStageType::Vertex;
        case RHI::ShaderHardwareStage::Geometry:
            return RPI::ShaderStageType::Geometry;
        case RHI::ShaderHardwareStage::Fragment:
            return RPI::ShaderStageType::Fragment;
        case RHI::ShaderHardwareStage::RayTracing:
            return RPI::ShaderStageType::RayTracing;
        case RHI::ShaderHardwareStage::Compute:
        default:
            return RPI::ShaderStageType::Compute;
        }
    }

    AZ::Outcome<VariantCompilationInputs, AZStd::string> SlangBackend::LoadVariantCompilationInputs(
        const VariantCompilationInputsRequest& request)
    {
        const AZStd::string shaderDescriptorPath(request.m_shaderDescriptorPath);
        const AZStd::string platformIdentifier(request.m_platformInfo->m_identifier);
        const RPI::SupervariantIndex supervariantIndex(request.m_supervariantIndex);

        VariantCompilationInputs inputs;

        // Option layout and specialization flag from the frontend's reflection product
        auto reflectionPathOutcome = ShaderBuilderUtility::ObtainBuildArtifactPathFromShaderAssetBuilder(
            request.m_shaderPlatformInterface->GetAPIUniqueIndex(), platformIdentifier, shaderDescriptorPath,
            supervariantIndex.GetIndex(), RPI::ShaderAssetSubId::ReflectionData);
        if (!reflectionPathOutcome.IsSuccess())
        {
            return AZ::Failure(reflectionPathOutcome.TakeError());
        }
        ShaderReflectionData reflectionData;
        if (!AZ::Utils::LoadObjectFromFileInPlace(reflectionPathOutcome.GetValue(), reflectionData))
        {
            return AZ::Failure(AZStd::string::format(
                "Failed to load the reflection product at %s", reflectionPathOutcome.GetValue().c_str()));
        }
        inputs.m_shaderOptionGroupLayout = BuildShaderOptionGroupLayout(reflectionData);
        if (!inputs.m_shaderOptionGroupLayout)
        {
            return AZ::Failure(AZStd::string::format(
                "Failed to build the shader option group layout from the reflection product of %s", shaderDescriptorPath.c_str()));
        }
        inputs.m_useSpecializationConstants = reflectionData.m_usesSpecializationConstants;

        if (!request.m_loadStageInputs)
        {
            return AZ::Success(AZStd::move(inputs));
        }

        if (request.m_shaderPlatformInterface->VariantCompilationRequiresSrgLayoutData())
        {
            return AZ::Failure(AZStd::string::format(
                "The %s RHI requires ShaderResourceGroup layout data for variant compilation, which the Slang backend "
                "does not provide yet",
                request.m_shaderPlatformInterface->GetAPIName().GetCStr()));
        }

        // Stage compiles start from the original source; the module closure lets them relink
        // without it. A missing closure product is not an error — the source path stands alone.
        inputs.m_stageSourcePath = request.m_shaderSourceFullPath;
        auto closurePathOutcome = ShaderBuilderUtility::ObtainBuildArtifactPathFromShaderAssetBuilder(
            request.m_shaderPlatformInterface->GetAPIUniqueIndex(), platformIdentifier, shaderDescriptorPath,
            supervariantIndex.GetIndex(), RPI::ShaderAssetSubId::ModuleClosureBundle);
        if (closurePathOutcome.IsSuccess())
        {
            inputs.m_moduleClosurePath = closurePathOutcome.TakeValue();
        }
        else
        {
            AZ_Warning(
                AZStd::string(request.m_builderName).c_str(), false,
                "No module-closure product for %s; variant compiles will run the source frontend. [%s]",
                shaderDescriptorPath.c_str(), closurePathOutcome.GetError().c_str());
        }
        return AZ::Success(AZStd::move(inputs));
    }

    AZ::Outcome<StageResult, AZStd::string> SlangBackend::CompileStage(const StageInput& input)
    {
        const RHI::ShaderTargetDescriptor targetDescriptor = input.m_shaderPlatformInterface->GetShaderTargetDescriptor(*input.m_platformInfo);
        if (!CanCompileTarget(targetDescriptor))
        {
            return AZ::Failure(AZStd::string::format(
                "The Slang backend cannot produce the target the %s RHI declares",
                input.m_shaderPlatformInterface->GetAPIName().GetCStr()));
        }

        const AZStd::string entryPointName(input.m_entryPointName);
        const MapOfStringToStageType entryPoints = {
            {entryPointName, ToStageType(input.m_stage)},
        };

        ProgramCompileRequest request;
        request.m_sourcePath = input.m_sourcePath;
        request.m_entryPoints = &entryPoints;
        request.m_includePaths = input.m_includePaths;
        const AZStd::string apiPreludeDirectory = GetApiPreludeDirectory(*input.m_shaderPlatformInterface, *input.m_platformInfo, input.m_builderName);
        request.m_apiPreludeDirectory = apiPreludeDirectory;
        request.m_buildArguments = input.m_buildArguments;
        if (input.m_variantOptionValues)
        {
            // A variant compile: pinned options bake as link-time constants, unpinned options
            // keep their dynamic fallback reads
            request.m_optionsLoweringMode = ShaderOptionLoweringMode::Baked;
            request.m_bakedOptionValues = input.m_variantOptionValues;
        }
        else
        {
            request.m_optionsLoweringMode = input.m_useSpecializationConstants
                ? ShaderOptionLoweringMode::SpecializationConstant
                : ShaderOptionLoweringMode::DynamicFallback;
        }

        SlangCompilerService& compilerService = SlangCompilerService::Get();
        auto compilerLock = compilerService.AcquireCompilerLock();

        // Relink from the frontend's module closure when one is available and matches the running
        // compiler; recompiling from source is the mandatory fallback. Closure relink is the
        // optimization that avoids re-parsing/re-checking for the many *derived* variants (baked
        // option values), and produces byte-identical bytecode to a source recompile (M12). The root
        // variant gains nothing from it, so it compiles from source — which also keeps root builds
        // robust against Slang's IR round-trip limits on very large modules (e.g. shaders that pull
        // in the full shared Scene/View SRGs).
        AZStd::optional<ProgramCompilation> restoredCompilation;
        if (!input.m_moduleClosurePath.empty() && input.m_variantOptionValues)
        {
            const AZStd::string moduleClosurePath(input.m_moduleClosurePath);
            SlangModuleClosureBundle bundle;
            if (AZ::Utils::LoadObjectFromFileInPlace(moduleClosurePath, bundle))
            {
                auto closureOutcome = CompileProgramFromClosure(targetDescriptor, request, bundle);
                if (closureOutcome.IsSuccess())
                {
                    restoredCompilation.emplace(closureOutcome.TakeValue());
                }
                else
                {
                    AZ_Warning(
                        AZStd::string(input.m_builderName).c_str(), false,
                        "Module-closure relink unavailable for %.*s; recompiling from source. [%s]",
                        AZ_STRING_ARG(input.m_sourcePath), closureOutcome.GetError().c_str());
                }
            }
            else
            {
                AZ_Warning(
                    AZStd::string(input.m_builderName).c_str(), false,
                    "Failed to read the module-closure bundle at %s; recompiling from source", moduleClosurePath.c_str());
            }
        }
        if (!restoredCompilation)
        {
            auto compilationOutcome = CompileProgram(targetDescriptor, request);
            if (!compilationOutcome.IsSuccess())
            {
                return AZ::Failure(compilationOutcome.TakeError());
            }
            restoredCompilation.emplace(compilationOutcome.TakeValue());
        }
        const ProgramCompilation& compilation = *restoredCompilation;

        Slang::ComPtr<slang::IBlob> bytecode;
        Slang::ComPtr<slang::IBlob> diagnostics;
        const SlangResult codeResult = compilation.m_linkedProgram->getEntryPointCode(0, 0, bytecode.writeRef(), diagnostics.writeRef());
        SlangCompilerService::ReportDiagnostics(input.m_sourcePath, diagnostics, SLANG_FAILED(codeResult));
        if (SLANG_FAILED(codeResult) || !bytecode)
        {
            return AZ::Failure(AZStd::string::format("Failed to generate code for entry point %s", entryPointName.c_str()));
        }

        StageResult result;
        result.m_descriptor.m_stageType = input.m_stage;
        result.m_descriptor.m_entryFunctionName = entryPointName;
        const uint8_t* bytes = static_cast<const uint8_t*>(bytecode->getBufferPointer());
        result.m_descriptor.m_byteCode.assign(bytes, bytes + bytecode->getBufferSize());

        // Bytecode-level fixups the RHI declares (DX12 patches specialization-constant sentinels
        // here and records the patch offsets in m_extraData)
        const bool stageUsesSpecializationConstants =
            input.m_useSpecializationConstants && compilation.m_shaderOptionLayout != nullptr;
        if (!input.m_shaderPlatformInterface->PostProcessStage(
                AZStd::string(input.m_sourcePath), AZStd::string(input.m_tempDirPath), stageUsesSpecializationConstants, result.m_descriptor))
        {
            return AZ::Failure(AZStd::string::format(
                "Post-processing failed for entry point %s of %.*s", entryPointName.c_str(), AZ_STRING_ARG(input.m_sourcePath)));
        }
        return AZ::Success(AZStd::move(result));
    }
} // namespace AZ::ShaderBuilder
