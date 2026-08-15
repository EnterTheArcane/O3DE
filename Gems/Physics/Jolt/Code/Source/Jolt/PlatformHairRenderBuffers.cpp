/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/PlatformHairRenderBuffers.h>

#include <Atom/RHI/Buffer.h>
#include <Atom/RHI/DeviceBuffer.h>
#include <Atom/RHI/DeviceFence.h>
#include <Atom/RHI/Factory.h>
#include <Atom/RHI/Fence.h>
#include <Atom/RHI/FrameGraphBuilder.h>
#include <Atom/RHI/FrameGraphInterface.h>
#include <Atom/RHI/RHISystemInterface.h>
#include <Atom/RHI/ScopeProducer.h>
#include <Atom/RHI.Reflect/BufferScopeAttachmentDescriptor.h>
#include <Atom/RPI.Public/Buffer/Buffer.h>
#include <Atom/RPI.Public/Buffer/BufferSystemInterface.h>

#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/Debug/Trace.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/limits.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/utility/move.h>

#include <cstring>

#include <Jolt/Compute/ComputeBuffer.h>
#include <Jolt/Physics/Hair/Hair.h>

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
    namespace
    {
        constexpr AZ::u32 RenderBufferCount = 3;
        constexpr int DeviceIndex = AZ::RHI::MultiDevice::DefaultDeviceIndex;

        enum class RenderBufferState : AZ::u8
        {
            Available,
            Acquired,
            PendingHandoff,
            Ready,
            Updating,
        };

        class HandoffScope final
            : public AZ::RHI::ScopeProducer
        {
        public:
            HandoffScope(
                const AZ::RHI::ScopeId& scopeId,
                AZ::RHI::AttachmentId attachmentId,
                AZ::RHI::Fence& fence)
                : ScopeProducer(scopeId, DeviceIndex)
                , m_attachmentId(AZStd::move(attachmentId))
                , m_fence(fence)
            {
                SetHardwareQueueClass(AZ::RHI::HardwareQueueClass::Compute);
            }

        private:
            void SetupFrameGraphDependencies(AZ::RHI::FrameGraphInterface frameGraph) override
            {
                const AZ::RHI::BufferScopeAttachmentDescriptor descriptor{m_attachmentId};
                [[maybe_unused]] const AZ::RHI::ResultCode result = frameGraph.UseShaderAttachment(
                    descriptor,
                    AZ::RHI::ScopeAttachmentAccess::ReadWrite,
                    AZ::RHI::ScopeAttachmentStage::ComputeShader);
                AZ_Assert(result == AZ::RHI::ResultCode::Success, "Failed to return a hair buffer to compute ownership.");
                frameGraph.SignalFence(m_fence);
            }

            AZ::RHI::AttachmentId m_attachmentId;
            AZ::RHI::Fence& m_fence;
        };

        struct RenderBufferEntry final
        {
            AZ::Data::Instance<AZ::RPI::Buffer> m_buffer;
            JPH::Ref<JPH::ComputeBuffer> m_computeBuffer;
            AZ::RHI::Ptr<AZ::RHI::Fence> m_fence;
            AZStd::unique_ptr<HandoffScope> m_handoffScope;
            AZ::u64 m_token = 0;
            RenderBufferState m_state = RenderBufferState::Available;
        };

        [[nodiscard]]
        JPH::ComputeBufferResult CreateNativeRenderBuffer(
            JPH::ComputeSystem& computeSystem,
            AZ::RHI::DeviceBuffer& buffer,
            const AZ::u32 vertexCount,
            const AZ::u32 stride)
        {
            const AZ::RHI::APIType apiType = AZ::RHI::Factory::Get().GetType();
#if defined(JOLT_GPU_HAIR_DX12)
            if (apiType == AZ::DX12::RHIType)
            {
                return Platform::Dx12::CreateHairRenderBuffer(
                    computeSystem,
                    buffer,
                    vertexCount,
                    stride);
            }
#endif

#if defined(JOLT_GPU_HAIR_VULKAN)
            if (apiType == AZ::Vulkan::RHIType)
            {
                return Platform::Vulkan::CreateHairRenderBuffer(
                    computeSystem,
                    buffer,
                    vertexCount,
                    stride);
            }
#endif

#if defined(JOLT_GPU_HAIR_METAL)
            if (apiType == AZ::Metal::RHIType)
            {
                return Platform::Metal::CreateHairRenderBuffer(
                    computeSystem,
                    buffer,
                    vertexCount,
                    stride);
            }
#endif

            JPH::ComputeBufferResult result;
            result.SetError("The active renderer has no hair render-buffer adapter.");
            return result;
        }

        void SynchronizeNativeRenderBufferAfterUpdate(AZ::RHI::DeviceBuffer& buffer)
        {
            const AZ::RHI::APIType apiType = AZ::RHI::Factory::Get().GetType();
#if defined(JOLT_GPU_HAIR_DX12)
            if (apiType == AZ::DX12::RHIType)
            {
                Platform::Dx12::SynchronizeHairRenderBufferAfterUpdate(buffer);
                return;
            }
#endif

#if defined(JOLT_GPU_HAIR_VULKAN)
            if (apiType == AZ::Vulkan::RHIType)
            {
                Platform::Vulkan::SynchronizeHairRenderBufferAfterUpdate(buffer);
                return;
            }
#endif

#if defined(JOLT_GPU_HAIR_METAL)
            if (apiType == AZ::Metal::RHIType)
            {
                Platform::Metal::SynchronizeHairRenderBufferAfterUpdate(buffer);
            }
#endif
        }

        class PlatformHairRenderBuffers final
            : public IHairRenderBuffers
        {
        public:
            PlatformHairRenderBuffers(
                JPH::ComputeSystem& computeSystem,
                const AZ::u32 vertexCount,
                const AZ::u32 stride)
                : m_computeSystem(computeSystem)
                , m_vertexCount(vertexCount)
                , m_stride(stride)
            {
            }

            bool Initialize(JPH::Hair& hair) override
            {
                AZ::RPI::BufferSystemInterface* bufferSystem = AZ::RPI::BufferSystemInterface::Get();
                if (!bufferSystem || m_vertexCount == 0 || m_stride == 0)
                {
                    return false;
                }

                for (AZ::u32 index = 0; index < RenderBufferCount; ++index)
                {
                    RenderBufferEntry& entry = m_entries[index];
                    const AZStd::string bufferName = AZStd::string::format(
                        "Jolt Hair Render Positions %p %u",
                        this,
                        index);
                    const AZ::RPI::CommonBufferDescriptor descriptor = {
                        .m_bufferName = bufferName,
                        .m_poolType = AZ::RPI::CommonBufferPoolType::ReadWrite,
                        .m_elementSize = m_stride,
                        .m_byteCount = aznumeric_cast<AZ::u64>(m_vertexCount) * m_stride,
                        .m_ownerDeviceIndex = DeviceIndex,
                    };
                    entry.m_buffer = bufferSystem->CreateBufferFromCommonPool(descriptor);
                    if (!entry.m_buffer)
                    {
                        return false;
                    }

                    AZ::RHI::Buffer* rhiBuffer = entry.m_buffer->GetRHIBuffer();
                    AZ::RHI::Ptr<AZ::RHI::DeviceBuffer> deviceBuffer = rhiBuffer->GetDeviceBuffer(DeviceIndex);
                    if (!deviceBuffer)
                    {
                        return false;
                    }

                    JPH::ComputeBufferResult computeBufferResult = CreateNativeRenderBuffer(
                        m_computeSystem,
                        *deviceBuffer,
                        m_vertexCount,
                        m_stride);
                    if (computeBufferResult.HasError())
                    {
                        AZ_Error(
                            "Jolt",
                            false,
                            "Failed to wrap a renderer-owned hair buffer: %s",
                            computeBufferResult.GetError().c_str());
                        return false;
                    }
                    entry.m_computeBuffer = computeBufferResult.Get();

                    entry.m_fence = aznew AZ::RHI::Fence;
                    if (entry.m_fence->Init(
                            rhiBuffer->GetDeviceMask(),
                            AZ::RHI::FenceState::Signaled) != AZ::RHI::ResultCode::Success)
                    {
                        return false;
                    }

                    entry.m_handoffScope = AZStd::make_unique<HandoffScope>(
                        AZ::RHI::ScopeId{AZStd::string::format("Jolt Hair Handoff %p %u", this, index)},
                        entry.m_buffer->GetAttachmentId(),
                        *entry.m_fence);
                }

                hair.OverrideRenderPositionsCB(
                    [](JPH::ComputeBuffer* sourceBuffer, JPH::Float3* positions, const JPH::uint vertexCount)
                    {
                        const JPH::Float3* source = sourceBuffer->Map<JPH::Float3>(JPH::ComputeBuffer::EMode::Read);
                        if (source)
                        {
                            memcpy(positions, source, sizeof(JPH::Float3) * vertexCount);
                        }
                        sourceBuffer->Unmap();
                    });
                hair.SetRenderPositionsCB(m_entries[0].m_computeBuffer);
                m_currentIndex = 0;
                return true;
            }

            bool BeginUpdate(JPH::Hair& hair) override
            {
                if (m_updateIndex != InvalidIndex)
                {
                    return false;
                }

                AZ::u32 selectedIndex = InvalidIndex;
                AZ::u32 index = m_nextIndex;
                for (AZ::u32 attempt = 0; attempt < RenderBufferCount; ++attempt)
                {
                    const RenderBufferState state = m_entries[index].m_state;
                    if (state == RenderBufferState::Available
                        || state == RenderBufferState::Ready)
                    {
                        selectedIndex = index;
                        break;
                    }

                    ++index;
                    if (index == RenderBufferCount)
                    {
                        index = 0;
                    }
                }

                if (selectedIndex == InvalidIndex)
                {
                    index = m_nextIndex;
                    for (AZ::u32 attempt = 0; attempt < RenderBufferCount; ++attempt)
                    {
                        RenderBufferEntry& entry = m_entries[index];
                        if (entry.m_state != RenderBufferState::PendingHandoff)
                        {
                            ++index;
                            if (index == RenderBufferCount)
                            {
                                index = 0;
                            }
                            continue;
                        }

                        AZ::RHI::Ptr<AZ::RHI::DeviceFence> deviceFence = entry.m_fence->GetDeviceFence(DeviceIndex);
                        if (!deviceFence
                            || deviceFence->WaitOnCpu() != AZ::RHI::ResultCode::Success)
                        {
                            return false;
                        }
                        selectedIndex = index;
                        break;
                    }
                }

                if (selectedIndex == InvalidIndex)
                {
                    return false;
                }

                RenderBufferEntry& entry = m_entries[selectedIndex];
                entry.m_state = RenderBufferState::Updating;
                hair.SetRenderPositionsCB(entry.m_computeBuffer);
                m_updateIndex = selectedIndex;
                m_nextIndex = selectedIndex + 1;
                if (m_nextIndex == RenderBufferCount)
                {
                    m_nextIndex = 0;
                }
                return true;
            }

            void CompleteUpdate() override
            {
                if (m_updateIndex == InvalidIndex)
                {
                    return;
                }

                RenderBufferEntry& entry = m_entries[m_updateIndex];
                AZ::RHI::Ptr<AZ::RHI::DeviceBuffer> deviceBuffer =
                    entry.m_buffer->GetRHIBuffer()->GetDeviceBuffer(DeviceIndex);
                AZ_Assert(deviceBuffer, "A renderer-owned hair buffer lost its device allocation.");
                if (deviceBuffer)
                {
                    SynchronizeNativeRenderBufferAfterUpdate(*deviceBuffer);
                }

                entry.m_token = m_nextToken++;
                if (m_nextToken == 0)
                {
                    m_nextToken = 1;
                }
                entry.m_state = RenderBufferState::Ready;
                m_currentIndex = m_updateIndex;
                m_updateIndex = InvalidIndex;
            }

            bool Acquire(HairRenderBuffer& buffer) override
            {
                if (m_currentIndex == InvalidIndex)
                {
                    return false;
                }

                RenderBufferEntry& entry = m_entries[m_currentIndex];
                if (entry.m_state != RenderBufferState::Ready)
                {
                    return false;
                }

                entry.m_state = RenderBufferState::Acquired;
                buffer = {
                    .m_attachmentId = entry.m_buffer->GetAttachmentId(),
                    .m_buffer = entry.m_buffer->GetRHIBuffer(),
                    .m_token = entry.m_token,
                    .m_vertexCount = m_vertexCount,
                    .m_stride = m_stride,
                };
                return true;
            }

            bool ImportHandoff(
                AZ::RHI::FrameGraphBuilder& frameGraphBuilder,
                const AZ::u64 token) override
            {
                for (RenderBufferEntry& entry : m_entries)
                {
                    if (entry.m_token != token
                        || entry.m_state != RenderBufferState::Acquired)
                    {
                        continue;
                    }

                    if (entry.m_fence->Reset() != AZ::RHI::ResultCode::Success)
                    {
                        return false;
                    }

                    AZ::RHI::FrameGraphAttachmentInterface attachmentDatabase =
                        frameGraphBuilder.GetAttachmentDatabase();
                    if (!attachmentDatabase.IsAttachmentValid(entry.m_buffer->GetAttachmentId())
                        && attachmentDatabase.ImportBuffer(
                            entry.m_buffer->GetAttachmentId(),
                            entry.m_buffer->GetRHIBuffer()) != AZ::RHI::ResultCode::Success)
                    {
                        entry.m_fence->SignalOnCpu();
                        return false;
                    }

                    if (frameGraphBuilder.ImportScopeProducer(*entry.m_handoffScope)
                        != AZ::RHI::ResultCode::Success)
                    {
                        entry.m_fence->SignalOnCpu();
                        return false;
                    }

                    entry.m_state = RenderBufferState::PendingHandoff;
                    return true;
                }

                return false;
            }

            bool PrepareForDestruction() override
            {
                for (RenderBufferEntry& entry : m_entries)
                {
                    if (entry.m_state == RenderBufferState::Acquired
                        || entry.m_state == RenderBufferState::Updating)
                    {
                        return false;
                    }

                    if (entry.m_state == RenderBufferState::PendingHandoff)
                    {
                        AZ::RHI::Ptr<AZ::RHI::DeviceFence> deviceFence = entry.m_fence->GetDeviceFence(DeviceIndex);
                        if (!deviceFence
                            || deviceFence->WaitOnCpu() != AZ::RHI::ResultCode::Success)
                        {
                            return false;
                        }
                        entry.m_state = RenderBufferState::Available;
                    }
                }

                return true;
            }

        private:
            static constexpr AZ::u32 InvalidIndex = AZStd::numeric_limits<AZ::u32>::max();

            AZStd::array<RenderBufferEntry, RenderBufferCount> m_entries;
            JPH::ComputeSystem& m_computeSystem;
            AZ::u64 m_nextToken = 1;
            AZ::u32 m_vertexCount = 0;
            AZ::u32 m_stride = 0;
            AZ::u32 m_currentIndex = InvalidIndex;
            AZ::u32 m_nextIndex = 0;
            AZ::u32 m_updateIndex = InvalidIndex;
        };
    } // namespace

    AZStd::unique_ptr<IHairRenderBuffers> CreatePlatformHairRenderBuffers(
        JPH::ComputeSystem& computeSystem,
        const AZ::u32 vertexCount,
        const AZ::u32 stride)
    {
        return AZStd::make_unique<PlatformHairRenderBuffers>(
            computeSystem,
            vertexCount,
            stride);
    }
} // namespace Jolt
