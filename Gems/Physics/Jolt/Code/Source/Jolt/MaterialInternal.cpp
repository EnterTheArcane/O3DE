/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/MaterialInternal.h>

namespace Jolt
{
    NativeMaterial::NativeMaterial(
        const MaterialHandle handle,
        const MaterialConfiguration& configuration)
        : m_debugName(configuration.m_debugName)
        , m_debugColor(
              configuration.m_debugColor.GetR8(),
              configuration.m_debugColor.GetG8(),
              configuration.m_debugColor.GetB8(),
              configuration.m_debugColor.GetA8())
        , m_handle(handle)
    {
    }

    MaterialHandle NativeMaterial::GetHandle() const
    {
        return m_handle;
    }

    const char* NativeMaterial::GetDebugName() const
    {
        return m_debugName.c_str();
    }

    JPH::Color NativeMaterial::GetDebugColor() const
    {
        return m_debugColor;
    }
} // namespace Jolt
