/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/Platform/Vulkan/HairComputeSystem.h>

#include <Jolt/GpuHairShaderLoader.h>

#include <Atom/RHI/RHISystemInterface.h>
#include <Atom/RHI.Interface/Vulkan/RHIVulkanInterface.h>

#include <Jolt/Compute/VK/ComputeSystemVKWithAllocator.h>
#include <Jolt/Compute/VK/ComputeBufferVK.h>

namespace Jolt::Platform::Vulkan
{
    namespace
    {
        class ComputeSystem final
            : public JPH::ComputeSystemVKWithAllocator
        {
        public:
            ~ComputeSystem() override
            {
                Shutdown(false);
            }
        };

        VkResult SubmitCommandBuffers(
            void* userData,
            [[maybe_unused]] const VkQueue queue,
            const JPH::uint32 submitCount,
            const VkSubmitInfo* submitInfos,
            const VkFence fence)
        {
            AZ::RHI::Device* device = static_cast<AZ::RHI::Device*>(userData);
            if (!device)
            {
                return VK_ERROR_DEVICE_LOST;
            }

            return AZ::Vulkan::SubmitCommandBuffers(
                *device,
                AZ::RHI::HardwareQueueClass::Compute,
                submitCount,
                submitInfos,
                fence);
        }
    } // namespace

    JPH::ComputeSystemResult CreateHairComputeSystem()
    {
        JPH::ComputeSystemResult result;
        AZ::RHI::RHISystemInterface* rhiSystem = AZ::RHI::RHISystemInterface::Get();
        if (!rhiSystem)
        {
            result.SetError("The renderer is not initialized.");
            return result;
        }

        AZ::RHI::Device* device = rhiSystem->GetDevice();
        if (!device)
        {
            result.SetError("The renderer has no default device.");
            return result;
        }

        const VkInstance instance = AZ::Vulkan::GetInstanceNativeHandle();
        const VkPhysicalDevice physicalDevice = AZ::Vulkan::GetPhysicalDeviceNativeHandle(device->GetPhysicalDevice());
        const VkDevice nativeDevice = AZ::Vulkan::GetDeviceNativeHandle(*device);
        const PFN_vkGetInstanceProcAddr getInstanceProcAddr = AZ::Vulkan::GetInstanceProcAddr();
        const PFN_vkGetDeviceProcAddr getDeviceProcAddr = AZ::Vulkan::GetDeviceProcAddr(*device);
        if (!instance || !physicalDevice || !nativeDevice || !getInstanceProcAddr || !getDeviceProcAddr)
        {
            result.SetError("The active Vulkan renderer has incomplete native device state.");
            return result;
        }

        const uint32_t computeQueueFamily = AZ::Vulkan::GetCommandQueueFamilyIndex(
            *device,
            AZ::RHI::HardwareQueueClass::Compute);
        JPH::Ref<ComputeSystem> computeSystem = new ComputeSystem();
        if (!computeSystem->Initialize(
                instance,
                physicalDevice,
                getInstanceProcAddr,
                getDeviceProcAddr,
                nativeDevice,
                computeQueueFamily,
                result))
        {
            return result;
        }

        computeSystem->SetQueueSubmitFunction(SubmitCommandBuffers, device);
        computeSystem->mShaderLoader = LoadGpuHairShader;
        result.Set(computeSystem.GetPtr());
        return result;
    }

    JPH::ComputeBufferResult CreateHairRenderBuffer(
        JPH::ComputeSystem& computeSystem,
        AZ::RHI::DeviceBuffer& buffer,
        const AZ::u32 vertexCount,
        const AZ::u32 stride)
    {
        JPH::ComputeBufferResult result;
        const VkBuffer nativeBuffer = AZ::Vulkan::GetNativeBuffer(buffer);
        if (!nativeBuffer)
        {
            result.SetError("The renderer-owned Vulkan buffer has no native resource.");
            return result;
        }

        AZ::Vulkan::SetBufferStateAfterExternalAccess(
            buffer,
            AZ::RHI::HardwareQueueClass::Compute,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            VK_ACCESS_NONE);

        auto* vulkanComputeSystem = static_cast<JPH::ComputeSystemVK*>(&computeSystem);
        JPH::Ref<JPH::ComputeBufferVK> computeBuffer = new JPH::ComputeBufferVK(
            vulkanComputeSystem,
            JPH::ComputeBuffer::EType::RWBuffer,
            vertexCount,
            stride);
        if (!computeBuffer->InitializeExternal(
                nativeBuffer,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                static_cast<VkAccessFlagBits>(VK_ACCESS_NONE)))
        {
            result.SetError("Failed to wrap the renderer-owned Vulkan buffer.");
            return result;
        }

        result.Set(computeBuffer.GetPtr());
        return result;
    }

    void SynchronizeHairRenderBufferAfterUpdate(AZ::RHI::DeviceBuffer& buffer)
    {
        AZ::Vulkan::SetBufferStateAfterExternalAccess(
            buffer,
            AZ::RHI::HardwareQueueClass::Compute,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    }
} // namespace Jolt::Platform::Vulkan
