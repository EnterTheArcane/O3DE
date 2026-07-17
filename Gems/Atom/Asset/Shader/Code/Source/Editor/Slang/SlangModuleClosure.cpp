/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "SlangModuleClosure.h"

#include <AzCore/Serialization/SerializeContext.h>

#include <slang-com-ptr.h>

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
                ->Version(1)
                ->Field("schemaVersion", &SlangModuleClosureBundle::m_schemaVersion)
                ->Field("compilerBuildTag", &SlangModuleClosureBundle::m_compilerBuildTag)
                ->Field("targetFormat", &SlangModuleClosureBundle::m_targetFormat)
                ->Field("modules", &SlangModuleClosureBundle::m_modules)
                ;
        }
    }

    AZ::Outcome<SlangModuleClosureBundle, AZStd::string> BuildModuleClosureBundle(
        slang::ISession* session,
        AZStd::string_view compilerBuildTag,
        uint32_t targetFormat)
    {
        SlangModuleClosureBundle bundle;
        bundle.m_compilerBuildTag = compilerBuildTag;
        bundle.m_targetFormat = targetFormat;

        const SlangInt moduleCount = session->getLoadedModuleCount();
        for (SlangInt moduleIndex = 0; moduleIndex < moduleCount; ++moduleIndex)
        {
            slang::IModule* module = session->getLoadedModule(moduleIndex);

            Slang::ComPtr<ISlangBlob> moduleBlob;
            if (SLANG_FAILED(module->serialize(moduleBlob.writeRef())) || !moduleBlob)
            {
                return AZ::Failure(AZStd::string::format("Failed to serialize Slang module %s", module->getName()));
            }

            SlangModuleClosureBundle::Module& bundleModule = bundle.m_modules.emplace_back();
            bundleModule.m_name = module->getName();
            if (const char* modulePath = module->getFilePath())
            {
                bundleModule.m_path = modulePath;
            }
            const uint8_t* bytes = static_cast<const uint8_t*>(moduleBlob->getBufferPointer());
            bundleModule.m_serializedModule.assign(bytes, bytes + moduleBlob->getBufferSize());
        }

        return AZ::Success(AZStd::move(bundle));
    }
} // namespace AZ::ShaderBuilder
