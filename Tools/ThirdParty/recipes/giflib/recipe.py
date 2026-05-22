from thirdparty import RecipeBase as ConanFile
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.tools.files import apply_conandata_patches, copy, get
from thirdparty.tools.microsoft import is_msvc
import os

class Recipe(ConanFile):
    name = "giflib"
    version = "5.2.2"
    license = "MIT"
    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "utils" : [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "utils" : True,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def requirements(self):
        if is_msvc(self) and self.options.utils:
            self.requires("getopt-for-visual-studio/20200201")

    def source(self):
        get(self, url="https://downloads.sourceforge.net/project/giflib/giflib-5.x/giflib-5.2.2.tar.gz", sha256="be7ffbd057cadebe2aa144542fd90c6838c6a083b5e8a9048b8ee3b66b29d5fb", destination=self.source_folder, strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["GIFLIB_SRC_DIR"] = self.source_folder.replace("\\", "/")
        tc.variables["UTILS"] = self.options.utils
        tc.generate()

        if is_msvc(self):
            cd = CMakeDeps(self)
            cd.generate()

    def build(self):
        apply_conandata_patches(self)
        cmake = CMake(self)
        cmake.configure(build_script_folder=os.path.join(self.source_folder, os.pardir))
        cmake.build()

    def package(self):
        copy(self, "COPYING", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.set_property("cmake_find_mode", "both")
        self.cpp_info.set_property("cmake_file_name", "GIF")
        self.cpp_info.set_property("cmake_target_name", "GIF::GIF")
        self.cpp_info.libs = ["gif"]
        if is_msvc(self):
            self.cpp_info.defines.append("USE_GIF_DLL" if self.options.shared else "USE_GIF_LIB")
