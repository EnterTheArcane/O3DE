// Copyright (c) Contributors to the Open 3D Engine Project.
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include <ISPC/ispc_texcomp.h>

#include <array>
#include <cstdint>

int main()
{
    std::array<std::uint8_t, 64> rgba;
    rgba.fill(255);
    const rgba_surface surface{rgba.data(), 4, 4, 16};
    std::array<std::uint8_t, 8> bc1{};
    CompressBlocksBC1(&surface, bc1.data());
    if (bc1[0] == 0 || bc1[1] == 0)
    {
        return 1;
    }
    return 0;
}
