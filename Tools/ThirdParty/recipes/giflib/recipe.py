import os

from thirdparty import RecipeBase
from thirdparty.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.files import apply_patches, copy, get
from thirdparty.microsoft import is_msvc


class Recipe(RecipeBase):
    name = "giflib"
    version = "5.2.2"
    license = "MIT"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "utils": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "utils": True,
    }

    def configure(self):
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def requirements(self):
        if is_msvc(self) and self.options.utils:
            self.requires("getopt-for-visual-studio")

    def source(self):
        get(
            self,
            url="https://downloads.sourceforge.net/project/giflib/giflib-5.x/giflib-5.2.2.tar.gz",
            sha256="be7ffbd057cadebe2aa144542fd90c6838c6a083b5e8a9048b8ee3b66b29d5fb",
            destination=self.folders.source,
            strip_root=True)
        apply_patches(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["GIFLIB_SRC_DIR"] = self.folders.source.as_posix()
        tc.variables["UTILS"] = self.options.utils
        tc.generate()

        if is_msvc(self):
            deps = CMakeDeps(self)
            deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure(build_script_folder=os.path.join(self.folders.source, os.pardir))
        cmake.build()

    def package(self):
        copy(self, "COPYING", src=self.folders.source, dst=os.path.join(self.folders.package, "licenses"))
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "GIF")
        self.cpp_info.set_property("cmake_target_name", "GIF::GIF")
        self.cpp_info.libs = ["gif"]
        if is_msvc(self):
            self.cpp_info.defines.append("USE_GIF_DLL" if self.options.shared else "USE_GIF_LIB")
