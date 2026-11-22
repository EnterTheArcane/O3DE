// Copyright (c) Contributors to the Open 3D Engine Project.
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include <OpenColorIO/OpenColorIO.h>

int main()
{
    const auto config = OCIO_NAMESPACE::Config::CreateRaw();
    const auto processor = config->getProcessor("raw", "raw")->getDefaultCPUProcessor();
    float rgb[] = { 0.25f, 0.5f, 1.0f };
    processor->applyRGB(rgb);
    if (rgb[0] != 0.25f || rgb[1] != 0.5f || rgb[2] != 1.0f)
    {
        return 1;
    }
    return 0;
}
