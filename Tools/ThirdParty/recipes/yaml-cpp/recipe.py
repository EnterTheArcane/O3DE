# Ported from conan-center-index/yaml-cpp by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import apply_patches, collect_libs, copy, get, rmdir
from thirdparty.tools.microsoft import is_msvc, is_msvc_static_runtime
from thirdparty.tools.scm import Version
import os

class Recipe(RecipeBase):
    name = "yaml-cpp"
    license = "MIT"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    def source(self):
        get(url=self.thirdparty_data["versions"][self.version]["url"], dest=self.source_folder, sha256=self.thirdparty_data["versions"][self.version]["sha256"])

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["YAML_CPP_BUILD_TESTS"] = False
        tc.variables["YAML_CPP_BUILD_CONTRIB"] = True
        tc.variables["YAML_CPP_BUILD_TOOLS"] = False
        tc.variables["YAML_CPP_INSTALL"] = True
        tc.variables["YAML_BUILD_SHARED_LIBS"] = self.options.shared
        if Version(self.version) <= "0.8.0": # pylint: disable=conan-condition-evals-to-constant
            tc.cache_variables["CMAKE_POLICY_VERSION_MINIMUM"] = "3.5" # CMake 4 support
        if self.is_windows:
            tc.variables["YAML_MSVC_SHARED_RT"] = not False
            tc.preprocessor_definitions["_NOEXCEPT"] = "noexcept"
        if Version(self.version) >= "0.9.0":
            tc.cache_variables["YAML_ENABLE_PIC"] = self.options.get("fPIC", "OFF")
        tc.generate()

    def build(self):
        apply_patches(self)
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy("LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(os.path.join(self.package_folder, "CMake"))
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(os.path.join(self.package_folder, "share"))
