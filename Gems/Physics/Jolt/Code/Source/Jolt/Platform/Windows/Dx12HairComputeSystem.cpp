/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/Platform/Windows/Dx12HairComputeSystem.h>

#include <Jolt/GpuHairShaderLoader.h>

#include <Atom/RHI/RHISystemInterface.h>
#include <Atom/RHI.Interface/DX12/RHIDX12Interface.h>

#include <Jolt/Compute/DX12/ComputeSystemDX12.h>
#include <Jolt/Compute/DX12/ComputeBufferDX12.h>

namespace Jolt::Platform::Dx12
{
    namespace
    {
        class ComputeSystem final
            : public JPH::ComputeSystemDX12
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

        ID3D12Device5* nativeDevice = AZ::DX12::GetDeviceNativeHandle(*device);
        if (!nativeDevice)
        {
            result.SetError("The active renderer is not using the DX12 backend.");
            return result;
        }

        return CreateHairComputeSystem(*nativeDevice);
    }

    JPH::ComputeSystemResult CreateHairComputeSystem(ID3D12Device5& device)
    {
        JPH::ComputeSystemResult result;
        JPH::Ref<ComputeSystem> computeSystem = new ComputeSystem();
        if (!computeSystem->Initialize(&device, result))
        {
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
        ID3D12Resource* resource = AZ::DX12::GetBufferResource(buffer);
        if (!resource)
        {
            result.SetError("The renderer-owned DX12 buffer has no native resource.");
            return result;
        }

        auto* dx12ComputeSystem = static_cast<JPH::ComputeSystemDX12*>(&computeSystem);
        JPH::Ref<JPH::ComputeBufferDX12> computeBuffer = new JPH::ComputeBufferDX12(
            dx12ComputeSystem,
            JPH::ComputeBuffer::EType::RWBuffer,
            vertexCount,
            stride);
        if (!computeBuffer->InitializeExternal(resource, D3D12_RESOURCE_STATE_COMMON))
        {
            result.SetError("Failed to wrap the renderer-owned DX12 buffer.");
            return result;
        }

        result.Set(computeBuffer.GetPtr());
        return result;
    }

    void SynchronizeHairRenderBufferAfterUpdate(AZ::RHI::DeviceBuffer& buffer)
    {
        AZ::DX12::SetBufferStateAfterExternalAccess(
            buffer,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }
} // namespace Jolt::Platform::Dx12
