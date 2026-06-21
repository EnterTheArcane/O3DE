import os

from thirdparty import RecipeBase
from thirdparty.apple import fix_apple_shared_install_name
from thirdparty.build import stdcpp_library
from thirdparty.env import VirtualBuildEnv
from thirdparty.files import copy, get, rmdir, rm, rename
from thirdparty.meson import Meson, MesonToolchain
from thirdparty.microsoft import is_msvc
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "openh264"
    version = "2.6.0"
    license = "BSD-2-Clause"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    @property
    def _is_clang_cl(self):
        return self.settings.os == 'Windows' and self.settings.compiler == 'clang'

    def build_requirements(self):
        self.tool_requires("meson")
        if not self.conf.get("tools.gnu:pkg_config", default=False, check_type=str):
            self.tool_requires("pkgconf")
        if self.settings.arch in ["X64"]:
            self.tool_requires("nasm")
        if is_msvc(self) and self.settings.arch == "ARM":
            self.tool_requires("strawberryperl")
            self.tool_requires("gas-preprocessor")

    def latest_version(self):
        repo = GithubRepository(self, "cisco/openh264")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/cisco/openh264/archive/refs/tags/v2.6.0.tar.gz",
            sha256="558544ad358283a7ab2930d69a9ceddf913f4a51ee9bf1bfb9e377322af81a69",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        env = VirtualBuildEnv(self)
        env.generate()
        tc = MesonToolchain(self)
        tc.project_options["tests"] = "disabled"
        tc.generate()

    def build(self):
        meson = Meson(self)
        meson.configure()
        meson.build()

    def package(self):
        copy(self, pattern="LICENSE", dst=os.path.join(self.package_folder, "licenses"), src=self.source_folder)
        meson = Meson(self)
        meson.install()

        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))

        if is_msvc(self) or self._is_clang_cl:
            rm(self, "*.pdb", os.path.join(self.package_folder, "bin"))
            if not self.options.shared:
                lib_dir = os.path.join(self.package_folder, "lib")
                gnu_lib = os.path.join(lib_dir, "libopenh264.a")
                # meson emits the static lib as libopenh264.a; rename to the MSVC-style
                # openh264.lib. Clear any stale destination first so a re-run doesn't fail
                # with "dst exists" (os.rename won't overwrite on Windows).
                if os.path.exists(gnu_lib):
                    rm(self, "openh264.lib", lib_dir)
                    rename(self, gnu_lib, os.path.join(lib_dir, "openh264.lib"))
        fix_apple_shared_install_name(self)

    def package_info(self):
        self.cpp_info.libs = [f"openh264"]
        if self.settings.os in ("FreeBSD", "Linux"):
            self.cpp_info.system_libs.extend(["m", "pthread"])
        if self.settings.os == "Android":
            self.cpp_info.system_libs.append("m")
        if not self.options.shared:
            libcxx = stdcpp_library(self)
            if libcxx:
                if self.settings.os == "Android" and libcxx == "c++_static":
                    # INFO: When builing for Android, need to link against c++abi too. Otherwise will get linkage errors:
                    # ld.lld: error: undefined symbol: operator new(unsigned long)
                    # >>> referenced by welsEncoderExt.cpp
                    self.cpp_info.system_libs.append("c++abi")
                self.cpp_info.system_libs.append(libcxx)
