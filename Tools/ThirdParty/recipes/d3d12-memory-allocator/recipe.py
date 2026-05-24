import os

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import copy, get, rmdir
from thirdparty.tools.scm import Version
from thirdparty.tools.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "d3d12-memory-allocator"
    version = "3.1.0"
    license = "MIT"

    options = {
        "shared": [True, False],
    }
    default_options = {
        "shared": False,
    }

    def build_requirements(self):
        self.tool_requires("cmake")

    def latest_version(self):
        repo = GithubRepository(self, "GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator/archive/refs/tags/v3.1.0.tar.gz",
            sha256="629aa00919f2f0f8cdfcdc485db5077f456e939e418cee148e17458efa055f63",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_DOCUMENTATION"] = False
        tc.variables["D3D12MA_BUILD_SAMPLE"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "*LICENSE*", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "share"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "D3D12MemoryAllocator")
        self.cpp_info.set_property("cmake_target_name", "GPUOpen::D3D12MemoryAllocator")
        postfix = {"Release": "", "Debug": "d", "RelWithDebInfo": "rd", "MinSizeRel": "s"}[str(self.settings.build_type)]
        self.cpp_info.libs = [f"D3D12MA{postfix}"]
