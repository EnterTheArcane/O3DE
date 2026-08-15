/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/PlatformHairComputeProvider.h>

#include <Jolt/PlatformHairRenderBuffers.h>

#include <Atom/RHI/Factory.h>
#include <Atom/RPI.Public/Buffer/BufferSystemInterface.h>

#if defined(JOLT_GPU_HAIR_DX12)
#include <Atom/RHI.Reflect/DX12/Base.h>
#include <Jolt/Platform/Windows/Dx12HairComputeSystem.h>
#endif

#if defined(JOLT_GPU_HAIR_VULKAN)
#include <Atom/RHI.Reflect/Vulkan/Base.h>
#include <Jolt/Platform/Vulkan/HairComputeSystem.h>
#endif

#if defined(JOLT_GPU_HAIR_METAL)
#include <Atom/RHI.Reflect/Metal/Base.h>
#include <Jolt/Platform/Metal/HairComputeSystem.h>
#endif

namespace Jolt
{
    HairComputeBackend PlatformHairComputeProvider::GetBackend() const
    {
        return HairComputeBackend::PlatformGpu;
    }

    JPH::ComputeSystemResult PlatformHairComputeProvider::CreateComputeSystem()
    {
        JPH::ComputeSystemResult result;
        if (!AZ::RHI::Factory::IsReady())
        {
            result.SetError("The renderer is not initialized.");
            return result;
        }

        const AZ::RHI::APIType apiType = AZ::RHI::Factory::Get().GetType();
#if defined(JOLT_GPU_HAIR_DX12)
        if (apiType == AZ::DX12::RHIType)
        {
            return Platform::Dx12::CreateHairComputeSystem();
        }
#endif

#if defined(JOLT_GPU_HAIR_VULKAN)
        if (apiType == AZ::Vulkan::RHIType)
        {
            return Platform::Vulkan::CreateHairComputeSystem();
        }
#endif

#if defined(JOLT_GPU_HAIR_METAL)
        if (apiType == AZ::Metal::RHIType)
        {
            return Platform::Metal::CreateHairComputeSystem();
        }
#endif

        result.SetError("The active renderer has no Jolt GPU hair adapter.");
        return result;
    }

    AZStd::unique_ptr<IHairRenderBuffers> PlatformHairComputeProvider::CreateRenderBuffers(
        JPH::ComputeSystem& computeSystem,
        const AZ::u32 vertexCount,
        const AZ::u32 stride)
    {
        if (!AZ::RPI::BufferSystemInterface::Get())
        {
            return {};
        }

        return CreatePlatformHairRenderBuffers(
            computeSystem,
            vertexCount,
            stride);
    }
} // namespace Jolt
