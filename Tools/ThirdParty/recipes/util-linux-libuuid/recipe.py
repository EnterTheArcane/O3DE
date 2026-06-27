import os

from thirdparty import RecipeBase, RecipeOptions
from thirdparty.apple import fix_apple_shared_install_name, is_apple_os, XCRun
from thirdparty.env import VirtualBuildEnv
from thirdparty.files import copy, get, rm, rmdir
from thirdparty.autotools import Autotools, AutotoolsToolchain, AutotoolsDeps


class _Options(RecipeOptions):
    shared: bool = False
    fPIC: bool = True
    with_python_bindings: bool = False


class Recipe(RecipeBase[_Options]):
    name = "util-linux-libuuid"
    version = "2.41.2"
    license = "BSD-3-Clause"

    @property
    def _has_sys_file_header(self):
        return self.settings.os in ["Linux", "Mac"]

    def configure(self):
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def validate(self):
        from thirdparty.errors import RecipeInvalidConfiguration
        if self.settings.os != "Linux":
            raise RecipeInvalidConfiguration(f"{self.name} is only supported on Linux")

    def requirements(self):
        if self.settings.os == "Mac":
            self.requires("libgettext")

    def source(self):
        get(
            self,
            url="https://mirrors.edge.kernel.org/pub/linux/utils/util-linux/v2.41/util-linux-2.41.2.tar.xz",
            sha256="6062a1d89b571a61932e6fc0211f36060c4183568b81ee866cf363bce9f6583e",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        VirtualBuildEnv(self).generate()

        tc = AutotoolsToolchain(self)
        tc.configure_args.append("--disable-all-programs")
        tc.configure_args.append("--enable-libuuid")
        if not self.options.with_python_bindings:
            tc.configure_args.append("--without-python")
        if self._has_sys_file_header:
            tc.extra_defines.append("HAVE_SYS_FILE_H")
        if "x86" in self.settings.arch:
            tc.extra_cflags.append("-mstackrealign")

        # Based on https://github.com/recipe-io/recipe-center-index/blob/c647b1/recipes/libx264/all/recipe.py#L94
        if is_apple_os(self) and self.settings.arch == "ARM":
            tc.configure_args.append("--host=aarch64-apple-darwin")
            tc.extra_asflags = ["-arch arm64"]
            tc.extra_ldflags = ["-arch arm64"]
            if self.settings.os != "Mac":
                xcrun = XCRun(self)
                platform_flags = ["-isysroot", xcrun.sdk_path]
                apple_min_version_flag = AutotoolsToolchain(self).apple_min_version_flag
                if apple_min_version_flag:
                    platform_flags.append(apple_min_version_flag)
                tc.extra_asflags.extend(platform_flags)
                tc.extra_cflags.extend(platform_flags)
                tc.extra_ldflags.extend(platform_flags)

        tc.generate()

        deps = AutotoolsDeps(self)
        deps.generate()
        deps.generate()

    def build(self):
        autotools = Autotools(self)
        autotools.configure()
        autotools.make()

    def package(self):
        copy(self, "COPYING.BSD-3-Clause", src=self.folders.source / "Documentation" / "licenses", dst=self.folders.package / "licenses")
        autotools = Autotools(self)
        autotools.install()
        rm(self, "*.la", self.folders.package / "lib")
        rmdir(self, self.folders.package / "lib" / "pkgconfig")
        rmdir(self, self.folders.package / "bin")
        rmdir(self, self.folders.package / "sbin")
        rmdir(self, self.folders.package / "share")
        rmdir(self, self.folders.package / "usr")
        fix_apple_shared_install_name(self)

    def package_info(self):
        self.info.set_property("pkg_config_name", "uuid")
        self.info.set_property("cmake_target_name", "libuuid::libuuid")
        self.info.set_property("cmake_file_name", "libuuid")
        # Maintain alias to `LibUUID::LibUUID` for previous version of the recipe
        self.info.set_property("cmake_target_aliases", ["LibUUID::LibUUID"])

        self.info.libs = ["uuid"]
        self.info.includedirs.append(os.path.join("include", "uuid"))

        if self.settings.os == "Linux":
            self.info.system_libs.extend(["pthread"])
