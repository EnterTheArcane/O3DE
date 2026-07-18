/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "SlangModuleClosure.h"

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/parallel/atomic.h>

#include <slang-com-ptr.h>

#include <Slang/SlangCompilerService.h>

namespace AZ::ShaderBuilder
{
    void SlangModuleClosureBundle::Reflect(ReflectContext* context)
    {
        if (SerializeContext* serializeContext = azrtti_cast<SerializeContext*>(context))
        {
            serializeContext->Class<Module>()
                ->Version(1)
                ->Field("name", &Module::m_name)
                ->Field("path", &Module::m_path)
                ->Field("serializedModule", &Module::m_serializedModule)
                ;

            serializeContext->Class<SlangModuleClosureBundle>()
                ->Version(2)
                ->Field("schemaVersion", &SlangModuleClosureBundle::m_schemaVersion)
                ->Field("compilerBuildTag", &SlangModuleClosureBundle::m_compilerBuildTag)
                ->Field("targetFormat", &SlangModuleClosureBundle::m_targetFormat)
                ->Field("rootModuleName", &SlangModuleClosureBundle::m_rootModuleName)
                ->Field("modules", &SlangModuleClosureBundle::m_modules)
                ;
        }
    }

    AZ::Outcome<SlangModuleClosureBundle, AZStd::string> BuildModuleClosureBundle(
        slang::ISession* session,
        AZStd::string_view compilerBuildTag,
        uint32_t targetFormat,
        AZStd::string_view rootModuleName,
        AZStd::span<const AZStd::string_view> excludedModuleNames)
    {
        SlangModuleClosureBundle bundle;
        bundle.m_compilerBuildTag = compilerBuildTag;
        bundle.m_targetFormat = targetFormat;
        bundle.m_rootModuleName = rootModuleName;

        bool rootModuleFound = false;
        const SlangInt moduleCount = session->getLoadedModuleCount();
        for (SlangInt moduleIndex = 0; moduleIndex < moduleCount; ++moduleIndex)
        {
            slang::IModule* module = session->getLoadedModule(moduleIndex);
            const AZStd::string_view moduleName = module->getName();
            if (AZStd::find(excludedModuleNames.begin(), excludedModuleNames.end(), moduleName) != excludedModuleNames.end())
            {
                continue;
            }

            Slang::ComPtr<ISlangBlob> moduleBlob;
            if (SLANG_FAILED(module->serialize(moduleBlob.writeRef())) || !moduleBlob)
            {
                return AZ::Failure(AZStd::string::format("Failed to serialize Slang module %s", module->getName()));
            }

            SlangModuleClosureBundle::Module& bundleModule = bundle.m_modules.emplace_back();
            bundleModule.m_name = moduleName;
            if (const char* modulePath = module->getFilePath())
            {
                bundleModule.m_path = modulePath;
            }
            const uint8_t* bytes = static_cast<const uint8_t*>(moduleBlob->getBufferPointer());
            bundleModule.m_serializedModule.assign(bytes, bytes + moduleBlob->getBufferSize());
            rootModuleFound = rootModuleFound || moduleName == rootModuleName;
        }

        if (!rootModuleFound)
        {
            return AZ::Failure(AZStd::string::format(
                "The session has no module named %.*s to serve as the closure root", AZ_STRING_ARG(rootModuleName)));
        }
        return AZ::Success(AZStd::move(bundle));
    }

    AZ::Outcome<void, AZStd::string> ValidateModuleClosureBundle(
        const SlangModuleClosureBundle& bundle,
        AZStd::string_view compilerBuildTag,
        uint32_t targetFormat)
    {
        if (bundle.m_schemaVersion != SlangModuleClosureBundle::CurrentSchemaVersion)
        {
            return AZ::Failure(AZStd::string::format(
                "Module closure schema %u does not match the current schema %u",
                bundle.m_schemaVersion, SlangModuleClosureBundle::CurrentSchemaVersion));
        }
        if (bundle.m_compilerBuildTag != compilerBuildTag)
        {
            return AZ::Failure(AZStd::string::format(
                "Module closure was produced by compiler '%s'; the running compiler is '%.*s'",
                bundle.m_compilerBuildTag.c_str(), AZ_STRING_ARG(compilerBuildTag)));
        }
        if (bundle.m_targetFormat != targetFormat)
        {
            return AZ::Failure(AZStd::string::format(
                "Module closure targets format %u; this compile targets %u", bundle.m_targetFormat, targetFormat));
        }
        if (bundle.m_rootModuleName.empty() || bundle.m_modules.empty())
        {
            return AZ::Failure(AZStd::string("Module closure names no root module"));
        }
        return AZ::Success();
    }

