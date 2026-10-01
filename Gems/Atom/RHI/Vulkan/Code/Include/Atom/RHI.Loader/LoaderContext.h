/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <vulkan/vulkan.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/std/containers/unordered_set.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzCore/std/string/string.h>

namespace AZ
{
    namespace Vulkan
    {
        //! Utility class for loading the Vulkan function pointers using volk.
        //! Device-level entry points are stored in a per-device dispatch table so that
        //! multiple devices can be driven without going through the loader trampoline.
        //! Instance and physical-device entry points are loaded as volk global functions.
        class LoaderContext
        {
        public:
            //! Parameters for initializing the function loader
            struct Descriptor
            {
                VkInstance m_instance = VK_NULL_HANDLE;
                VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
                VkDevice m_device = VK_NULL_HANDLE;
                AZStd::vector<const char*> m_loadedLayers;
                AZStd::vector<const char*> m_loadedExtensions;
            };

            //! Creates a new instance of a loader context.
            static AZStd::unique_ptr<LoaderContext> Create();

            ~LoaderContext();

            //! Loads the function pointers using the instance, device and physical device provided.
            bool Init(const Descriptor& descriptor);
            //! Shutdown the loader context.
            void Shutdown();
            //! Returns a list of available instance layers.
            AZStd::vector<AZStd::string> GetInstanceLayerNames() const;
            //! Returns a list of available instance extensions.
            AZStd::vector<AZStd::string> GetInstanceExtensionNames(const char* layerName = nullptr) const;
            //! Returns the device dispatch table with the loaded device-level function pointers.
            const VolkDeviceTable& GetContext() const;
            //! Returns whether the given instance or device extension was enabled when this context was loaded.
            bool IsExtensionEnabled(const char* extensionName) const;

        private:
            LoaderContext() = default;

            //! Loads the Vulkan loader library and the global function pointers.
            bool Preload();
            //! Removes extensions whose entry points failed to load.
            void FilterAvailableExtensions();

            VolkDeviceTable m_context = {};
            AZStd::unordered_set<AZStd::string> m_enabledExtensions;
        };
    } // namespace Vulkan
} // namespace AZ
