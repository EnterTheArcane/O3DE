import os

from thirdparty import RecipeBase
from thirdparty.apple import fix_apple_shared_install_name, is_apple_os
from thirdparty.build import cross_building
from thirdparty.env import VirtualBuildEnv, VirtualRunEnv
from thirdparty.files import apply_patches, copy, get, rm, rmdir
from thirdparty.gnu import Autotools, AutotoolsDeps, AutotoolsToolchain, GnuFtp
from thirdparty.scm import Version


class Recipe(RecipeBase):
    name = "gdbm"
    version = "1.26"
    license = "GPL-3.0-or-later"

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
        self.settings.rm_safe("compiler.libcxx")
        self.settings.rm_safe("compiler.cppstd")
        if not self.options.with_nls:
            self.options.rm_safe("with_libiconv")

    def validate(self):
        from thirdparty.errors import RecipeInvalidConfiguration
        if self.settings.os == "Windows":
            raise RecipeInvalidConfiguration(f"{self.name} is not supported on Windows")

    def requirements(self):
        if self.options.get_safe("with_libiconv"):
            self.requires("libiconv")
        if self.options.with_readline:
            self.requires("readline")
        self.tool_requires("bison")
        self.tool_requires("flex")
        self.tool_requires("gnu-config")

    def latest_version(self):
        repo = GnuFtp(self, "gdbm")
        return Version(repo.latest_release)

    def source(self):
        get(
            self,
            url="https://ftp.gnu.org/gnu/gdbm/gdbm-1.26.tar.gz",
            sha256="6a24504a14de4a744103dcb936be976df6fbe88ccff26065e54c1c47946f4a5e",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        virtual_build_env = VirtualBuildEnv(self)
        virtual_build_env.generate()
        if not cross_building(self):
            virtual_run_env = VirtualRunEnv(self)
            virtual_run_env.generate(scope="build")
        tc = AutotoolsToolchain(self)
        yes_no = lambda v: "yes" if v else "no"
        enable_debug = self.settings.build_type in ["Debug", "RelWithDebInfo"]
        tc.configure_args.extend(
            [
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
            libiconv_package_folder = self.dependencies.direct_host["libiconv"].folders.package
            tc.configure_args.extend(
                [
                    f"--with-libiconv-prefix={libiconv_package_folder}"
                    "--with-libintl-prefix",
                ])
        else:
            tc.configure_args.extend(
                [
                    "--without-libiconv-prefix",
                    "--without-libintl-prefix",
                ])
        if is_apple_os(self):
            # Inject -headerpad_max_install_names, otherwise fix_apple_shared_install_name() may fail.
            # See https://github.com/recipe-io/recipe-center-index/issues/20002
            tc.extra_ldflags.append("-headerpad_max_install_names")
        tc.generate()
        deps = AutotoolsDeps(self)
        deps.generate()

    def _patch_sources(self):
        apply_patches(self)
        for gnu_config in [
            self.conf.get("user.gnu-config:config_guess", check_type=str),
            self.conf.get("user.gnu-config:config_sub", check_type=str),
        ]:
            if gnu_config:
                copy(self, os.path.basename(gnu_config), os.path.dirname(gnu_config), os.path.join(self.folders.source, "build-aux"))

    def build(self):
        self._patch_sources()
        autotools = Autotools(self)
        autotools.configure()
        autotools.make(target="maintainer-clean-generic")
        autotools.make()

    def package(self):
        copy(self, "COPYING", self.folders.source, os.path.join(self.folders.package, "licenses"))
        autotools = Autotools(self)
        autotools.install()
        rm(self, "*.la", os.path.join(self.folders.package, "lib"))
        rmdir(self, os.path.join(self.folders.package, "share"))
        fix_apple_shared_install_name(self)

    def package_info(self):
        if self.options.libgdbm_compat:
            self.cpp_info.libs.append("gdbm_compat")
        self.cpp_info.libs.append("gdbm")

        bin_path = os.path.join(self.folders.package, "bin")
        self.output.info(f"Appending PATH environment variable: {bin_path}")
