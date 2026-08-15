/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Atom/RHI.Interface/Metal/RHIMetalInterface.h>

#include <RHI/Buffer.h>
#include <RHI/Device.h>

namespace AZ::Metal
{
    void* GetDeviceNativeHandle(RHI::Device& device)
    {
        Device* metalDevice = azrtti_cast<Device*>(&device);
        AZ_Assert(metalDevice, "%s can only be called with a Metal RHI object", __FUNCTION__);
        if (!metalDevice)
        {
            return nullptr;
        }

        return metalDevice->GetMtlDevice();
    }

    void* GetBufferNativeHandle(RHI::DeviceBuffer& buffer)
    {
        Buffer* metalBuffer = azrtti_cast<Buffer*>(&buffer);
        AZ_Assert(metalBuffer, "%s can only be called with a Metal RHI object", __FUNCTION__);
        if (!metalBuffer)
        {
            return nullptr;
        }

        return metalBuffer->GetMemoryView().GetGpuAddress<id<MTLBuffer>>();
    }

    size_t GetBufferAllocationOffset(RHI::DeviceBuffer& buffer)
    {
        Buffer* metalBuffer = azrtti_cast<Buffer*>(&buffer);
        AZ_Assert(metalBuffer, "%s can only be called with a Metal RHI object", __FUNCTION__);
        if (!metalBuffer)
        {
            return 0;
        }

        return metalBuffer->GetMemoryView().GetOffset();
    }
} // namespace AZ::Metal
