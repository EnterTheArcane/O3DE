# Simplified vulkan-validationlayers for Windows
import os

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.tools.files import copy, get, save


class Recipe(RecipeBase):
    name = "vulkan-validationlayers"
    version = "1.4.313.0"
    license = "Apache-2.0"
    options = {
        "fPIC": [True, False],
    }
    default_options = {
        "fPIC": True,
    }

    def requirements(self) -> list[str]:
        return [
            "vulkan-headers",
            "vulkan-utility-libraries",
            "spirv-headers",
            "spirv-tools",
            "robin-hood-hashing",
        ]

    def source(self):
        get(
            url="https://github.com/KhronosGroup/Vulkan-ValidationLayers/archive/refs/tags/vulkan-sdk-1.4.313.0.tar.gz",
            dest=self.source_folder,
            sha256="49b8ee6c2352157b12b1c87eb1165bc0f82a885bc2135ad97041ac84f79aacd0",
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["BUILD_WERROR"] = False
        tc.cache_variables["BUILD_TESTS"] = False
        tc.cache_variables["VVL_CODEGEN"] = False
        # Both SPIRV-Tools and SPIRV-Tools-opt configs live in the build folder
        build_dir = self.build_folder.replace("\\", "/")
        tc.cache_variables["SPIRV-Tools_DIR"] = build_dir
        tc.cache_variables["SPIRV-Tools-opt_DIR"] = build_dir
        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()

        # Write a comprehensive SPIRV-Tools cmake config that defines the targets
        # required by vulkan-validationlayers (SPIRV-Tools, SPIRV-Tools-opt, SPIRV-Tools-static).
        spirv_tools_dep = self.dependencies.get("spirv-tools")
        if spirv_tools_dep:
            pkg = spirv_tools_dep.package_folder.replace("\\", "/")
            inc = f"{pkg}/include"
            tools_lib = f"{pkg}/lib/SPIRV-Tools.lib"
            opt_lib   = f"{pkg}/lib/SPIRV-Tools-opt.lib"
            save(
                os.path.join(self.build_folder, "SPIRV-ToolsConfig.cmake"),
                f"""\
set(SPIRV-Tools_FOUND TRUE)
if(NOT TARGET SPIRV-Tools)
  add_library(SPIRV-Tools STATIC IMPORTED GLOBAL)
  set_target_properties(SPIRV-Tools PROPERTIES
    IMPORTED_LOCATION "{tools_lib}"
    INTERFACE_INCLUDE_DIRECTORIES "{inc}"
  )
endif()
if(NOT TARGET SPIRV-Tools-opt)
  add_library(SPIRV-Tools-opt STATIC IMPORTED GLOBAL)
  set_target_properties(SPIRV-Tools-opt PROPERTIES
    IMPORTED_LOCATION "{opt_lib}"
    INTERFACE_INCLUDE_DIRECTORIES "{inc}"
    INTERFACE_LINK_LIBRARIES "SPIRV-Tools"
  )
endif()
if(NOT TARGET SPIRV-Tools-static)
  add_library(SPIRV-Tools-static ALIAS SPIRV-Tools)
endif()
""",
            )

        # SPIRV-Tools-opt stub: just ensure SPIRV-Tools is found (targets are defined above).
        save(
            os.path.join(self.build_folder, "SPIRV-Tools-optConfig.cmake"),
            "include(CMakeFindDependencyMacro)\nfind_dependency(SPIRV-Tools)\n",
        )

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(
            "LICENSE.md",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        cmake = CMake(self)
        cmake.install()
