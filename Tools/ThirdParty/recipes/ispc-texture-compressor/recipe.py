import os

from thirdparty import RecipeBase
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import copy, get


class Recipe(RecipeBase):
    name = "ispc-texture-compressor"
    version = "2024.09.23"
    license = "MIT"

    options = {
        "shared": [True, False],
        "fPIC": [True, False]
    }
    default_options = {
        "shared": False,
        "fPIC": True
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def build_requirements(self):
        self.tool_requires("cmake")
        self.tool_requires("ispc")

    def source(self):
        get(
            self,
            url="https://github.com/GameTechDev/ISPCTextureCompressor/archive/79ddbc90334fc31edd438e68ccb0fe99b4e15aab.tar.gz",
            sha256="506650f63f7a4a41237206083c8b3785a64daa94d4a896b3757d39581972f70b",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["ISPC_TEXCOMP_SRC_DIR"] = self.source_folder.replace("\\", "/")
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure(build_script_folder=os.path.join(self.source_folder, os.pardir))
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "ispc-texture-compressor")
        self.cpp_info.set_property("cmake_target_name", "ispc_texcomp::ispc_texcomp")
        self.cpp_info.libs = ["ispc_texcomp"]
        if self.settings.os == "Windows":
            bin_dir = os.path.join(self.package_folder, "bin")
            self.buildenv_info.prepend_path("PATH", bin_dir)
            self.env_info.PATH.append(bin_dir)
