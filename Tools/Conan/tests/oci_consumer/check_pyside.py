# Copyright (c) Contributors to the Open 3D Engine Project.
# SPDX-License-Identifier: Apache-2.0 OR MIT

import os
from pathlib import Path
import sys

sdk, qt = map(Path, sys.argv[1:])
if sys.platform == "win32":
    packages = sdk / "lib/site-packages"
    pyside_dlls = os.add_dll_directory(str(sdk / "bin"))
    qt_dlls = os.add_dll_directory(str(qt / "bin"))
else:
    packages = sdk / "lib/python/site-packages"
    if not packages.is_dir():
        major, minor = sys.version_info[:2]
        packages = sdk / f"lib/python{major}.{minor}/site-packages"
sys.path.insert(0, str(packages))

from PySide6 import QtCore, QtGui, QtUiTools

assert Path(QtCore.__file__).is_relative_to(packages), QtCore.__file__
assert Path(QtUiTools.__file__).is_relative_to(packages), QtUiTools.__file__
if sys.platform == "win32":
    debug = hasattr(sys, "gettotalrefcount")
    for module in (QtCore, QtUiTools):
        assert module.__file__.endswith("_d.pyd") == debug, module.__file__
assert QtCore.qVersion() == "6.12.0"
image = QtGui.QImage(1, 1, QtGui.QImage.Format.Format_ARGB32)
assert image.width() == 1
