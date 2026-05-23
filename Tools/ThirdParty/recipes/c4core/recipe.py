import os

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.tools.files import copy, get, rm, rmdir, apply_patches
from thirdparty.tools.scm.github import GithubRepository
from thirdparty.tools.scm import Version


class Recipe(RecipeBase):
    name = "c4core"
    version = "0.2.5"
    license = "MIT",

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_fast_float": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_fast_float": True,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def requirements(self):
        if self.options.with_fast_float:
            self.requires("fast_float", transitive_headers=True)

    def latest_version(self):
        repo = GithubRepository(self, "biojppm/c4core")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/biojppm/c4core/releases/download/v0.2.5/c4core-0.2.5-src.tgz",
            sha256="758f23718cbdc9465f104249561c4028858caf3355a90616b54d1dd937a981b1",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["C4CORE_WITH_FASTFLOAT"] = bool(self.options.with_fast_float)
        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        apply_patches(self)
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, pattern="LICENSE*", dst=os.path.join(self.package_folder, "licenses"), src=self.source_folder)
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "cmake"))
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        rm(self, "*.natvis", os.path.join(self.package_folder, "include"), recursive=True)

    def package_info(self):
        self.cpp_info.libs = ["c4core"]
        if not self.options.with_fast_float:
            self.cpp_info.defines.append("C4CORE_NO_FAST_FLOAT")

        self.cpp_info.set_property("cmake_file_name", "c4core")
        self.cpp_info.set_property("cmake_target_name", "c4core::c4core")

        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs.append("m")
