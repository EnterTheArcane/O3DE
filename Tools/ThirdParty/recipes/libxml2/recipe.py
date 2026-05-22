# Simplified libxml2 recipe using CMake build (2.14+)
import os

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.tools.files import copy, get, rmdir


class Recipe(RecipeBase):
    name = "libxml2"
    license = "MIT"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    def requirements(self) -> list[str]:
        return ["zlib", "libiconv"]

    def source(self):
        get(url=self.thirdparty_data["versions"][self.version]["url"],
            dest=self.source_folder,
            sha256=self.thirdparty_data["versions"][self.version]["sha256"])

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["BUILD_SHARED_LIBS"] = self.options.shared
        tc.cache_variables["LIBXML2_WITH_ICONV"] = True
        tc.cache_variables["LIBXML2_WITH_ICU"] = False
        tc.cache_variables["LIBXML2_WITH_LZMA"] = False
        tc.cache_variables["LIBXML2_WITH_PYTHON"] = False
        tc.cache_variables["LIBXML2_WITH_ZLIB"] = True
        tc.cache_variables["LIBXML2_WITH_LEGACY"] = False
        tc.cache_variables["LIBXML2_WITH_TESTS"] = False
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy("Copyright", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(os.path.join(self.package_folder, "share"))
