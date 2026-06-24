import os

from thirdparty import RecipeBase
from thirdparty.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.files import copy, get, rmdir, replace_in_file
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "openexr"
    version = "3.4.12"
    license = "BSD-3-Clause"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    def requirements(self):
        self.requires("zlib")
        # Note: OpenEXR and Imath are versioned independently.
        self.requires("imath")
        self.requires("libdeflate")

        self.requires("openjph")

    def latest_version(self):
        repo = GithubRepository(self, "AcademySoftwareFoundation/openexr")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/AcademySoftwareFoundation/openexr/releases/download/v3.4.12/openexr-3.4.12.tar.gz",
            sha256="2d45db1d4bb78a5b263cd21cefa93119e1fcd37a13fa446c74663b6b8ec02d00",
            destination=self.folders.source,
            strip_root=True)
        replace_in_file(
            self,
            os.path.join(self.folders.source, "CMakeLists.txt"),
            "add_subdirectory(website/src)",
            "# add_subdirectory(website/src)")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["OPENEXR_INSTALL_EXAMPLES"] = False
        tc.variables["BUILD_TESTING"] = False
        tc.variables["BUILD_WEBSITE"] = False
        tc.variables["DOCS"] = False
        tc.generate()

        deps = CMakeDeps(self)
        deps.set_property("openjph", "cmake_target_name", "openjph")
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE.md", src=self.folders.source, dst=os.path.join(self.folders.package, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.folders.package, "share"))
        rmdir(self, os.path.join(self.folders.package, "lib", "pkgconfig"))
        rmdir(self, os.path.join(self.folders.package, "lib", "cmake"))

    @staticmethod
    def _recipe_comp(name):
        return f"openexr_{name.lower()}"

    def _add_component(self, name):
        component = self.info.components[self._recipe_comp(name)]
        component.set_property("cmake_target_name", f"OpenEXR::{name}")
        return component

    def package_info(self):
        self.info.set_property("cmake_file_name", "OpenEXR")
        self.info.set_property("pkg_config_name", "OpenEXR")

        lib_suffix = ""
        if not self.options.shared or self.settings.os == "Windows":
            openexr_version = Version(self.version)
            lib_suffix += f"-{openexr_version.major}_{openexr_version.minor}"
        if self.settings.build_type == "Debug":
            lib_suffix += "_d"

        # OpenEXR::OpenEXRConfig
        OpenEXRConfig = self._add_component("OpenEXRConfig")
        OpenEXRConfig.includedirs.append(os.path.join("include", "OpenEXR"))

        # OpenEXR::IexConfig
        IexConfig = self._add_component("IexConfig")
        IexConfig.includedirs = OpenEXRConfig.includedirs

        # OpenEXR::IlmThreadConfig
        IlmThreadConfig = self._add_component("IlmThreadConfig")
        IlmThreadConfig.includedirs = OpenEXRConfig.includedirs

        # OpenEXR::Iex
        Iex = self._add_component("Iex")
        Iex.libs = [f"Iex{lib_suffix}"]
        Iex.requires = [self._recipe_comp("IexConfig")]
        if self.settings.os in ["Linux", "FreeBSD"]:
            Iex.system_libs = ["m"]

        # OpenEXR::IlmThread
        IlmThread = self._add_component("IlmThread")
        IlmThread.libs = [f"IlmThread{lib_suffix}"]
        IlmThread.requires = [
            self._recipe_comp("IlmThreadConfig"), self._recipe_comp("Iex"),
        ]
        if self.settings.os in ["Linux", "FreeBSD"]:
            IlmThread.system_libs = ["pthread", "m"]

        # OpenEXR::OpenEXRCore
        OpenEXRCore = self._add_component("OpenEXRCore")
        OpenEXRCore.libs = [f"OpenEXRCore{lib_suffix}"]
        OpenEXRCore.requires = [self._recipe_comp("OpenEXRConfig"), "zlib::zlib"]
        OpenEXRCore.requires.append("libdeflate::libdeflate")
        OpenEXRCore.requires.append("openjph::openjph")
        if self.settings.os in ["Linux", "FreeBSD"]:
            OpenEXRCore.system_libs = ["m"]

        # OpenEXR::OpenEXR
        OpenEXR = self._add_component("OpenEXR")
        OpenEXR.libs = [f"OpenEXR{lib_suffix}"]
        OpenEXR.requires = [
            self._recipe_comp("OpenEXRCore"), self._recipe_comp("IlmThread"),
            self._recipe_comp("Iex"), "imath::imath",
        ]
        if self.settings.os in ["Linux", "FreeBSD"]:
            OpenEXR.system_libs = ["m"]

        # OpenEXR::OpenEXRUtil
        OpenEXRUtil = self._add_component("OpenEXRUtil")
        OpenEXRUtil.libs = [f"OpenEXRUtil{lib_suffix}"]
        OpenEXRUtil.requires = [self._recipe_comp("OpenEXR")]
        if self.settings.os in ["Linux", "FreeBSD"]:
            OpenEXRUtil.system_libs = ["m"]

        # TODO: Add tools directory to PATH
