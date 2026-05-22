# Simplified vulkan-validationlayers for Windows
import os

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.tools.files import apply_patches, copy, get, save


class Recipe(RecipeBase):
    name = "vulkan-validationlayers"
    version = "1.3.243.0"
    license = "Apache-2.0"
    options = {
        "fPIC": [True, False],
    }
    default_options = {
        "fPIC": True,
    }

    def requirements(self) -> list[str]:
        return ["vulkan-headers", "spirv-headers", "spirv-tools", "robin-hood-hashing"]

    def source(self):
        get(
            url="https://github.com/KhronosGroup/Vulkan-ValidationLayers/archive/refs/tags/sdk-1.3.243.0.tar.gz",
            dest=self.source_folder,
            sha256="fd9f6c24027de177b2fb0eb6385542d62f4c21665a8d4cc7e1c118688e0836de",
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["USE_CCACHE"] = False
        tc.variables["BUILD_WERROR"] = False
        tc.variables["BUILD_TESTS"] = False
        tc.variables["INSTALL_TESTS"] = False
        tc.variables["BUILD_LAYERS"] = True
        tc.variables["BUILD_LAYER_SUPPORT_FILES"] = True
        tc.variables["BUILD_WSI_XCB_SUPPORT"] = False
        tc.variables["BUILD_WSI_XLIB_SUPPORT"] = False
        tc.variables["BUILD_WSI_WAYLAND_SUPPORT"] = False
        if self.version >= "1.3.239":
            tc.cache_variables["VVL_CLANG_TIDY"] = False
        # Point to spirv-tools-opt cmake config
        tc.cache_variables["SPIRV-Tools-opt_DIR"] = self.build_folder.replace("\\", "/")
        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()

        # Stub SPIRV-Tools-opt config for the validation layers
        save(
            os.path.join(self.build_folder, "SPIRV-Tools-optConfig.cmake"),
            "include(CMakeFindDependencyMacro)\nfind_dependency(SPIRV-Tools)\n",
        )

    def build(self):
        apply_patches(self)
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
