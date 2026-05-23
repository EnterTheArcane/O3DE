from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import copy, get, replace_in_file, rmdir
from thirdparty.tools.scm import Version
import os

class Recipe(RecipeBase):
    name = "kuba-zip"
    version = "0.3.2"
    license = "Unlicense"

    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def source(self):
        get(self, url="https://github.com/kuba--/zip/archive/v0.3.2.tar.gz", sha256="0c33740aec7a3913bca07df360420c19cac5e794e0f602f14f798cb2e6f710e5", destination=self.source_folder, strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["CMAKE_DISABLE_TESTING"] = True
        tc.variables["ZIP_STATIC_PIC"] = self.options.get_safe("fPIC", True)
        tc.variables["ZIP_BUILD_DOCS"] = False
        if Version(self.version) < "0.2.3":
            tc.cache_variables["CMAKE_POLICY_VERSION_MINIMUM"] = "3.5" # CMake 4 support
        tc.generate()

    def _patch_sources(self):
        replace_in_file(self, os.path.join(self.source_folder, "CMakeLists.txt"), "-Werror", "")

    def build(self):
        self._patch_sources()
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "UNLICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "zip")
        self.cpp_info.set_property("cmake_target_name", "zip::zip")

        self.cpp_info.libs = ["zip"]
        if self.options.shared:
            self.cpp_info.defines.append("ZIP_SHARED")
