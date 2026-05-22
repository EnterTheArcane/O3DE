from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import apply_patches, copy, get, rmdir
from thirdparty.tools.microsoft import check_min_vs, is_msvc, is_msvc_static_runtime
from thirdparty.tools.scm import Version
import os


class Recipe(RecipeBase):
    name = "opus"
    version = "1.5.2"
    license = "BSD-3-Clause"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "fixed_point": [True, False],
        "stack_protector": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "fixed_point": False,
        "stack_protector": True,
    }
    implements = ["auto_shared_fpic"]
    languages = "C"

    def source(self):
        get(
            url="https://github.com/xiph/opus/releases/download/v1.5.2/opus-1.5.2.tar.gz",
            sha256="65c1d2f78b9f2fb20082c38cbe47c951ad5839345876e46941612ee87f9a7ce1",
            dest=self.source_folder,
            strip_root=True,
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["OPUS_BUILD_SHARED_LIBRARY"] = self.options.shared
        tc.cache_variables["OPUS_FIXED_POINT"] = self.options.fixed_point
        tc.cache_variables["OPUS_STACK_PROTECTOR"] = self.options.stack_protector
        if self.is_windows:
            tc.cache_variables["OPUS_STATIC_RUNTIME"] = False
        tc.generate()

    def build(self):
        apply_patches(self)
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(
            "COPYING",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        cmake = CMake(self)
        cmake.install()
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
