/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <cstddef>

namespace AZ::RHI
{
    class DeviceBuffer;
    class Device;
} // namespace AZ::RHI

namespace AZ::Metal
{
    //! Returns the unretained native id<MTLDevice> as an opaque pointer.
    [[nodiscard]]
    void* GetDeviceNativeHandle(RHI::Device& device);

    //! Returns the unretained native id<MTLBuffer> as an opaque pointer.
    [[nodiscard]]
    void* GetBufferNativeHandle(RHI::DeviceBuffer& buffer);

    [[nodiscard]]
    size_t GetBufferAllocationOffset(RHI::DeviceBuffer& buffer);
} // namespace AZ::Metal
