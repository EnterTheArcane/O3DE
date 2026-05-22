from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import apply_patches, copy, get, rmdir
from thirdparty.tools.scm import Version
import os


class Recipe(RecipeBase):
    name = "xxhash"
    version = "0.8.3"
    license = "BSD-2-Clause"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "utility": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "utility": True,
    }

    def source(self):
        get(
            url="https://github.com/Cyan4973/xxHash/archive/v0.8.3.tar.gz",
            dest=self.source_folder,
            sha256="aae608dfe8213dfd05d909a57718ef82f30722c392344583d3f39050c7f29a80",
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["XXHASH_BUNDLED_MODE"] = False
        tc.variables["XXHASH_BUILD_XXHSUM"] = self.options.utility
        # Fix CMake configuration if target is iOS/tvOS/watchOS
        tc.cache_variables["CMAKE_MACOSX_BUNDLE"] = False
        # Generate a relocatable shared lib on Macos
        tc.cache_variables["CMAKE_POLICY_DEFAULT_CMP0042"] = "NEW"
        if Version(self.version) < "0.8.3":
            tc.cache_variables["CMAKE_POLICY_VERSION_MINIMUM"] = (
                "3.5"  # CMake 4 support
            )
        tc.generate()

    def build(self):
        apply_patches(self)
        cmake = CMake(self)
        cmake.configure(
            build_script_folder=os.path.join(self.source_folder, "cmake_unofficial")
        )
        cmake.build()

    def package(self):
        copy(
            "LICENSE",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        cmake = CMake(self)
        cmake.install()
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(os.path.join(self.package_folder, "share"))
