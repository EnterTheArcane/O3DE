from thirdparty import RecipeBase as ConanFile
from thirdparty.tools.apple import fix_apple_shared_install_name, is_apple_os
from thirdparty.tools.build import cross_building
from thirdparty.tools.env import VirtualBuildEnv, VirtualRunEnv
from thirdparty.tools.files import apply_conandata_patches, copy, get, rm, rmdir
from thirdparty.tools.gnu import Autotools, AutotoolsDeps, AutotoolsToolchain
import os

class Recipe(ConanFile):
    name = "gdbm"
    version = "1.23"
    license = "GPL-3.0-or-later"
    package_type = "library"
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "libgdbm_compat": [True, False],
        "gdbmtool_debug": [True, False],
        "with_libiconv": [True, False],
        "with_readline": [True, False],
        "with_nls": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "libgdbm_compat": False,
        "gdbmtool_debug": True,
        "with_libiconv": False,
        "with_readline": False,
        "with_nls": True,
    }

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        self.settings.rm_safe("compiler.libcxx")
        self.settings.rm_safe("compiler.cppstd")
        if not self.options.with_nls:
            self.options.rm_safe("with_libiconv")

    def validate(self):
        from thirdparty._conan.errors import ConanInvalidConfiguration
        if self.settings.os == "Windows":
            raise ConanInvalidConfiguration(f"{self.name} is not supported on Windows")

    def requirements(self):
        if self.options.get_safe("with_libiconv"):
            self.requires("libiconv/1.17")
        if self.options.with_readline:
            self.requires("readline/8.1.2")

    def build_requirements(self):
        self.tool_requires("bison/3.8.2")
        self.tool_requires("flex/2.6.4")
        self.tool_requires("gnu-config/cci.20210814")

    def source(self):
        get(self, url="https://ftp.gnu.org/gnu/gdbm/gdbm-1.23.tar.gz", sha256="74b1081d21fff13ae4bd7c16e5d6e504a4c26f7cde1dca0d963a484174bbcacd", destination=self.source_folder, strip_root=True)

    def generate(self):
        virtual_build_env = VirtualBuildEnv(self)
        virtual_build_env.generate()
        if not cross_building(self):
            virtual_run_env = VirtualRunEnv(self)
            virtual_run_env.generate(scope="build")
        tc = AutotoolsToolchain(self)
        yes_no = lambda v: "yes" if v else "no"
        enable_debug = self.settings.build_type in ["Debug", "RelWithDebInfo"]
        tc.configure_args.extend([
            f"--enable-debug={yes_no(enable_debug)}",
            f"--enable-libgdbm-compat={yes_no(self.options.libgdbm_compat)}",
            f"--enable-gdbmtool-debug={yes_no(self.options.gdbmtool_debug)}",
            f"--enable-nls={yes_no(self.options.with_nls)}",
            f"--with-readline={yes_no(self.options.with_readline)}",
            f"--with-pic={yes_no(self.options.get_safe('fPIC', True))}",
        ])
        if self.options.gdbmtool_debug:
            tc.extra_defines.append("YYDEBUG=1")
        if self.options.get_safe("with_libiconv"):
            libiconv_package_folder = self.dependencies.direct_host["libiconv"].package_folder
            tc.configure_args.extend([
                f"--with-libiconv-prefix={libiconv_package_folder}"
                "--with-libintl-prefix"
            ])
        else:
            tc.configure_args.extend([
                "--without-libiconv-prefix",
                "--without-libintl-prefix"
            ])
        if is_apple_os(self):
            # Inject -headerpad_max_install_names, otherwise fix_apple_shared_install_name() may fail.
            # See https://github.com/conan-io/conan-center-index/issues/20002
            tc.extra_ldflags.append("-headerpad_max_install_names")
        tc.generate()
        autotools_deps = AutotoolsDeps(self)
        autotools_deps.generate()

    def _patch_sources(self):
        apply_conandata_patches(self)
        for gnu_config in [
            self.conf.get("user.gnu-config:config_guess", check_type=str),
            self.conf.get("user.gnu-config:config_sub", check_type=str),
        ]:
            if gnu_config:
                copy(self, os.path.basename(gnu_config), os.path.dirname(gnu_config), os.path.join(self.source_folder, "build-aux"))

    def build(self):
        self._patch_sources()
        autotools = Autotools(self)
        autotools.configure()
        autotools.make(target="maintainer-clean-generic")
        autotools.make()

    def package(self):
        copy(self, "COPYING", self.source_folder, os.path.join(self.package_folder, "licenses"))
        autotools = Autotools(self)
        autotools.install()
        rm(self, "*.la", os.path.join(self.package_folder, "lib"))
        rmdir(self, os.path.join(self.package_folder, "share"))
        fix_apple_shared_install_name(self)

    def package_info(self):
        if self.options.libgdbm_compat:
            self.cpp_info.libs.append("gdbm_compat")
        self.cpp_info.libs.append("gdbm")

        bin_path = os.path.join(self.package_folder, "bin")
        self.output.info("Appending PATH environment variable: {}".format(bin_path))
        self.env_info.PATH.append(bin_path)
