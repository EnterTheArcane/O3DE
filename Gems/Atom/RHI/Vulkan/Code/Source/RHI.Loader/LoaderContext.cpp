/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <vulkan/vulkan.h>

#include <Atom/RHI.Loader/LoaderContext.h>

namespace AZ
{
    namespace Vulkan
    {
        AZStd::unique_ptr<LoaderContext> LoaderContext::Create()
        {
            AZStd::unique_ptr<LoaderContext> loader(aznew LoaderContext());
            if (!loader->Preload())
            {
                return nullptr;
            }
            return loader;
        }

        LoaderContext::~LoaderContext()
        {
            Shutdown();
        }

        bool LoaderContext::Init(const LoaderContext::Descriptor& descriptor)
        {
            m_enabledExtensions.clear();
            for (const char* extension : descriptor.m_loadedExtensions)
            {
                m_enabledExtensions.emplace(extension);
            }

            if (descriptor.m_device != VK_NULL_HANDLE)
            {
                volkLoadDeviceTable(&m_context, descriptor.m_device);
                FilterAvailableExtensions();
            }
            else if (descriptor.m_instance != VK_NULL_HANDLE)
            {
                volkLoadInstanceOnly(descriptor.m_instance);
            }
            return true;
        }

        void LoaderContext::Shutdown()
        {
            m_context = {};
            m_enabledExtensions.clear();
        }

        bool LoaderContext::Preload()
        {
            return volkInitialize() == VK_SUCCESS;
        }

        AZStd::vector<AZStd::string> LoaderContext::GetInstanceLayerNames() const
        {
            AZStd::vector<AZStd::string> layerNames;
            uint32_t layerPropertyCount = 0;
            VkResult result = vkEnumerateInstanceLayerProperties(&layerPropertyCount, nullptr);
            if (result != VK_SUCCESS || layerPropertyCount == 0)
            {
                return layerNames;
            }

            AZStd::vector<VkLayerProperties> layerProperties(layerPropertyCount);
            result = vkEnumerateInstanceLayerProperties(&layerPropertyCount, layerProperties.data());
            if (result != VK_SUCCESS)
            {
                return layerNames;
            }

            layerNames.reserve(layerNames.size() + layerProperties.size());
            for (uint32_t layerPropertyIndex = 0; layerPropertyIndex < layerPropertyCount; ++layerPropertyIndex)
            {
                layerNames.emplace_back(layerProperties[layerPropertyIndex].layerName);
            }

            return layerNames;
        }   

        AZStd::vector<AZStd::string> LoaderContext::GetInstanceExtensionNames(const char* layerName /*= nullptr*/) const
        {
            AZStd::vector<AZStd::string> extensionNames;
            uint32_t extPropertyCount = 0;
            VkResult result = vkEnumerateInstanceExtensionProperties(layerName, &extPropertyCount, nullptr);
            if (result != VK_SUCCESS || extPropertyCount == 0)
            {
                return extensionNames;
            }

            AZStd::vector<VkExtensionProperties> extProperties;
            extProperties.resize(extPropertyCount);

            result = vkEnumerateInstanceExtensionProperties(layerName, &extPropertyCount, extProperties.data());
            if (result != VK_SUCCESS)
            {
                return extensionNames;
            }

            extensionNames.reserve(extensionNames.size() + extProperties.size());
            for (uint32_t extPropertyIndex = 0; extPropertyIndex < extPropertyCount; extPropertyIndex++)
            {
                extensionNames.emplace_back(extProperties[extPropertyIndex].extensionName);
            }

            return extensionNames;
        }

        const VolkDeviceTable& LoaderContext::GetContext() const
        {
            return m_context;
        }

        bool LoaderContext::IsExtensionEnabled(const char* extensionName) const
        {
            return m_enabledExtensions.find(extensionName) != m_enabledExtensions.end();
        }

        void LoaderContext::FilterAvailableExtensions()
        {
            // In some cases (like when running with the GPU profiler on Quest2) the extension is reported as available
            // but the function pointers do not load. Disable the extension if that's the case.
            if (m_enabledExtensions.find(VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != m_enabledExtensions.end() &&
                vkCmdBeginDebugUtilsLabelEXT == nullptr)
            {
                m_enabledExtensions.erase(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            }
        }

    } // namespace Vulkan
} // namespace AZ
