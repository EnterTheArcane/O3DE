# Ported from conan-center-index/jansson by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import copy, get, rmdir
from thirdparty.tools.microsoft import is_msvc, is_msvc_static_runtime
from thirdparty.tools.scm import Version
import os

class Recipe(RecipeBase):
    name = "jansson"
    license = "MIT"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "use_urandom": [True, False],
        "use_windows_cryptoapi": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "use_urandom": True,
        "use_windows_cryptoapi": True,
    }

    def source(self):
        get(url=self.thirdparty_data["versions"][self.version]["url"], dest=self.source_folder, sha256=self.thirdparty_data["versions"][self.version]["sha256"])

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["JANSSON_BUILD_DOCS"] = False
        tc.variables["JANSSON_BUILD_SHARED_LIBS"] = self.options.shared
        tc.variables["JANSSON_EXAMPLES"] = False
        tc.variables["JANSSON_WITHOUT_TESTS"] = True
        tc.variables["USE_URANDOM"] = self.options.use_urandom
        tc.variables["USE_WINDOWS_CRYPTOAPI"] = self.options.use_windows_cryptoapi
        if self.is_windows:
            tc.variables["JANSSON_STATIC_CRT"] = False
        if Version(self.version) <= "2.14.1":  # pylint: disable=conan-condition-evals-to-constant
            tc.cache_variables["CMAKE_POLICY_VERSION_MINIMUM"] = "3.5" # CMake 4 support
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy("LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(os.path.join(self.package_folder, "cmake"))
