/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "SlangCompilerService.h"

#include <AzCore/Debug/Trace.h>
#include <AzCore/IO/Path/Path.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/Utils/Utils.h>

namespace AZ::ShaderBuilder
{
    SlangCompilerService& SlangCompilerService::Get()
    {
        static SlangCompilerService instance;
        return instance;
    }

    SlangCompilerService::SlangCompilerService()
    {
        [[maybe_unused]] const SlangResult result = slang::createGlobalSession(m_globalSession.writeRef());
        AZ_Assert(
            SLANG_SUCCEEDED(result) && m_globalSession,
            "%s: failed to create the Slang global session (SlangResult 0x%08x)",
            LogName,
            static_cast<uint32_t>(result));

        if (m_globalSession)
        {
            m_buildTag = m_globalSession->getBuildTagString();
            AZ_TracePrintf(LogName, "Loaded Slang compiler, build tag: %s\n", m_buildTag.c_str());

            // Slang reaches DXIL by emitting HLSL into a downstream DXC. Route it at the DXC
            // fork the engine ships for AZSLC — every DXIL then comes from the same compiler
            // regardless of source language, and the fork accepts the `volatile` fold blocker
            // (vanilla DXC reserves the keyword) the specialization-constant read below needs.
            AZ::IO::FixedMaxPath directXShaderCompilerDirectory = AZ::Utils::GetExecutableDirectory();
            directXShaderCompilerDirectory /= "Builders/DirectXShaderCompiler";
            AZ::IO::FixedMaxPath directXShaderCompilerLibrary = directXShaderCompilerDirectory;
            directXShaderCompilerLibrary /= "dxcompiler.dll";
            if (AZ::IO::SystemFile::Exists(directXShaderCompilerLibrary.c_str()))
            {
                m_globalSession->setDownstreamCompilerPath(SLANG_PASS_THROUGH_DXC, directXShaderCompilerDirectory.c_str());
            }
            else
            {
                AZ_Warning(
                    LogName, false,
                    "DirectXShaderCompiler not found at %s; Slang DXIL compiles fall back to the embedded compiler, which cannot "
                    "produce specialization-constant shaders",
                    directXShaderCompilerDirectory.c_str());
            }

            // Prepended to every HLSL source Slang generates. The volatile local blocks constant
            // folding, so the specialization id survives into the DXIL as a discrete patchable
            // dword the dxsc.exe tool can find — the same fold blocker AZSLC emits for its
            // specialization constants. `static` keeps it out of the DXIL when unreferenced
            // (including lib_* profiles), so non-specialized compiles are unaffected.
            m_globalSession->setLanguagePrelude(
                SLANG_SOURCE_LANGUAGE_HLSL,
                "static int AtomReadSpecializationConstant(int specializationId)\n"
                "{\n"
                "    volatile int specializationValue = specializationId;\n"
                "    return specializationValue;\n"
                "}\n");
        }
    }

    SlangCompilerService::~SlangCompilerService()
    {
        // The global session is released via ComPtr. slang_shutdown() is deliberately not
        // called: this destructor runs during static teardown at process exit, where other
        // statics may still hold Slang objects; the OS reclaims everything anyway.
        m_globalSession = nullptr;
    }

    AZStd::string_view SlangCompilerService::GetCompilerBuildTag() const
    {
        return m_buildTag;
    }

    AZStd::unique_lock<AZStd::recursive_mutex> SlangCompilerService::AcquireCompilerLock()
    {
        return AZStd::unique_lock<AZStd::recursive_mutex>(m_mutex);
    }

    void SlangCompilerService::ReportDiagnostics(
        [[maybe_unused]] AZStd::string_view contextPath,
        slang::IBlob* diagnostics,
        bool asError)
    {
        if (!diagnostics || diagnostics->getBufferSize() == 0)
        {
            return;
        }

        [[maybe_unused]] const int messageLength = static_cast<int>(diagnostics->getBufferSize());
        [[maybe_unused]] const char* messageText = static_cast<const char*>(diagnostics->getBufferPointer());
        if (asError)
        {
            AZ_Error(
                LogName,
                false,
                "While compiling '%.*s':\n%.*s",
                AZ_STRING_ARG(contextPath),
                messageLength,
                messageText);
        }
        else
        {
            AZ_Warning(
                LogName,
                false,
                "While compiling '%.*s':\n%.*s",
                AZ_STRING_ARG(contextPath),
                messageLength,
                messageText);
        }
    }

