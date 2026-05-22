from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import copy, get, replace_in_file, rm, rmdir, apply_patches
from thirdparty.tools.scm import Version
import os


class Recipe(RecipeBase):
    name = "spirv-tools"
    version = "1.4.313.0"
    license = "Apache-2.0"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "build_executables": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "build_executables": True,
    }

    def requirements(self) -> list[str]:
        return ["spirv-headers"]

    def source(self):
        get(
            url="https://github.com/KhronosGroup/SPIRV-Tools/archive/refs/tags/vulkan-sdk-1.4.313.0.tar.gz",
            dest=self.source_folder,
            sha256="6b60f723345ceed5291cceebbcfacf7fea9361a69332261fa08ae57e2a562005",
        )

    def generate(self):
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
        tc.variables["SPIRV-Headers_SOURCE_DIR"] = self.dependencies[
            "spirv-headers"
        ].package_folder.replace("\\", "/")

        # There are some switch( ) statements that are causing errors
        # need to turn this off
        tc.variables["SPIRV_WERROR"] = False

        tc.variables["SKIP_SPIRV_TOOLS_INSTALL"] = False
        tc.variables["SPIRV_LOG_DEBUG"] = False
        tc.variables["SPIRV_SKIP_TESTS"] = True
        tc.variables["SPIRV_CHECK_CONTEXT"] = False
        tc.variables["SPIRV_BUILD_FUZZER"] = False
        tc.variables["SPIRV_SKIP_EXECUTABLES"] = not self.options.build_executables
        # To install relocatable shared libs on Macos
        if Version(self.version) < "1.3.239":
            tc.cache_variables["CMAKE_POLICY_DEFAULT_CMP0042"] = "NEW"
            tc.cache_variables["CMAKE_POLICY_VERSION_MINIMUM"] = (
                "3.5"  # CMake 4 support
            )
        # For iOS/tvOS/watchOS
        tc.variables["CMAKE_MACOSX_BUNDLE"] = False

        tc.generate()

    def _patch_sources(self):
        apply_patches(self)
        # CMAKE_POSITION_INDEPENDENT_CODE was set ON for the entire
        # project in the lists file.
        replace_in_file(
            os.path.join(self.source_folder, "CMakeLists.txt"),
            "set(CMAKE_POSITION_INDEPENDENT_CODE ON)",
            "",
        )

    def build(self):
        self._patch_sources()
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(
            "LICENSE*",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        cmake = CMake(self)
        cmake.install()

        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(os.path.join(self.package_folder, "SPIRV-Tools"))
        rmdir(os.path.join(self.package_folder, "SPIRV-Tools-link"))
        rmdir(os.path.join(self.package_folder, "SPIRV-Tools-opt"))
        rmdir(os.path.join(self.package_folder, "SPIRV-Tools-reduce"))
        rmdir(os.path.join(self.package_folder, "SPIRV-Tools-lint"))
        rmdir(os.path.join(self.package_folder, "SPIRV-Tools-diff"))
        rmdir(os.path.join(self.package_folder, "SPIRV-Tools-tools"))
        if self.options.shared:
            for file_name in [
                "*SPIRV-Tools",
                "*SPIRV-Tools-opt",
                "*SPIRV-Tools-link",
                "*SPIRV-Tools-reduce",
                "*SPIRV-Tools-lint",
            ]:
                for ext in [".a", ".lib"]:
                    rm(f"{file_name}{ext}", os.path.join(self.package_folder, "lib"))
        else:
            rm("*SPIRV-Tools-shared.dll", os.path.join(self.package_folder, "bin"))
            rm("*SPIRV-Tools-shared*", os.path.join(self.package_folder, "lib"))
