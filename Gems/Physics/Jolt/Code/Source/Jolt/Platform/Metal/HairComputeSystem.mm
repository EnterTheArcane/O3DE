/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/Platform/Metal/HairComputeSystem.h>

#include <Jolt/GpuHairShaderLoader.h>

#include <Atom/RHI/RHISystemInterface.h>
#include <Atom/RHI.Interface/Metal/RHIMetalInterface.h>

#include <Jolt/Compute/MTL/ComputeSystemMTL.h>
#include <Jolt/Compute/MTL/ComputeBufferMTL.h>

namespace Jolt::Platform::Metal
{
    namespace
    {
        class ComputeSystem final
            : public JPH::ComputeSystemMTL
        {
        public:
            ~ComputeSystem() override
            {
                Shutdown();
            }
        };
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

        void* nativeDevice = AZ::Metal::GetDeviceNativeHandle(*device);
        if (!nativeDevice)
        {
            result.SetError("The active renderer is not using the Metal backend.");
            return result;
        }

        return CreateHairComputeSystem(nativeDevice);
    }

    JPH::ComputeSystemResult CreateHairComputeSystem(void* device)
    {
        JPH::ComputeSystemResult result;
        if (!device)
        {
            result.SetError("The Metal device is invalid.");
            return result;
        }

        JPH::Ref<ComputeSystem> computeSystem = new ComputeSystem();
        if (!computeSystem->Initialize(static_cast<id<MTLDevice>>(device)))
        {
            result.SetError("Failed to initialize the Metal compute system.");
            return result;
        }

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
        if (AZ::Metal::GetBufferAllocationOffset(buffer) != 0)
        {
            result.SetError("Renderer-owned Metal hair buffers must use unique allocations.");
            return result;
        }

        id<MTLBuffer> nativeBuffer = static_cast<id<MTLBuffer>>(AZ::Metal::GetBufferNativeHandle(buffer));
        if (nativeBuffer == nil)
        {
            result.SetError("The renderer-owned Metal buffer has no native resource.");
            return result;
        }

        auto* metalComputeSystem = static_cast<JPH::ComputeSystemMTL*>(&computeSystem);
        JPH::Ref<JPH::ComputeBufferMTL> computeBuffer = new JPH::ComputeBufferMTL(
            metalComputeSystem,
            JPH::ComputeBuffer::EType::RWBuffer,
            vertexCount,
            stride);
        if (!computeBuffer->InitializeExternal(nativeBuffer))
        {
            result.SetError("Failed to wrap the renderer-owned Metal buffer.");
            return result;
        }

        result.Set(computeBuffer.GetPtr());
        return result;
    }

    void SynchronizeHairRenderBufferAfterUpdate([[maybe_unused]] AZ::RHI::DeviceBuffer& buffer)
    {
    }
} // namespace Jolt::Platform::Metal
