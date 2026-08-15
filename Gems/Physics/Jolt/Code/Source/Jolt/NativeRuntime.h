/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/System.h>

#include <AzCore/base.h>

namespace Jolt
{
    [[nodiscard]]
    AZ::u64 GetNativeBuildFingerprint();

    class NativeRuntime final
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
