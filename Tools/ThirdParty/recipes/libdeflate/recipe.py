from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import collect_libs, copy, get, rmdir
import os


class Recipe(RecipeBase):
    name = "libdeflate"
    version = "1.25"
    license = "MIT"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    def source(self):
        get(
            url="https://github.com/ebiggers/libdeflate/archive/refs/tags/v1.25.tar.gz",
            dest=self.source_folder,
            sha256="d11473c1ad4c57d874695e8026865e38b47116bbcb872bfc622ec8f37a86017d",
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["LIBDEFLATE_BUILD_STATIC_LIB"] = not self.options.shared
        tc.variables["LIBDEFLATE_BUILD_SHARED_LIB"] = self.options.shared
        tc.variables["LIBDEFLATE_BUILD_GZIP"] = False
        tc.variables["LIBDEFLATE_BUILD_TESTS"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(
            "COPYING",
            self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        cmake = CMake(self)
        cmake.install()
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
