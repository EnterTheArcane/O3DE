// Copyright (c) Contributors to the Open 3D Engine Project.
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include <OpenImageIO/imageio.h>

#include <filesystem>
#include <iostream>
#include <string>

#if defined(__EMSCRIPTEN__) && defined(__wasm64__)
static_assert(OIIO::BaseTypeFromC<long>::value == OIIO::TypeDesc::INT64);
static_assert(OIIO::BaseTypeFromC<unsigned long long>::value == OIIO::TypeDesc::UINT64);
#endif

int main()
{
#if defined(TEST_OPENIMAGEIO_FORMATS)
    const std::string formats = "," + std::string(OIIO::get_string_attribute("format_list")) + ",";
    constexpr const char* requiredFormats[] = {
#if defined(TEST_OPENIMAGEIO_DICOM)
        "dicom",
#endif
#if defined(TEST_OPENIMAGEIO_OPENVDB)
        "openvdb",
#endif
        "gif", "heif", "jpeg2000", "jpegxl", "ptex", "raw", "webp"
    };
    for (const char* format : requiredFormats)
    {
        if (formats.find("," + std::string(format) + ",") == std::string::npos)
        {
            std::cerr << "Missing format " << format << " in " << formats << '\n';
            return 4;
        }
    }
#endif
    constexpr auto file = "test-package.png";
    const auto output = OIIO::ImageOutput::create(file);
    const OIIO::ImageSpec spec(1, 1, 3, OIIO::TypeDesc::UINT8);
    const unsigned char pixel[] = { 64, 128, 255 };
    if (!output || !output->open(file, spec) || !output->write_image(OIIO::TypeDesc::UINT8, pixel) || !output->close())
    {
        return 1;
    }

    const auto input = OIIO::ImageInput::open(file);
    unsigned char result[3] = {};
    if (!input || !input->read_image(0, 0, 0, 3, OIIO::TypeDesc::UINT8, result) || !input->close())
    {
        return 2;
    }
    std::filesystem::remove(file);
    if (result[0] != pixel[0] || result[1] != pixel[1] || result[2] != pixel[2])
    {
        return 3;
    }
    return 0;
}
