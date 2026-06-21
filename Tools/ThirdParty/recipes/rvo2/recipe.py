import os

from thirdparty import RecipeBase
from thirdparty.apple import fix_apple_shared_install_name
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import copy, get, replace_in_file
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "rvo2"
    version = "2.0.2"
    license = "Apache-2.0"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    def latest_version(self):
        repo = GithubRepository(self, "snape/RVO2")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/snape/RVO2/archive/v2.0.2.tar.gz",
            sha256="20b59fcc4cf61783cb0d1baa40a0dff3c557a97246651f95d9d9fed91bf17724",
            destination=self.source_folder,
            strip_root=True)
        replace_in_file(self, os.path.join(self.source_folder, "CMakeLists.txt"), "add_subdirectory(examples)", "")
        replace_in_file(
            self,
            os.path.join(self.source_folder, "src", "CMakeLists.txt"),
            "DESTINATION include",
            "DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}",
        )
        replace_in_file(
            self,
            os.path.join(self.source_folder, "src", "CMakeLists.txt"),
            "RVO DESTINATION lib",
            "RVO RUNTIME LIBRARY ARCHIVE",
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS"] = True
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        fix_apple_shared_install_name(self)

    def package_info(self):
        self.cpp_info.libs = ["RVO"]
