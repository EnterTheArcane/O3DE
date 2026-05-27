import os

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeConfigDeps, CMakeToolchain
from thirdparty.tools.files import copy, get, rmdir, save
from thirdparty.tools.scm import Version
from thirdparty.tools.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "glslang"
    version = "1.4.350.0"
    license = "BSD-3-Clause"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "build_executables": [True, False],
        "enable_optimizer": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "build_executables": False,
        "enable_optimizer": True,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def requirements(self):
        if self.options.enable_optimizer:
            self.requires("spirv-tools")

    def build_requirements(self):
        self.tool_requires("cmake")

    def latest_version(self):
        repo = GithubRepository(self, "KhronosGroup/glslang")
        return Version(repo.latest_tag("vulkan-sdk-").removeprefix("vulkan-sdk-"))

    def source(self):
        get(
            self,
            url="https://github.com/KhronosGroup/glslang/archive/refs/tags/vulkan-sdk-1.4.350.0.tar.gz",
            sha256="a6885b1631fd77c89cd689b939cf2b3032c5ec13ee99250270d34bcad1efc10c",
            destination=os.path.join(self.source_folder, "src"),
            strip_root=True)
        wrapper = (
            "cmake_minimum_required(VERSION 3.15)\n"
            "project(cmake_wrapper)\n"
            "if(ENABLE_OPT)\n"
            "    find_package(SPIRV-Tools REQUIRED CONFIG)\n"
            "endif()\n"
            "add_subdirectory(src)\n"
        )
        save(self, os.path.join(self.source_folder, "CMakeLists.txt"), wrapper)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_SHARED_LIBS"] = self.options.shared
        tc.variables["GLSLANG_ENABLE_INSTALL"] = True
        tc.variables["ENABLE_GLSLANG_BINARIES"] = self.options.build_executables
        tc.variables["ENABLE_HLSL"] = True
        tc.variables["ENABLE_RTTI"] = True
        tc.variables["ENABLE_OPT"] = self.options.enable_optimizer
        tc.variables["ENABLE_SPVREMAPPER"] = False
        tc.variables["ENABLE_CTEST"] = False
        if self.options.enable_optimizer:
            spirv_tools_pkg = self.dependencies["spirv-tools"].package_folder
            tc.variables["spirv-tools_SOURCE_DIR"] = spirv_tools_pkg.replace("\\", "/")
        tc.generate()
        deps = CMakeConfigDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        src = os.path.join(self.source_folder, "src")
        copy(self, "LICENSE*", src=src, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "glslang")

        self.cpp_info.components["glslang-core"].set_property("cmake_target_name", "glslang::glslang")
        self.cpp_info.components["glslang-core"].libs = ["glslang"]
        self.cpp_info.components["glslang-core"].requires = ["osdependent", "machineindependent", "genericcodegen"]
        if self.settings.os == "Windows":
            self.cpp_info.components["glslang-core"].system_libs = ["psapi"]
        elif self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.components["glslang-core"].system_libs = ["m", "pthread"]

        if not self.options.shared:
            self.cpp_info.components["osdependent"].set_property("cmake_target_name", "glslang::OSDependent")
            self.cpp_info.components["osdependent"].libs = ["OSDependent"]

            self.cpp_info.components["machineindependent"].set_property("cmake_target_name", "glslang::MachineIndependent")
            self.cpp_info.components["machineindependent"].libs = ["MachineIndependent"]
            self.cpp_info.components["machineindependent"].requires = ["osdependent", "genericcodegen"]

            self.cpp_info.components["genericcodegen"].set_property("cmake_target_name", "glslang::GenericCodeGen")
            self.cpp_info.components["genericcodegen"].libs = ["GenericCodeGen"]

        self.cpp_info.components["spirv"].set_property("cmake_target_name", "glslang::SPIRV")
        self.cpp_info.components["spirv"].libs = ["SPIRV"]
        self.cpp_info.components["spirv"].requires = ["glslang-core"]

        self.cpp_info.components["glslang-default-resource-limits"].set_property(
            "cmake_target_name", "glslang::glslang-default-resource-limits")
        self.cpp_info.components["glslang-default-resource-limits"].libs = ["glslang-default-resource-limits"]