    namespace
    {
        //! Wraps one bundle module's bytes as an ISlangBlob for loadModuleFromIRBlob. Owns a
        //! copy: the compiler may retain the blob beyond the restoring call.
        class SerializedModuleBlob final : public ISlangBlob
        {
        public:
            explicit SerializedModuleBlob(const AZStd::vector<uint8_t>& bytes)
                : m_bytes(bytes)
            {
            }

            SLANG_NO_THROW SlangResult SLANG_MCALL queryInterface(const SlangUUID& uuid, void** outObject) override
            {
                auto isUuidEqual = [](const SlangUUID& lhs, const SlangUUID& rhs)
                {
                    return memcmp(&lhs, &rhs, sizeof(SlangUUID)) == 0;
                };
                if (isUuidEqual(uuid, ISlangUnknown::getTypeGuid()) || isUuidEqual(uuid, ISlangBlob::getTypeGuid()))
                {
                    addRef();
                    *outObject = static_cast<ISlangBlob*>(this);
                    return SLANG_OK;
                }
                *outObject = nullptr;
                return SLANG_E_NO_INTERFACE;
            }

            SLANG_NO_THROW uint32_t SLANG_MCALL addRef() override
            {
                return ++m_referenceCount;
            }

            SLANG_NO_THROW uint32_t SLANG_MCALL release() override
            {
                const uint32_t remaining = --m_referenceCount;
                if (remaining == 0)
                {
                    delete this;
                }
                return remaining;
            }

            SLANG_NO_THROW const void* SLANG_MCALL getBufferPointer() override
            {
                return m_bytes.data();
            }

            SLANG_NO_THROW size_t SLANG_MCALL getBufferSize() override
            {
                return m_bytes.size();
            }

        private:
            AZStd::vector<uint8_t> m_bytes;
            AZStd::atomic<uint32_t> m_referenceCount{1};
        };
    } // namespace

    AZ::Outcome<slang::IModule*, AZStd::string> RestoreModuleClosure(
        slang::ISession* session,
        const SlangModuleClosureBundle& bundle)
    {
        slang::IModule* rootModule = nullptr;
        for (const SlangModuleClosureBundle::Module& bundleModule : bundle.m_modules)
        {
            Slang::ComPtr<slang::IBlob> moduleBlob;
            moduleBlob.attach(new SerializedModuleBlob(bundleModule.m_serializedModule));
            Slang::ComPtr<slang::IBlob> diagnostics;
            slang::IModule* restoredModule = session->loadModuleFromIRBlob(
                bundleModule.m_name.c_str(),
                bundleModule.m_path.empty() ? bundleModule.m_name.c_str() : bundleModule.m_path.c_str(),
                moduleBlob,
                diagnostics.writeRef());
            SlangCompilerService::ReportDiagnostics(bundleModule.m_name, diagnostics, !restoredModule);
            if (!restoredModule)
            {
                return AZ::Failure(AZStd::string::format(
                    "Failed to restore Slang module %s from the closure bundle", bundleModule.m_name.c_str()));
            }
            if (bundleModule.m_name == bundle.m_rootModuleName)
            {
                rootModule = restoredModule;
            }
        }

        if (!rootModule)
        {
            return AZ::Failure(AZStd::string::format(
                "The closure bundle has no module named %s to serve as the root", bundle.m_rootModuleName.c_str()));
        }
        return AZ::Success(AZStd::move(rootModule));
    }
} // namespace AZ::ShaderBuilder
