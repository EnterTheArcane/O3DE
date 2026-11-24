#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

from typing import ClassVar

from conan import ConanFile
from conan.tools.env import VirtualBuildEnv


class Packages(ConanFile):
    settings = "os", "arch", "build_type"
    python_requires = "apple-signing/1.0"
    extension_properties: ClassVar = {
        "engine_provided": ("zlib", "zstd"),
        # Python's bootstrap runs before engine prerequisites can be set up.
        "bundle_msvc_runtime": ("python",),
    }

    def generate(self):
        VirtualBuildEnv(self).generate()

    def requirements(self):
        self.requires("gz-cmake/5.2.0")
        self.requires("imath/3.2.3")
        self.requires("ispc-texture-compressor/2026.11.24")
        self.requires("nvcloth/1.1.6")
        self.requires("opencolorio/2.6.0")
        self.requires("openexr/3.5.4")
        self.requires("openimageio/3.2.2.0")
        self.requires("openssl/4.1.0")
        self.requires("physx/5.13.0")
        self.requires("python/3.15.2")
        self.requires("qt/6.12.0")
        self.requires("sdformat/16.1.0")

        desktop = self.settings.os in ("Linux", "Macos", "Windows")
        web = self.settings.os == "Emscripten"

        if desktop:
            self.requires("directxshadercompiler/1.9.2609")
            self.requires("pyside6/6.12.0")

        if not web:
            self.requires("crashpad/2026.10.01")
