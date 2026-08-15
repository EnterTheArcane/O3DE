/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/GpuHairShaderLoader.h>

#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/IO/Path/Path.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/Utils/Utils.h>
#include <AzCore/std/limits.h>

namespace Jolt
{
    bool LoadGpuHairShader(
        const char* name,
        JPH::Array<JPH::uint8>& data,
        JPH::String& error)
    {
        AZ::IO::FixedMaxPath path(AZ::Utils::GetExecutableDirectory());
        path /= "Jolt/Shaders";
        path /= name;

        AZ::IO::SystemFile file;
        if (!file.Open(path.c_str(), AZ::IO::SystemFile::SF_OPEN_READ_ONLY))
        {
            error = "Failed to open the Jolt hair shader.";
            return false;
        }

        const AZ::IO::SystemFile::SizeType byteCount = file.Length();
        if (byteCount > AZStd::numeric_limits<size_t>::max())
        {
            error = "The Jolt hair shader is too large to load.";
            return false;
        }

        data.resize(aznumeric_cast<size_t>(byteCount));
        if (file.Read(byteCount, data.data()) != byteCount)
        {
            data.clear();
            error = "Failed to read the complete Jolt hair shader.";
            return false;
        }

        return true;
    }
} // namespace Jolt
