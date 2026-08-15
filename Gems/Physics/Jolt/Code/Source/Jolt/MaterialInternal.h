/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Handle.h>
#include <Jolt/Material.h>

#include <AzCore/std/string/string.h>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/PhysicsMaterial.h>

namespace Jolt
{
    class NativeMaterial final
        : public JPH::PhysicsMaterial
    {
    public:
        JPH_OVERRIDE_NEW_DELETE

        NativeMaterial(
            MaterialHandle handle,
            const MaterialConfiguration& configuration);

        [[nodiscard]]
        MaterialHandle GetHandle() const;

        const char* GetDebugName() const override;

        JPH::Color GetDebugColor() const override;

    private:
        AZStd::string m_debugName;
        JPH::Color m_debugColor;
        MaterialHandle m_handle;
    };
} // namespace Jolt
