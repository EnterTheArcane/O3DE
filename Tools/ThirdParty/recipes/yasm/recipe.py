import os

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.env import VirtualBuildEnv
from thirdparty.tools.files import apply_patches, copy, get, rmdir
from thirdparty.tools.gnu import Autotools, AutotoolsToolchain
from thirdparty.tools.microsoft import is_msvc


class Recipe(RecipeBase):
    name = "yasm"
    version = "1.3.0"
    license = "BSD-2-Clause"

    def package_id(self):
        del self.info.settings.compiler

    def build_requirements(self):
        self.tool_requires("cmake")
        if self.settings.os == "Windows" and not is_msvc(self):
            self.win_bash = True
            if not self.conf.get("tools.microsoft.bash:path", check_type=str):
                self.tool_requires("msys2")

    def source(self):
        get(
            self,
            url="http://www.tortall.net/projects/yasm/releases/yasm-1.3.0.tar.gz",
            sha256="3dce6601b495f5b3d45b59f7d2492a340ee7e84b5beca17e48f862502bd5603f",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        if is_msvc(self):
            tc = CMakeToolchain(self)
            tc.cache_variables["YASM_BUILD_TESTS"] = False
            tc.cache_variables["BUILD_SHARED_LIBS"] = False
            tc.generate()
        else:
            env = VirtualBuildEnv(self)
            env.generate()
            tc = AutotoolsToolchain(self)
            enable_debug = "yes" if self.settings.build_type == "Debug" else "no"
            tc.configure_args.extend([
                f"--enable-debug={enable_debug}",
                "--disable-rpath",
                "--disable-nls",
            ])
            tc.generate()

    def build(self):
        apply_patches(self)
        if is_msvc(self):
            cmake = CMake(self)
            cmake.configure()
            cmake.build()
        else:
            autotools = Autotools(self)
            autotools.configure()
            autotools.make()

    def package(self):
        copy(self, "BSD.txt", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy(self, "COPYING", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        if is_msvc(self):
            cmake = CMake(self)
            cmake.install()
            rmdir(self, os.path.join(self.package_folder, "include"))
            rmdir(self, os.path.join(self.package_folder, "lib"))
        else:
            autotools = Autotools(self)
            autotools.install()
            rmdir(self, os.path.join(self.package_folder, "share"))
            rmdir(self, os.path.join(self.package_folder, "lib"))

    def package_info(self):
        self.cpp_info.includedirs = []
        self.cpp_info.libdirs = []
