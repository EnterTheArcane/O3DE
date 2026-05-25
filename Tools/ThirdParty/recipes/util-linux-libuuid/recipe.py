from thirdparty import RecipeBase
from thirdparty.tools.apple import fix_apple_shared_install_name, is_apple_os, XCRun
from thirdparty.tools.files import copy, get, rm, rmdir
from thirdparty.tools.gnu import Autotools, AutotoolsToolchain, AutotoolsDeps
from thirdparty.tools.scm import Version
import os


class Recipe(RecipeBase):
    name = "util-linux-libuuid"
    version = "2.41.2"
    license = "BSD-3-Clause"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_python_bindings": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_python_bindings": False,
    }

    @property
    def _has_sys_file_header(self):
        return self.settings.os in ["FreeBSD", "Linux", "Macos"]

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")
        
    def validate(self):
        from thirdparty._conan.errors import ConanInvalidConfiguration
        if self.settings.os != "Linux":
            raise ConanInvalidConfiguration(f"{self.name} is only supported on Linux")

    def requirements(self):
        if self.settings.os == "Macos":
            self.requires("libgettext")

    def source(self):
        get(
            self,
            url="https://mirrors.edge.kernel.org/pub/linux/utils/util-linux/v2.41/util-linux-2.41.2.tar.xz",
            sha256="6062a1d89b571a61932e6fc0211f36060c4183568b81ee866cf363bce9f6583e",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        tc = AutotoolsToolchain(self)
        tc.configure_args.append("--disable-all-programs")
        tc.configure_args.append("--enable-libuuid")
        if not self.options.with_python_bindings:
            tc.configure_args.append("--without-python")
        if self._has_sys_file_header:
            tc.extra_defines.append("HAVE_SYS_FILE_H")
        if "x86" in self.settings.arch:
            tc.extra_cflags.append("-mstackrealign")

        # Based on https://github.com/conan-io/conan-center-index/blob/c647b1/recipes/libx264/all/conanfile.py#L94
        if is_apple_os(self) and self.settings.arch == "armv8":
            tc.configure_args.append("--host=aarch64-apple-darwin")
            tc.extra_asflags = ["-arch arm64"]
            tc.extra_ldflags = ["-arch arm64"]
            if self.settings.os != "Macos":
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
        copy(self, "COPYING.BSD-3-Clause", src=os.path.join(self.source_folder, "Documentation", "licenses"), dst=os.path.join(self.package_folder, "licenses"))
        autotools = Autotools(self)
        autotools.install()
        rm(self, "*.la", os.path.join(self.package_folder, "lib"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(self, os.path.join(self.package_folder, "bin"))
        rmdir(self, os.path.join(self.package_folder, "sbin"))
        rmdir(self, os.path.join(self.package_folder, "share"))
        rmdir(self, os.path.join(self.package_folder, "usr"))
        fix_apple_shared_install_name(self)

    def package_info(self):
        self.cpp_info.set_property("pkg_config_name", "uuid")
        self.cpp_info.set_property("cmake_target_name", "libuuid::libuuid")
        self.cpp_info.set_property("cmake_file_name", "libuuid")
        # Maintain alias to `LibUUID::LibUUID` for previous version of the recipe
        self.cpp_info.set_property("cmake_target_aliases", ["LibUUID::LibUUID"])

        self.cpp_info.libs = ["uuid"]
        self.cpp_info.includedirs.append(os.path.join("include", "uuid"))

        if self.settings.os == "Linux" and Version(self.version) >= "2.41.2":
            self.cpp_info.system_libs.extend(["pthread"])
