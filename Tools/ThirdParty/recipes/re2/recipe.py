from thirdparty import RecipeBase
from thirdparty.tools.build import check_min_cppstd
from thirdparty.tools.cmake import CMake, CMakeToolchain, CMakeDeps
from thirdparty.tools.files import copy, get, rmdir
from thirdparty.tools.github import GithubRepository
from thirdparty.tools.scm import Version
import os

class Recipe(RecipeBase):
    name = "re2"
    version = "20251105"
    license = "BSD-3-Clause"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_icu": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_icu": False,
    }

    implements = ["auto_shared_fpic"]

    def requirements(self):
        if self.options.get_safe("with_icu"):
            self.requires("icu")
        self.requires("abseil", transitive_headers=True)

    def build_requirements(self):
        self.tool_requires("cmake")

    def latest_version(self):
        repo = GithubRepository(self, "google/re2")
        return Version(repo.latest_release.replace("-", ""))

    def source(self):
        get(
            self,
            url="https://github.com/google/re2/releases/download/2025-11-05/re2-2025-11-05.tar.gz",
            sha256="87f6029d2f6de8aa023654240a03ada90e876ce9a4676e258dd01ea4c26ffd67",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["RE2_BUILD_TESTING"] = False
        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "re2")
        self.cpp_info.set_property("cmake_target_name", "re2::re2")
        self.cpp_info.set_property("pkg_config_name", "re2")
        self.cpp_info.libs = ["re2"]
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs = ["m", "pthread"]
