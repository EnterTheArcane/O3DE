#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

import os
from pathlib import Path
import sys

folder = Path(sys.argv[1]).resolve()
sys.path.insert(0, str(folder))
if sys.platform == "win32":
    dlls = os.add_dll_directory(str(folder))

import OpenImageIO as oiio
import PyOpenColorIO as ocio

assert Path(oiio.__file__).is_relative_to(folder)
assert Path(ocio.__file__).is_relative_to(folder)
assert oiio.ImageSpec(1, 1, 3, oiio.TypeDesc(oiio.UINT8)).nchannels == 3
assert ocio.Config.CreateRaw().getProcessor("raw", "raw").isNoOp()

scripts = (
    Path(__file__).resolve().parents[4] / "Gems/Atom/Feature/Common/Editor/Scripts"
)
sys.path.insert(0, str(scripts))
from ColorGrading import inv_shaper_transform, shaper_transform

image = oiio.ImageBuf(oiio.ImageSpec(1, 1, 3, oiio.TypeDesc(oiio.FLOAT)))
pixel = (0.25, 0.5, 1.0)
image.setpixel(0, 0, tuple(shaper_transform(0.5, 0.1, value) for value in pixel))
result = tuple(inv_shaper_transform(0.5, 0.1, value) for value in image.getpixel(0, 0))
assert all(abs(actual - expected) < 1e-6 for actual, expected in zip(result, pixel))
