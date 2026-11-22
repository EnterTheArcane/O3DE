/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Windows.h>
#include <string_view>
#include "WindowsProbeResource.h"

int main()
{
    constexpr std::string_view expected = "o3de";
    const HMODULE module = GetModuleHandleW(nullptr);
    const HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(O3DE_WINDOWS_PROBE_RESOURCE), MAKEINTRESOURCEW(10));
    if (resource == nullptr || SizeofResource(module, resource) != expected.size())
    {
        return 1;
    }
    const auto* bytes = static_cast<const char*>(LockResource(LoadResource(module, resource)));
    if (bytes == nullptr || std::string_view(bytes, expected.size()) != expected)
    {
        return 2;
    }
    return 0;
}
