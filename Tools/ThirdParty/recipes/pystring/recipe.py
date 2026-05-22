from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import copy, get
import os


class Recipe(RecipeBase):
    name = "pystring"
    version = "1.1.4"
    license = "BSD-3-Clause"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    exports_sources = "CMakeLists.txt"

    def source(self):
        get(
            url="https://github.com/imageworks/pystring/archive/refs/tags/v1.1.4.tar.gz",
            dest=self.source_folder,
            sha256="49da0fe2a049340d3c45cce530df63a2278af936003642330287b68cefd788fb",
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["PYSTRING_SRC_DIR"] = self.source_folder.replace("\\", "/")
        tc.generate()

    def build(self):
        import shutil

        shutil.copy(
            os.path.join(os.path.dirname(os.path.abspath(__file__)), "CMakeLists.txt"),
            os.path.normpath(
                os.path.join(self.source_folder, os.pardir, "CMakeLists.txt")
            ),
        )
        cmake = CMake(self)
        cmake.configure(build_script_folder=os.path.join(self.source_folder, os.pardir))
        cmake.build()

    def package(self):
        copy(
            "LICENSE",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        cmake = CMake(self)
        cmake.install()
