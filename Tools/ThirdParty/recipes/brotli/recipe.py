from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import apply_patches, copy, get, rmdir
from thirdparty.tools.scm import Version
import os


class Recipe(RecipeBase):
    name = "brotli"
    version = "1.2.0"
    license = "MIT"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "target_bits": [64, 32, None],
        "endianness": ["big", "little", "neutral", None],
        "enable_portable": [True, False],
        "enable_rbit": [True, False],
        "enable_debug": [True, False],
        "enable_log": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "target_bits": None,
        "endianness": None,
        "enable_portable": False,
        "enable_rbit": True,
        "enable_debug": False,
        "enable_log": False,
    }

    def source(self):
        get(
            url="https://github.com/google/brotli/archive/v1.2.0.tar.gz",
            dest=self.source_folder,
            sha256="816c96e8e8f193b40151dad7e8ff37b1221d019dbcb9c35cd3fadbfe6477dfec",
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BROTLI_BUNDLED_MODE"] = False
        tc.variables["BROTLI_DISABLE_TESTS"] = True
        if Version(self.version) >= "1.2.0":
            tc.variables["BROTLI_BUILD_TOOLS"] = False
        if self.options.get("target_bits") == 32:
            tc.preprocessor_definitions["BROTLI_BUILD_32_BIT"] = 1
        elif self.options.get("target_bits") == 64:
            tc.preprocessor_definitions["BROTLI_BUILD_64_BIT"] = 1
        if self.options.get("endianness") == "big":
            tc.preprocessor_definitions["BROTLI_BUILD_BIG_ENDIAN"] = 1
        elif self.options.get("endianness") == "neutral":
            tc.preprocessor_definitions["BROTLI_BUILD_ENDIAN_NEUTRAL"] = 1
        elif self.options.get("endianness") == "little":
            tc.preprocessor_definitions["BROTLI_BUILD_LITTLE_ENDIAN"] = 1
        if self.options.enable_portable:
            tc.preprocessor_definitions["BROTLI_BUILD_PORTABLE"] = 1
        if not self.options.enable_rbit:
            tc.preprocessor_definitions["BROTLI_BUILD_NO_RBIT"] = 1
        if self.options.enable_debug:
            tc.preprocessor_definitions["BROTLI_DEBUG"] = 1
        if self.options.enable_log:
            tc.preprocessor_definitions["BROTLI_ENABLE_LOG"] = 1
        if Version(self.version) < "1.1.0":
            # To install relocatable shared libs on Macos
            tc.cache_variables["CMAKE_POLICY_DEFAULT_CMP0042"] = "NEW"
            tc.cache_variables["CMAKE_POLICY_VERSION_MINIMUM"] = (
                "3.5"  # CMake 4 support
            )
        tc.generate()

    def build(self):
        apply_patches(self)
        cmake = CMake(self)
        cmake.configure()
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
        if Version(self.version) >= "1.2.0":
            rmdir(os.path.join(self.package_folder, "share"))

    def _get_decorated_lib(self, name):
        libname = name
        if Version(self.version) < "1.1.0" and not self.options.shared:
            libname += "-static"
        return libname