    AZ::Outcome<Slang::ComPtr<slang::ISession>, AZStd::string> SlangCompilerService::CreateSession(const SessionDescriptor& descriptor)
    {
        if (!m_globalSession)
        {
            return AZ::Failure(AZStd::string("The Slang global session is not available"));
        }

        slang::TargetDesc targetDesc = {};
        targetDesc.format = descriptor.m_target;
        if (!descriptor.m_profile.empty())
        {
            targetDesc.profile = m_globalSession->findProfile(descriptor.m_profile.c_str());
            if (targetDesc.profile == SLANG_PROFILE_UNKNOWN)
            {
                return AZ::Failure(AZStd::string::format("Unknown Slang profile '%s'", descriptor.m_profile.c_str()));
            }
        }

        // Views into the descriptor's storage; must stay alive until createSession() returns.
        AZStd::vector<const char*> searchPaths;
        searchPaths.reserve(descriptor.m_searchPaths.size());
        for (const AZStd::string& searchPath : descriptor.m_searchPaths)
        {
            searchPaths.push_back(searchPath.c_str());
        }

        AZStd::vector<slang::PreprocessorMacroDesc> macros;
        macros.reserve(descriptor.m_preprocessorMacros.size());
        for (const auto& [macroName, macroValue] : descriptor.m_preprocessorMacros)
        {
            slang::PreprocessorMacroDesc macroDesc;
            macroDesc.name = macroName.c_str();
            macroDesc.value = macroValue.c_str();
            macros.push_back(macroDesc);
        }

        AZStd::vector<slang::CompilerOptionEntry> options = descriptor.m_extraOptions;
        if (descriptor.m_generateDebugInfo)
        {
            slang::CompilerOptionEntry debugInfoOption;
            debugInfoOption.name = slang::CompilerOptionName::DebugInformation;
            debugInfoOption.value.kind = slang::CompilerOptionValueKind::Int;
            debugInfoOption.value.intValue0 = SLANG_DEBUG_INFO_LEVEL_STANDARD;
            options.push_back(debugInfoOption);
        }

        slang::SessionDesc sessionDesc = {};
        sessionDesc.targets = &targetDesc;
        sessionDesc.targetCount = 1;
        if (descriptor.m_matrixLayoutRow)
        {
            sessionDesc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_ROW_MAJOR;
        }
        else
        {
            sessionDesc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;
        }
        sessionDesc.searchPaths = searchPaths.data();
        sessionDesc.searchPathCount = aznumeric_cast<SlangInt>(searchPaths.size());
        sessionDesc.preprocessorMacros = macros.data();
        sessionDesc.preprocessorMacroCount = aznumeric_cast<SlangInt>(macros.size());
        sessionDesc.fileSystem = descriptor.m_fileSystem;
        sessionDesc.compilerOptionEntries = options.data();
        sessionDesc.compilerOptionEntryCount = aznumeric_cast<uint32_t>(options.size());

        Slang::ComPtr<slang::ISession> session;
        const SlangResult result = m_globalSession->createSession(sessionDesc, session.writeRef());
        if (SLANG_FAILED(result) || !session)
        {
            return AZ::Failure(AZStd::string::format("Failed to create a Slang session (SlangResult 0x%08x)", static_cast<uint32_t>(result)));
        }

        return AZ::Success(AZStd::move(session));
    }

    AZ::Outcome<AZStd::vector<uint8_t>, AZStd::string> SlangCompilerService::CompileEntryPointFromSource(
        const SessionDescriptor& sessionDescriptor,
        const EntryPointCompileRequest& request)
    {
        AZStd::unique_lock<AZStd::recursive_mutex> compilerLock = AcquireCompilerLock();

        AZ::Outcome<Slang::ComPtr<slang::ISession>, AZStd::string> sessionOutcome = CreateSession(sessionDescriptor);
        if (!sessionOutcome.IsSuccess())
        {
            return AZ::Failure(sessionOutcome.TakeError());
        }
        Slang::ComPtr<slang::ISession> session = sessionOutcome.TakeValue();

        // Slang takes null-terminated strings; the request holds views.
        const AZStd::string moduleName(request.m_moduleName);
        const AZStd::string sourceCode(request.m_sourceCode);
        const AZStd::string entryPointName(request.m_entryPointName);

        Slang::ComPtr<slang::IBlob> diagnostics;
        slang::IModule* module = session->loadModuleFromSourceString(moduleName.c_str(), moduleName.c_str(), sourceCode.c_str(), diagnostics.writeRef());
        ReportDiagnostics(moduleName, diagnostics, module == nullptr);
        if (!module)
        {
            return AZ::Failure(AZStd::string::format("Failed to load Slang module '%s'", moduleName.c_str()));
        }

        Slang::ComPtr<slang::IEntryPoint> entryPoint;
        diagnostics = nullptr;
        module->findAndCheckEntryPoint(entryPointName.c_str(), request.m_stage, entryPoint.writeRef(), diagnostics.writeRef());
        ReportDiagnostics(moduleName, diagnostics, !entryPoint);
        if (!entryPoint)
        {
            return AZ::Failure(AZStd::string::format("Entry point '%s' was not found in Slang module '%s'", entryPointName.c_str(), moduleName.c_str()));
        }

        slang::IComponentType* components[] = {module, entryPoint.get()};
        Slang::ComPtr<slang::IComponentType> composite;
        diagnostics = nullptr;
        SlangResult result = session->createCompositeComponentType(components, AZ_ARRAY_SIZE(components), composite.writeRef(), diagnostics.writeRef());
        ReportDiagnostics(moduleName, diagnostics, SLANG_FAILED(result));
        if (SLANG_FAILED(result) || !composite)
        {
            return AZ::Failure(AZStd::string::format("Failed to compose Slang program for module '%s'", moduleName.c_str()));
        }

        Slang::ComPtr<slang::IComponentType> linkedProgram;
        diagnostics = nullptr;
        result = composite->link(linkedProgram.writeRef(), diagnostics.writeRef());
        ReportDiagnostics(moduleName, diagnostics, SLANG_FAILED(result));
        if (SLANG_FAILED(result) || !linkedProgram)
        {
            return AZ::Failure(AZStd::string::format("Failed to link Slang program for module '%s'", moduleName.c_str()));
        }

        Slang::ComPtr<slang::IBlob> code;
        diagnostics = nullptr;
        result = linkedProgram->getEntryPointCode(0, 0, code.writeRef(), diagnostics.writeRef());
        ReportDiagnostics(moduleName, diagnostics, SLANG_FAILED(result));
        if (SLANG_FAILED(result) || !code || code->getBufferSize() == 0)
        {
            return AZ::Failure(AZStd::string::format("Failed to generate target code for entry point '%s' of Slang module '%s'", entryPointName.c_str(), moduleName.c_str()));
        }

        const uint8_t* codeBegin = static_cast<const uint8_t*>(code->getBufferPointer());
        return AZ::Success(AZStd::vector<uint8_t>(codeBegin, codeBegin + code->getBufferSize()));
    }
} // namespace AZ::ShaderBuilder
