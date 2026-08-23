/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <Jolt/SystemConfiguration.h>

#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace AZ
{
    class JobContext;
}

namespace Jolt
{
    class Runtime;

    class JOLT_API System final
    {
    public:
        System(
            SystemConfiguration configuration,
            AZ::JobContext* jobContext = nullptr);

        //! Capability pointers are non-owning. All callers must be quiescent before the System is destroyed.
        ~System();

        AZ_DISABLE_COPY_MOVE(System);

        explicit operator bool() const noexcept;

    private:
        AZStd::unique_ptr<Runtime> m_runtime;
    };
} // namespace Jolt
