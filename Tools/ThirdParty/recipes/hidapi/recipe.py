import os

from thirdparty import RecipeBase
from thirdparty.tools.apple import is_apple_os
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import copy, get, rmdir
from thirdparty.tools.scm.github import GithubRepository
from thirdparty.tools.gnu import PkgConfigDeps
from thirdparty.tools.scm import Version


class Recipe(RecipeBase):
    name = "hidapi"
    version = "0.15.0"
    license = "GPL-3.0-or-later", "BSD-3-Clause"

    options = {
        "fPIC": [True, False],
        "shared": [True, False],
    }
    default_options = {
        "fPIC": True,
        "shared": False,
    }

    @property
    def _msbuild_configuration(self):
        return "Debug" if self.settings.build_type == "Debug" else "Release"

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def requirements(self):
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.requires("libusb")
        if self.settings.os == "Linux":
            self.requires("libudev")

    def build_requirements(self):
        if self.settings.os != "Windows":
            self.tool_requires("libtool")
            if self.settings.os in ["Linux", "FreeBSD"] and not self.conf.get("tools.gnu:pkg_config", check_type=str):
                self.tool_requires("pkgconf")
            if self.settings_build.os == "Windows":
                self.win_bash = True
                if not self.conf.get("tools.microsoft.bash:path", check_type=str):
                    self.tool_requires("msys2")

    def latest_version(self):
        repo = GithubRepository(self, "libusb/hidapi")
        return Version(repo.latest_release.removeprefix("hidapi-"))

    def source(self):
        get(
            self,
            url="https://github.com/libusb/hidapi/archive/hidapi-0.15.0.tar.gz",
            sha256="5d84dec684c27b97b921d2f3b73218cb773cf4ea915caee317ac8fc73cef8136",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()
        deps = PkgConfigDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE*", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))

    def package_info(self):
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.components["libusb"].set_property("pkg_config_name", "hidapi-libusb")
            self.cpp_info.components["libusb"].libs = ["hidapi-libusb"]
            self.cpp_info.components["libusb"].requires = ["libusb::libusb"]
            self.cpp_info.components["libusb"].system_libs = ["pthread", "dl", "rt"]

            self.cpp_info.components["hidraw"].set_property("pkg_config_name", "hidapi-hidraw")
            self.cpp_info.components["hidraw"].libs = ["hidapi-hidraw"]
            if self.settings.os == "Linux":
                self.cpp_info.components["hidraw"].requires = ["libudev::libudev"]
            self.cpp_info.components["hidraw"].system_libs = ["pthread", "dl"]
        else:
            self.cpp_info.libs = ["hidapi"]
            if is_apple_os(self):
                self.cpp_info.frameworks.extend(["IOKit", "CoreFoundation", "AppKit"])
