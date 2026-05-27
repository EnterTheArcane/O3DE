import os

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeConfigDeps, CMakeToolchain
from thirdparty.tools.files import apply_patches, copy, get, rmdir, replace_in_file, save
from thirdparty.tools.scm import Version
from thirdparty.tools.scm.github import GithubRepository


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

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def requirements(self):
        self.requires("zlib")
        # Note: OpenEXR and Imath are versioned independently.
        self.requires("imath", transitive_headers=True)
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
            destination=self.source_folder,
            strip_root=True)
        replace_in_file(
            self,
            os.path.join(self.source_folder, "CMakeLists.txt"),
            "add_subdirectory(website/src)",
            "# add_subdirectory(website/src)")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["OPENEXR_INSTALL_EXAMPLES"] = False
        tc.variables["BUILD_TESTING"] = False
        tc.variables["BUILD_WEBSITE"] = False
        tc.variables["DOCS"] = False
        tc.generate()

        cd = CMakeConfigDeps(self)
        cd.set_property("openjph", "cmake_target_name", "openjph")
        cd.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE.md", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "share"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))

    @staticmethod
    def _conan_comp(name):
        return f"openexr_{name.lower()}"

    def _add_component(self, name):
        component = self.cpp_info.components[self._conan_comp(name)]
        component.set_property("cmake_target_name", f"OpenEXR::{name}")
        return component

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "OpenEXR")
        self.cpp_info.set_property("pkg_config_name", "OpenEXR")

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
        Iex.requires = [self._conan_comp("IexConfig")]
        if self.settings.os in ["Linux", "FreeBSD"]:
            Iex.system_libs = ["m"]

        # OpenEXR::IlmThread
        IlmThread = self._add_component("IlmThread")
        IlmThread.libs = [f"IlmThread{lib_suffix}"]
        IlmThread.requires = [
            self._conan_comp("IlmThreadConfig"), self._conan_comp("Iex"),
        ]
        if self.settings.os in ["Linux", "FreeBSD"]:
            IlmThread.system_libs = ["pthread", "m"]

        # OpenEXR::OpenEXRCore
        OpenEXRCore = self._add_component("OpenEXRCore")
        OpenEXRCore.libs = [f"OpenEXRCore{lib_suffix}"]
        OpenEXRCore.requires = [self._conan_comp("OpenEXRConfig"), "zlib::zlib"]
        OpenEXRCore.requires.append("libdeflate::libdeflate")
        OpenEXRCore.requires.append("openjph::openjph")
        if self.settings.os in ["Linux", "FreeBSD"]:
            OpenEXRCore.system_libs = ["m"]

        # OpenEXR::OpenEXR
        OpenEXR = self._add_component("OpenEXR")
        OpenEXR.libs = [f"OpenEXR{lib_suffix}"]
        OpenEXR.requires = [
            self._conan_comp("OpenEXRCore"), self._conan_comp("IlmThread"),
            self._conan_comp("Iex"), "imath::imath",
        ]
        if self.settings.os in ["Linux", "FreeBSD"]:
            OpenEXR.system_libs = ["m"]

        # OpenEXR::OpenEXRUtil
        OpenEXRUtil = self._add_component("OpenEXRUtil")
        OpenEXRUtil.libs = [f"OpenEXRUtil{lib_suffix}"]
        OpenEXRUtil.requires = [self._conan_comp("OpenEXR")]
        if self.settings.os in ["Linux", "FreeBSD"]:
            OpenEXRUtil.system_libs = ["m"]

        # TODO: Add tools directory to PATH
