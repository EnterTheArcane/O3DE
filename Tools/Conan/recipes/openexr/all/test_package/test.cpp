// Copyright (c) Contributors to the Open 3D Engine Project.
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include <OpenEXR/ImfRgbaFile.h>

#include <filesystem>

int main()
{
    constexpr auto filename = "test-package.exr";
    using namespace OPENEXR_IMF_NAMESPACE;
    const Rgba pixel(0.5f, 0.25f, 0.125f, 1.0f);
    {
        RgbaOutputFile file(filename, 1, 1, WRITE_RGBA);
        file.setFrameBuffer(&pixel, 1, 1);
        file.writePixels(1);
    }

    Rgba result;
    {
        RgbaInputFile file(filename);
        file.setFrameBuffer(&result, 1, 1);
        file.readPixels(0, 0);
    }
    std::filesystem::remove(filename);
    if (result.r != pixel.r || result.g != pixel.g || result.b != pixel.b || result.a != pixel.a)
    {
        return 1;
    }
    return 0;
}
