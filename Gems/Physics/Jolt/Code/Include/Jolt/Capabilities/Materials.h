/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <Jolt/Handle.h>
#include <Jolt/Material.h>
#include <AzCore/std/parallel/atomic.h>

namespace Jolt
{
    class Runtime;

    class JOLT_API Materials
    {
    public:
        [[nodiscard]]
        static Materials* Get();

        [[nodiscard]]
        MaterialHandle CreateMaterial(const MaterialConfiguration& configuration);

        bool DestroyMaterial(MaterialHandle materialHandle);

        [[nodiscard]]
        bool IsValid(MaterialHandle materialHandle) const;

    private:
        friend class Runtime;

        Materials() = default;
        ~Materials() = default;

        static AZStd::atomic<Materials*> s_instance;
    };
} // namespace Jolt
