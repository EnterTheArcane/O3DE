import os

from thirdparty import RecipeBase
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import get, copy, rmdir, apply_patches, replace_in_file
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "clipper2"
    version = "2.0.1"
    license = "BSL-2.0"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "usingz": ["ON", "OFF", "ONLY"],
        "with_max_precision": ["ANY"],
        "with_hi_precision": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "usingz": "ON",
        "with_max_precision": 8,
        "with_hi_precision": False,
    }

    def latest_version(self):
        repo = GithubRepository(self, "AngusJohnson/Clipper2")
        return Version(repo.latest_release.removeprefix("Clipper2_"))

    def source(self):
        get(
            self,
            url="https://github.com/AngusJohnson/Clipper2/archive/refs/tags/Clipper2_2.0.1.tar.gz",
            sha256="2a3693aceab4aed3e39b743e038d87701acc53cf05ed7b2013aab3e0aec5287e",
            destination=self.source_folder,
            strip_root=True)
        apply_patches(self)
        replace_in_file(self, os.path.join(self.source_folder, "CPP", "CMakeLists.txt"), "-Werror", "")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS"] = True
        tc.variables["CLIPPER2_UTILS"] = False
        tc.variables["CLIPPER2_EXAMPLES"] = False
        tc.variables["CLIPPER2_TESTS"] = False
        tc.variables["CLIPPER2_USINGZ"] = self.options.usingz
        if "with_hi_precision" in self.options:
            tc.variables["CLIPPER2_HI_PRECISION"] = self.options.with_hi_precision
        if "with_max_precision" in self.options:
            tc.variables["CLIPPER2_MAX_PRECISION"] = self.options.with_max_precision
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure(build_script_folder=os.path.join(self.source_folder, "CPP"))
        cmake.build()

    def package(self):
        copy(self, pattern="LICENSE", dst=os.path.join(self.package_folder, "licenses"), src=self.source_folder)
        cmake = CMake(self)
        cmake.install()

        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))

    def package_info(self):
        if self.options.usingz != "ONLY":
            self.cpp_info.components["clipper2"].set_property("cmake_target_name", "Clipper2::clipper2")
            self.cpp_info.components["clipper2"].set_property("pkg_config_name", "Clipper2")
            self.cpp_info.components["clipper2"].libs = ["Clipper2"]
            if self.settings.os in ["Linux", "FreeBSD"]:
                self.cpp_info.components["clipper2"].system_libs.append("m")

        if self.options.usingz != "OFF":
            self.cpp_info.components["clipper2z"].set_property("cmake_target_name", "Clipper2::clipper2z")
            self.cpp_info.components["clipper2z"].set_property("pkg_config_name", "Clipper2Z")
            self.cpp_info.components["clipper2z"].libs = ["Clipper2Z"]
            self.cpp_info.components["clipper2z"].defines.append("USINGZ")
            if self.settings.os in ["Linux", "FreeBSD"]:
                self.cpp_info.components["clipper2z"].system_libs.append("m")
