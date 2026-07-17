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

#include <AzFramework/StringFunc/StringFunc.h>

#include <Editor/ShaderBuilderUtility.h>
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

        // The prelude is force-included: the file system hook injects the import into every
        // module the session loads, and the root source below gets the same injection, so the
        // Atom vocabulary needs no per-file imports anywhere.
        AZStd::vector<AZStd::string> injectedImportLines;
        injectedImportLines.push_back(AZStd::string::format("import %.*s;", AZ_STRING_ARG(ForceIncludedModuleReference)));
        if (!request.m_apiPreludeDirectory.empty())
        {
            injectedImportLines.push_back(AZStd::string::format("import %.*s;", AZ_STRING_ARG(ApiPreludeModuleName)));
        }

        ProgramCompilation compilation;
        compilation.m_fileSystem.attach(new SlangSourceFileSystem(
            injectedImportLines,
            AZStd::vector<AZStd::string>(AZStd::begin(InjectionExemptFileNames), AZStd::end(InjectionExemptFileNames))));

        // Restore diagnostics locations to the original file with a #line directive.
        AZStd::string forwardSlashedPath = sourcePath;
        AZStd::replace(forwardSlashedPath.begin(), forwardSlashedPath.end(), '\\', '/');
        AZStd::string sourceText;
        for (const AZStd::string& importLine : injectedImportLines)
        {
            sourceText += importLine;
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

        AZStd::vector<slang::IComponentType*> components;
        components.push_back(compilation.m_module);
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

        // The per-API prelude deploys beside the AzslcHeader of the same RHI.
        AZStd::string apiPreludeDirectory;
        {
            AZ::IO::FixedMaxPath azslHeaderPath = AZ::Utils::GetExecutableDirectory();
            azslHeaderPath /= input.m_shaderPlatformInterface->GetAzslHeader(*input.m_platformInfo);
            AZ::IO::FixedMaxPath preludePath = azslHeaderPath.ParentPath();
            preludePath /= "ApiPrelude.slang";
            if (AZ::IO::SystemFile::Exists(preludePath.c_str()))
            {
                apiPreludeDirectory = azslHeaderPath.ParentPath().String();
            }
            else
            {
                AZ_Warning(
                    AZStd::string(input.m_builderName).c_str(), false, "No per-API Slang prelude found at %s; compiling without it",
                    preludePath.c_str());
            }
        }

        ProgramCompileRequest request;
        request.m_sourcePath = input.m_shaderSourceFullPath;
        request.m_entryPoints = input.m_entryPoints;
        request.m_includePaths = input.m_includePaths;
        request.m_apiPreludeDirectory = apiPreludeDirectory;
        request.m_buildArguments = input.m_buildArguments;

        SlangCompilerService& compilerService = SlangCompilerService::Get();
        auto compilerLock = compilerService.AcquireCompilerLock();
        auto compilationOutcome = CompileProgram(targetDescriptor, request);
        if (!compilationOutcome.IsSuccess())
        {
            return AZ::Failure(compilationOutcome.TakeError());
        }

        return AZ::Failure(AZStd::string::format(
            "%.*s compiled and linked, but the Slang backend cannot produce the reflection contract yet — the reflection walker lands with plan milestone M8",
            AZ_STRING_ARG(input.m_shaderSourceFullPath)));
    }

    AZ::Outcome<StageResult, AZStd::string> SlangBackend::CompileStage(const StageInput& input)
    {
        return AZ::Failure(AZStd::string::format(
            "The Slang backend cannot compile stage %.*s yet — stage compilation lands with plan milestone M8",
            AZ_STRING_ARG(input.m_entryPointName)));
    }
} // namespace AZ::ShaderBuilder
