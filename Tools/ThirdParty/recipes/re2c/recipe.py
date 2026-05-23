import os

from thirdparty import RecipeBase
from thirdparty.tools.files import apply_conandata_patches, chdir, copy, get, rmdir, replace_in_file
from thirdparty.tools.cmake import CMakeToolchain, CMake
from thirdparty.tools.microsoft import is_msvc
from thirdparty.tools.scm import Version

class Recipe(RecipeBase):
    name = "re2c"
    version = "4.3"
    license = "LicenseRef-re2c"

    package_type = "application"
    settings = "os", "arch", "compiler", "build_type"

    def configure(self):
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def source(self):
        get(self, url="https://github.com/skvadrik/re2c/releases/download/4.3/re2c-4.3.tar.xz", sha256="51e88d6d6b6ab03eb7970276aca7e0db4f8e29c958b84b561d2fdcb8351c7150", destination=self.source_folder, strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["RE2C_REBUILD_DOCS"] = False
        tc.cache_variables["RE2C_BUILD_BENCHMARKS"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE",
             src=self.source_folder,
             dst=os.path.join(self.package_folder, "licenses"),
             keep_path=False)
        copy(self, "NO_WARRANTY",
             src=self.source_folder,
             dst=os.path.join(self.package_folder, "licenses"),
             keep_path=False)
        copy(self, "*.re",
             src=os.path.join(self.source_folder, "include"),
             dst=os.path.join(self.package_folder, "include"),
             keep_path=False)

        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "share"))

    def package_info(self):
        self.cpp_info.frameworkdirs = []
        self.cpp_info.libdirs = []
        self.cpp_info.resdirs = []
        self.cpp_info.includedirs = []

        include_dir = os.path.join(self.package_folder, "include")
        self.buildenv_info.define("RE2C_STDLIB_DIR", include_dir)
