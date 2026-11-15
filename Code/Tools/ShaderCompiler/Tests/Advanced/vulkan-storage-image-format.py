"""
Copyright (c) Contributors to the Open 3D Engine Project.
SPDX-License-Identifier: Apache-2.0 OR MIT
"""

import re
import testfuncs

result = 0
resultFailed = 0


def doTests(compiler, silent, azdxcpath):
    global result, resultFailed
    source = "vulkan-storage-image-format.azsl"
    unknown_format = '[[vk::image_format("unknown")]]'
    for platform in ("vk", "dx", "mt"):
        output, ok = testfuncs.buildAndGet(source, compiler, silent, [f"--namespace={platform}"])
        if ok:
            output = output.decode("utf-8")
            if platform == "vk":
                ok = output.count(unknown_format) == 5
                ok = ok and output.count('[[vk::image_format("rgba16f")]]') == 1
                ok = ok and bool(re.search(
                    r'\[\[vk::image_format\("unknown"\)\]\]\s+globallycoherent\s+RWTexture2D<float4>\s+Images_coherentImage',
                    output))
                ok = ok and bool(re.search(r'^RWTexture2DArray<uint> Images_image2DArray', output, re.MULTILINE))
                ok = ok and bool(re.search(
                    r'\[\[vk::image_format\("rgba16f"\)\]\]\s+RWTexture2D<float4>\s+Images_explicitFormat',
                    output))
            else:
                ok = unknown_format not in output
        if ok:
            result += 1
        else:
            resultFailed += 1
