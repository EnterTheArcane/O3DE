import os

from thirdparty import RecipeBase
from thirdparty.build import stdcpp_library
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.env import VirtualBuildEnv
from thirdparty.files import copy, get, replace_in_file, rm, rmdir
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "spirv-tools"
    version = "1.4.352.0"
    license = "Apache-2.0"

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

    def requirements(self):
        self.requires(f"spirv-headers")

    def latest_version(self):
        repo = GithubRepository(self, "KhronosGroup/SPIRV-Tools")
        return Version(repo.latest_tag("vulkan-sdk-").removeprefix("vulkan-sdk-"))

    def source(self):
        get(
            self,
            url="https://github.com/KhronosGroup/SPIRV-Tools/archive/18b08af19b08515d5a1749adbdb05bc4369a57ed.tar.gz",
            sha256="3445457d807912b879f526eade9e191235e1bd272ff8e70e7fd8b7e5435e2397",
            destination=self.source_folder,
            strip_root=True)
        replace_in_file(self, os.path.join(self.source_folder, "CMakeLists.txt"), "set(CMAKE_POSITION_INDEPENDENT_CODE ON)", "")

    def generate(self):
        env = VirtualBuildEnv(self)
        env.generate()

        tc = CMakeToolchain(self)

        # ====================
        # Shared libs mess in Spirv-Tools (see https://github.com/KhronosGroup/SPIRV-Tools/issues/3909)
        # ====================
        # We have 2 solutions if shared True:
        #  - Only package SPIRV-Tools-shared lib (private symbols properly hidden), and wait resolution
        #    of above issue before allowing to build shared for all Spirv-Tools libs.
        #  - Build and package shared libs with all symbols exported
        #    (it would require CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS for msvc)
        # Currently this recipe implements the first solution

        # Static and shared libs are controlled by a weird combination
        # of SPIRV_TOOLS_BUILD_STATIC and BUILD_SHARED_LIBS.
        tc.variables["SPIRV_TOOLS_BUILD_STATIC"] = True
        # ============

        # Required by the project's CMakeLists.txt
        tc.variables["SPIRV-Headers_SOURCE_DIR"] = self.dependencies["spirv-headers"].package_folder.replace("\\", "/")

        # There are some switch( ) statements that are causing errors
        # need to turn this off
        tc.variables["SPIRV_WERROR"] = False

        tc.variables["SKIP_SPIRV_TOOLS_INSTALL"] = False
        tc.variables["SPIRV_LOG_DEBUG"] = False
        tc.variables["SPIRV_SKIP_TESTS"] = True
        tc.variables["SPIRV_CHECK_CONTEXT"] = False
        tc.variables["SPIRV_BUILD_FUZZER"] = False
        tc.variables["SPIRV_SKIP_EXECUTABLES"] = False
        # For iOS/tvOS/watchOS
        tc.variables["CMAKE_MACOSX_BUNDLE"] = False

        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE*", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()

        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        rmdir(self, os.path.join(self.package_folder, "SPIRV-Tools"))
        rmdir(self, os.path.join(self.package_folder, "SPIRV-Tools-link"))
        rmdir(self, os.path.join(self.package_folder, "SPIRV-Tools-opt"))
        rmdir(self, os.path.join(self.package_folder, "SPIRV-Tools-reduce"))
        rmdir(self, os.path.join(self.package_folder, "SPIRV-Tools-lint"))
        rmdir(self, os.path.join(self.package_folder, "SPIRV-Tools-diff"))
        rmdir(self, os.path.join(self.package_folder, "SPIRV-Tools-tools"))
        if self.options.shared:
            for file_name in [
                "*SPIRV-Tools", "*SPIRV-Tools-opt", "*SPIRV-Tools-link",
                "*SPIRV-Tools-reduce", "*SPIRV-Tools-lint",
            ]:
                for ext in [".a", ".lib"]:
                    rm(self, f"{file_name}{ext}", os.path.join(self.package_folder, "lib"))
        else:
            rm(self, "*SPIRV-Tools-shared.dll", os.path.join(self.package_folder, "bin"))
            rm(self, "*SPIRV-Tools-shared*", os.path.join(self.package_folder, "lib"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "SPIRV-Tools")
        self.cpp_info.set_property("pkg_config_name", "SPIRV-Tools-shared" if self.options.shared else "SPIRV-Tools")

        # SPIRV-Tools
        self.cpp_info.components["spirv-tools-core"].set_property(
            "cmake_target_name",
            "SPIRV-Tools-shared" if self.options.shared else "SPIRV-Tools-static",
        )
        self.cpp_info.components["spirv-tools-core"].set_property("cmake_target_aliases", ["SPIRV-Tools"])  # before 2020.5, kept for conveniency
        self.cpp_info.components["spirv-tools-core"].libs = ["SPIRV-Tools-shared" if self.options.shared else "SPIRV-Tools"]
        self.cpp_info.components["spirv-tools-core"].requires = ["spirv-headers::spirv-headers"]
        if self.options.shared:
            self.cpp_info.components["spirv-tools-core"].defines = ["SPIRV_TOOLS_SHAREDLIB"]
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.components["spirv-tools-core"].system_libs.extend(["m", "rt"])
        if not self.options.shared:
            libcxx = stdcpp_library(self)
            if libcxx:
                self.cpp_info.components["spirv-tools-core"].system_libs.append(libcxx)

        # FIXME: others components should have their own CMake config file
        if not self.options.shared:
            # SPIRV-Tools-opt
            self.cpp_info.components["spirv-tools-opt"].set_property("cmake_target_name", "SPIRV-Tools-opt")
            self.cpp_info.components["spirv-tools-opt"].libs = ["SPIRV-Tools-opt"]
            self.cpp_info.components["spirv-tools-opt"].requires = ["spirv-tools-core", "spirv-headers::spirv-headers"]
            if self.settings.os in ["Linux", "FreeBSD"]:
                self.cpp_info.components["spirv-tools-opt"].system_libs.append("m")

            # SPIRV-Tools-link
            self.cpp_info.components["spirv-tools-link"].set_property("cmake_target_name", "SPIRV-Tools-link")
            self.cpp_info.components["spirv-tools-link"].libs = ["SPIRV-Tools-link"]
            self.cpp_info.components["spirv-tools-link"].requires = ["spirv-tools-core", "spirv-tools-opt"]

            # SPIRV-Tools-reduce
            self.cpp_info.components["spirv-tools-reduce"].set_property("cmake_target_name", "SPIRV-Tools-reduce")
            self.cpp_info.components["spirv-tools-reduce"].libs = ["SPIRV-Tools-reduce"]
            self.cpp_info.components["spirv-tools-reduce"].requires = ["spirv-tools-core", "spirv-tools-opt"]

            # SPIRV-Tools-lint
            self.cpp_info.components["spirv-tools-lint"].set_property("cmake_target_name", "SPIRV-Tools-lint")
            self.cpp_info.components["spirv-tools-lint"].libs = ["SPIRV-Tools-lint"]
            self.cpp_info.components["spirv-tools-lint"].requires = ["spirv-tools-core", "spirv-tools-opt"]

            # SPIRV-Tools-diff
            self.cpp_info.components["spirv-tools-diff"].set_property("cmake_target_name", "SPIRV-Tools-diff")
            self.cpp_info.components["spirv-tools-diff"].libs = ["SPIRV-Tools-diff"]
            self.cpp_info.components["spirv-tools-diff"].requires = ["spirv-tools-core", "spirv-tools-opt"]

        if Version(self.version) < "1.3" and not self.options.shared:
            del self.cpp_info.components["spirv-tools-diff"]
