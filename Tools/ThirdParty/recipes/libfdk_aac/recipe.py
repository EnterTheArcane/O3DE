import os

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import copy, get
from thirdparty.tools.scm import Version
from thirdparty.tools.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "libfdk_aac"
    version = "2.0.3"
    license = "https://github.com/mstorsjo/fdk-aac/blob/master/NOTICE"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def latest_version(self):
        repo = GithubRepository(self, "mstorsjo/fdk-aac")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://sourceforge.net/projects/opencore-amr/files/fdk-aac/fdk-aac-2.0.3.tar.gz",
            sha256="829b6b89eef382409cda6857fd82af84fabb63417b08ede9ea7a553f811cb79e",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_PROGRAMS"] = False
        tc.variables["FDK_AAC_INSTALL_CMAKE_CONFIG_MODULE"] = False
        tc.variables["FDK_AAC_INSTALL_PKGCONFIG_MODULE"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "NOTICE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "fdk-aac")
        self.cpp_info.set_property("cmake_target_name", "FDK-AAC::fdk-aac")
        self.cpp_info.set_property("pkg_config_name", "fdk-aac")

        self.cpp_info.components["fdk-aac"].libs = ["fdk-aac"]
        if self.settings.os in ["Linux", "FreeBSD", "Android"]:
            self.cpp_info.components["fdk-aac"].system_libs.append("m")

        self.cpp_info.components["fdk-aac"].set_property("cmake_target_name", "FDK-AAC::fdk-aac")
