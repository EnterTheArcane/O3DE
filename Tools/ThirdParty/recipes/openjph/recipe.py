import os

from thirdparty import RecipeBase, RecipeOptions
from thirdparty.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.files import copy, get, rmdir
from thirdparty.microsoft import is_msvc
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class _Options(RecipeOptions):
    shared: bool = False
    fPIC: bool = True


class Recipe(RecipeBase[_Options]):
    name = "openjph"
    version = "0.27.3"
    license = "BSD-2-Clause"

    def requirements(self):
        self.requires("libtiff")

    def latest_version(self):
        repo = GithubRepository(self, "aous72/OpenJPH")
        return Version(repo.latest_release)

    def source(self):
        get(
            self,
            url="https://github.com/aous72/OpenJPH/archive/0.27.3.tar.gz",
            sha256="f96808ef72cf3acca73a52123bda3e680f6550dfb4774ad7de57eb3ce26de57a",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["OJPH_BUILD_EXECUTABLES"] = True
        tc.cache_variables["OJPH_ENABLE_TIFF_SUPPORT"] = True
        tc.cache_variables["OJPH_BUILD_STREAM_EXPAND"] = False
        tc.cache_variables["OJPH_DISABLE_SIMD"] = False
        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cm = CMake(self)
        cm.configure()
        cm.build()

    def package(self):
        cm = CMake(self)
        cm.install()

        copy(self, "LICENSE", src=self.folders.source, dst=os.path.join(self.folders.package, "licenses"))
        rmdir(self, os.path.join(self.folders.package, "lib", "cmake"))
        rmdir(self, os.path.join(self.folders.package, "lib", "pkgconfig"))

    def package_info(self):
        self.info.set_property("cmake_file_name", "openjph")
        self.info.set_property("cmake_target_name", "openjph::openjph")
        self.info.set_property("pkg_config_name", "openjph")

        version_suffix = "_d" if self.settings.build_type == "Debug" else ""
        if is_msvc(self):
            v = Version(self.version)
            version_suffix = f".{v.major}.{v.minor}"
            if self.settings.build_type == "Debug":
                version_suffix += "d"
        self.info.libs = ["openjph" + version_suffix]
