import os

from thirdparty import RecipeBase
from thirdparty.cmake import CMakeToolchain, CMake
from thirdparty.files import copy, get, rmdir
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "utfcpp"
    version = "4.1.1"
    license = "BSL-1.0"

    def latest_version(self):
        repo = GithubRepository(self, "nemtrif/utfcpp")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/nemtrif/utfcpp/archive/v4.1.1.tar.gz",
            sha256="1ca68016f0abc24172998e39ce0d8f8e2b7a26f7579a0ff85d4e1b9a7aea56f8",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.folders.source, dst=os.path.join(self.folders.package, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.folders.package, "lib"))
        rmdir(self, os.path.join(self.folders.package, "share"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "utf8cpp")
        self.cpp_info.set_property("cmake_target_name", "utf8cpp::utf8cpp")
        # FIXME: Keep CMake target utf8cpp for backward compatibility as more projects are using it in CCI.
        self.cpp_info.set_property("cmake_target_aliases", ["utf8::cpp", "utf8cpp"])
        self.cpp_info.includedirs.append(os.path.join("include", "utf8cpp"))

        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []
