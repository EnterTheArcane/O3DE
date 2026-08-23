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

namespace Jolt
{
    class Runtime;

    class JOLT_API Materials
    {
    public:
        //! Returns the active capability, or nullptr if no global System is active.
        //! The pointer is non-owning and must not be acquired or used while System destruction can occur.
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
    };
} // namespace Jolt
