from thirdparty import RecipeBase
from thirdparty.tools.build import check_min_cppstd
from thirdparty.tools.cmake import CMake, CMakeToolchain, CMakeDeps
from thirdparty.tools.files import apply_patches, copy, get, rmdir
from thirdparty.tools.github import GithubRepository
from thirdparty.tools.scm import Version

import os

class Recipe(RecipeBase):
    name = "libultrahdr"
    version = "1.4.0"
    license = "Apache-2.0"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_jpeg": ["libjpeg", "libjpeg-turbo", "mozjpeg"],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_jpeg": "libjpeg",
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def requirements(self):
        if self.options.with_jpeg == "libjpeg":
            self.requires("libjpeg")
        elif self.options.with_jpeg == "libjpeg-turbo":
            self.requires("libjpeg-turbo")
        elif self.options.with_jpeg == "mozjpeg":
            self.requires("mozjpeg")

    def build_requirements(self):
        # The project requires cmake 3.15 but the use of CMAKE_REQUIRE_FIND_PACKAGE_JPEG below
        # requires 3.22.
        self.tool_requires("cmake")

    def latest_version(self):
        repo = GithubRepository(self, "google/libultrahdr")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/google/libultrahdr/archive/refs/tags/v1.4.0.tar.gz",
            sha256="e7e1252e2c44d8ed6b99ee0f67a3caf2d8a61c43834b13b1c3cd485574c03ab9",
            destination=self.source_folder,
            strip_root=True)
        apply_patches(self)

    def generate(self):
        tc = CMakeToolchain(self)

        # Force-disable fallback to internal dependency builder if no deps found
        tc.cache_variables["UHDR_BUILD_DEPS"] = False
        tc.cache_variables['UHDR_BUILD_EXAMPLES'] = False
        tc.cache_variables["CMAKE_REQUIRE_FIND_PACKAGE_JPEG"] = True

        tc.generate()
        deps = CMakeDeps(self)
        if self.options.with_jpeg:
            deps.set_property(self.options.with_jpeg, "cmake_file_name", "JPEG")
            deps.set_property(self.options.with_jpeg, "cmake_target_name", "JPEG::JPEG")
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))

    def package_info(self):
        self.cpp_info.libs = ['uhdr']

        if self.options.with_jpeg == "libjpeg":
            self.cpp_info.requires = ["libjpeg::libjpeg"]
        elif self.options.with_jpeg == "libjpeg-turbo":
            self.cpp_info.requires = ["libjpeg-turbo::jpeg"]
        elif self.options.with_jpeg == "mozjpeg":
            self.cpp_info.requires = ["mozjpeg::libjpeg"]

        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs = ["pthread"]
