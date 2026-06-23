import os

from thirdparty import RecipeBase
from thirdparty.apple import fix_apple_shared_install_name, is_apple_os
from thirdparty.env import VirtualBuildEnv
from thirdparty.errors import RecipeException
from thirdparty.files import apply_patches, copy, get, rm, rmdir
from thirdparty.gnu import Autotools, AutotoolsToolchain


class Recipe(RecipeBase):
    name = "flex"
    version = "2.6.4"
    license = "BSD-2-Clause"

    def configure(self):
        self.settings.rm_safe("compiler.libcxx")
        self.settings.rm_safe("compiler.cppstd")

    def validate(self):
        if self.settings.os == "Windows":
            raise RecipeException(
                "flex is not compatible with Windows; use winflexbison instead.")

    def requirements(self):
        # flex needs m4 at runtime to generate scanners
        self.requires("m4")

    def build_requirements(self):
        self.tool_requires("m4")
        self.tool_requires("gnu-config")

    def source(self):
        get(
            self,
            url="https://github.com/westes/flex/releases/download/v2.6.4/flex-2.6.4.tar.gz",
            sha256="e87aae032bf07c26f85ac0ed3250998c37621d95f8bd748b31f15b33c45ee995",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        env = VirtualBuildEnv(self)
        env.generate()

        tc = AutotoolsToolchain(self)
        tc.configure_args.extend([
            "--disable-nls",
            "--disable-bootstrap",
            "HELP2MAN=/bin/true",
            "M4=m4",
            # https://github.com/westes/flex/issues/247
            "ac_cv_func_malloc_0_nonnull=yes",
            "ac_cv_func_realloc_0_nonnull=yes",
            # https://github.com/easybuilders/easybuild-easyconfigs/pull/5792
            "ac_cv_func_reallocarray=no",
        ])
        if is_apple_os(self):
            tc.extra_ldflags.append("-headerpad_max_install_names")
        tc.generate()

    def _patch_sources(self):
        apply_patches(self)
        # Refresh config.guess/config.sub so newer hosts (e.g. Apple Silicon) are recognised.
        for gnu_config in (
            self.conf.get("user.gnu-config:config_guess", check_type=str),
            self.conf.get("user.gnu-config:config_sub", check_type=str),
        ):
            if gnu_config:
                copy(self, os.path.basename(gnu_config),
                     src=os.path.dirname(gnu_config),
                     dst=os.path.join(self.folders.source, "build-aux"))

    def build(self):
        self._patch_sources()
        autotools = Autotools(self)
        autotools.configure()
        autotools.make()

    def package(self):
        copy(self, "COPYING", src=self.folders.source,
             dst=os.path.join(self.folders.package, "licenses"))
        autotools = Autotools(self)
        autotools.install()
        rmdir(self, os.path.join(self.folders.package, "share"))
        rm(self, "*.la", os.path.join(self.folders.package, "lib"))
        fix_apple_shared_install_name(self)

    def package_info(self):
        self.cpp_info.libs = ["fl"]
        self.cpp_info.system_libs = ["m"]
        # Avoid CMakeDeps messing with Conan targets
        self.cpp_info.set_property("cmake_find_mode", "none")

        lex_path = os.path.join(self.folders.package, "bin", "flex").replace("\\", "/")
        self.buildenv_info.define("LEX", lex_path)
