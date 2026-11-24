#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

from pathlib import Path

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.files import copy, get, rename, save
from conan.tools.layout import basic_layout


class Recipe(ConanFile):
    name = "emsdk"
    homepage = "https://emscripten.org/"
    license = "MIT OR NCSA"
    package_type = "application"
    settings = "os", "arch"

    def set_version(self):
        if self.version is None:
            (self.version,) = self.conan_data["sources"]

    def layout(self):
        basic_layout(self)

    def validate(self):
        sources = self.conan_data["sources"][self.version]
        if str(self.settings.arch) not in sources.get(str(self.settings.os), {}):
            raise ConanInvalidConfiguration(
                f"No Emscripten SDK archive for {self.settings.os} {self.settings.arch}"
            )

    def package_id(self):
        self.info.settings.rm_safe("os.version")

    def build(self):
        sources = self.conan_data["sources"][self.version]
        archives = sources[str(self.settings.os)][str(self.settings.arch)]
        for name, destination in (("sdk", "upstream"), ("node", "node")):
            get(
                self,
                **archives[name],
                destination=destination,
                strip_root=True,
                extract_filter="data",
            )

    def package(self):
        build = Path(self.build_folder)
        package = Path(self.package_folder)
        for directory in ("upstream", "node"):
            rename(self, build / directory, package / directory)
        copy(
            self,
            "LICENSE",
            package / "upstream/emscripten",
            package / "licenses/emscripten",
        )
        node = "node/node.exe" if self.settings.os == "Windows" else "node/bin/node"
        copy(self, "LICENSE", package / "node", package / "licenses/node")
        # Emscripten expands CFGDIR to the location of this file after relocation.
        save(
            self,
            package / ".emscripten",
            f"""import os

# Qt reads the same relative path that the upstream emsdk config exposes.
EMSCRIPTEN_ROOT = os.path.dirname(__file__) + '/upstream/emscripten'
LLVM_ROOT = '$CFGDIR/upstream/bin'
BINARYEN_ROOT = '$CFGDIR/upstream'
NODE_JS = '$CFGDIR/{node}'
CACHE = '~/.cache/conan-emsdk/{self.version}'
""",
        )

    def package_info(self):
        root = Path(self.package_folder)
        tools = root / "upstream/emscripten"
        node = root / (
            "node/node.exe" if self.settings.os == "Windows" else "node/bin/node"
        )
        self.cpp_info.bindirs = [
            "upstream/emscripten",
            node.parent.relative_to(root).as_posix(),
        ]
        self.cpp_info.includedirs = []
        self.cpp_info.libdirs = []
        self.buildenv_info.define_path("EMSDK", root.as_posix())
        self.buildenv_info.define_path("EM_CONFIG", (root / ".emscripten").as_posix())
        self.buildenv_info.define_path("EMSDK_NODE", node.as_posix())
        # Native tools can call emcc without using it as their own compiler.
        if self.settings_target is None or self.settings_target.os != "Emscripten":
            return
        for var, command in (
            ("CC", "emcc"),
            ("CXX", "em++"),
            ("AR", "emar"),
            ("RANLIB", "emranlib"),
            ("NM", "emnm"),
        ):
            self.buildenv_info.define_path(var, (tools / command).as_posix())
        self.conf_info.define(
            "tools.build:compiler_executables",
            {"c": (tools / "emcc").as_posix(), "cpp": (tools / "em++").as_posix()},
        )
        self.conf_info.define(
            "tools.cmake.cmaketoolchain:user_toolchain",
            [(tools / "cmake/Modules/Platform/Emscripten.cmake").as_posix()],
        )
