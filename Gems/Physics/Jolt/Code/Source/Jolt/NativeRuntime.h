/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Capabilities/RuntimeConfiguration.h>
#include <Jolt/Configuration.h>

#include <AzCore/base.h>
#include <AzCore/std/parallel/mutex.h>

namespace Jolt
{
    class DebugRenderer;

    struct NativeMemoryStatistics final
    {
        AZ::u64 m_allocatedBytes = 0;
        AZ::u64 m_peakAllocatedBytes = 0;
        AZ::u64 m_allocationCount = 0;
        AZ::u64 m_freeCount = 0;
        AZ::u64 m_reallocationCount = 0;
    };

    [[nodiscard]]
    JOLT_API AZ::u64 GetNativeBuildFingerprint();

    JOLT_API void AcquireNativeMemoryStatistics();

    JOLT_API void ReleaseNativeMemoryStatistics();

    [[nodiscard]]
    JOLT_API NativeMemoryStatistics GetNativeMemoryStatistics(bool reset);

    [[nodiscard]]
    JOLT_API float GetSoftBodyTriangleThickness();

    [[nodiscard]]
    DebugRenderer* GetNativeDebugRenderer();

    [[nodiscard]]
    AZStd::mutex& GetNativeDebugRendererMutex();

    class JOLT_API NativeRuntime final
    {
    public:
        explicit NativeRuntime(float softBodyTriangleThickness = 0.1f);
        ~NativeRuntime();

        AZ_DISABLE_COPY_MOVE(NativeRuntime);

        constexpr explicit operator bool() const noexcept
        {
            return m_initialized;
        }

        [[nodiscard]]
        RuntimeInfo GetRuntimeInfo() const;

    private:
        bool m_initialized = false;
    };
} // namespace Jolt
