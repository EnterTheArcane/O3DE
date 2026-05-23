import os

from thirdparty import RecipeBase
from thirdparty.tools.build import can_run
from thirdparty.tools.env import VirtualBuildEnv, VirtualRunEnv
from thirdparty.tools.files import apply_patches, copy, get, replace_in_file, rmdir
from thirdparty.tools.scm.gitlab import GitlabRepository
from thirdparty.tools.gnu import PkgConfigDeps
from thirdparty.tools.meson import Meson, MesonToolchain
from thirdparty.tools.scm import Version


class Recipe(RecipeBase):
    name = "wayland"
    version = "1.24.0"
    license = "MIT"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "enable_libraries": [True, False],
        "enable_dtd_validation": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        # "enable_libraries": True, # See `config_options()`
        "enable_dtd_validation": True,
    }

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def validate(self):
        from thirdparty._conan.errors import ConanInvalidConfiguration
        if self.settings.os == "Windows":
            raise ConanInvalidConfiguration(f"{self.name} is not supported on Windows")

    def config_options(self):
        # enable libraries by defualt only on Linux, Android
        self.options.enable_libraries = self.settings.os in ("Linux", "Android")

    def requirements(self):
        if self.options.enable_libraries:
            self.requires("libffi")
        if self.options.enable_dtd_validation:
            self.requires("libxml2")
        self.requires("expat")

    def build_requirements(self):
        self.tool_requires("meson")
        if not self.conf.get("tools.gnu:pkg_config", default=False, check_type=str):
            self.tool_requires("pkgconf")
        if not can_run(self):
            self.tool_requires(str(self.ref))

    def latest_version(self):
        repo = GitlabRepository(self, "wayland/wayland", host="gitlab.freedesktop.org")
        return Version(repo.latest_release)

    def source(self):
        get(
            self,
            url="https://gitlab.freedesktop.org/wayland/wayland/-/releases/1.24.0/downloads/wayland-1.24.0.tar.xz",
            sha256="82892487a01ad67b334eca83b54317a7c86a03a89cfadacfef5211f11a5d0536",
            destination=self.source_folder,
            strip_root=True)
        apply_patches(self)

    def generate(self):
        env = VirtualBuildEnv(self)
        env.generate()
        if can_run(self):
            env = VirtualRunEnv(self)
            env.generate(scope="build")

        pkg_config_deps = PkgConfigDeps(self)
        if not can_run(self):
            pkg_config_deps.build_context_activated = ["wayland"]
        elif self.dependencies["expat"].is_build_context:  # wayland is being built as build_require
            # If wayland is the build_require, all its dependencies are treated as build_requires
            pkg_config_deps.build_context_activated = [dep.ref.name for _, dep in self.dependencies.host.items()]
        pkg_config_deps.generate()
        tc = MesonToolchain(self)
        tc.project_options["libdir"] = "lib"
        tc.project_options["datadir"] = "res"
        tc.project_options["libraries"] = self.options.enable_libraries
        tc.project_options["dtd_validation"] = self.options.enable_dtd_validation
        tc.project_options["documentation"] = False
        if not can_run(self):
            tc.project_options["build.pkg_config_path"] = self.generators_folder
        tc.project_options["scanner"] = True
        tc.generate()

    def _patch_sources(self):
        replace_in_file(self, os.path.join(self.source_folder, "meson.build"),
                        "subdir('tests')", "#subdir('tests')")

    def build(self):
        self._patch_sources()
        meson = Meson(self)
        meson.configure()
        meson.build()

    def package(self):
        copy(self, "COPYING", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        meson = Meson(self)
        meson.install()
        pkg_config_dir = os.path.join(self.package_folder, "lib", "pkgconfig")
        rmdir(self, pkg_config_dir)

    def package_info(self):
        self.cpp_info.components["wayland-scanner"].set_property("pkg_config_name", "wayland-scanner")
        self.cpp_info.components["wayland-scanner"].resdirs = ["res"]
        self.cpp_info.components["wayland-scanner"].includedirs = []
        self.cpp_info.components["wayland-scanner"].libdirs = []
        self.cpp_info.components["wayland-scanner"].set_property("component_version", self.version)
        self.cpp_info.components["wayland-scanner"].requires = ["expat::expat"]
        if self.options.enable_dtd_validation:
            self.cpp_info.components["wayland-scanner"].requires.append("libxml2::libxml2")
        pkgconfig_variables = {
            'datarootdir': '${prefix}/res',
            'pkgdatadir': '${datarootdir}/wayland',
            'bindir': '${prefix}/bin',
            'wayland_scanner': '${bindir}/wayland-scanner',
        }
        self.cpp_info.components["wayland-scanner"].set_property(
            "pkg_config_custom_content",
            "\n".join(f"{key}={value}" for key,value in pkgconfig_variables.items()))

        if self.options.enable_libraries:
            self.cpp_info.components["wayland-server"].libs = ["wayland-server"]
            self.cpp_info.components["wayland-server"].set_property("pkg_config_name", "wayland-server")
            self.cpp_info.components["wayland-server"].requires = ["libffi::libffi"]
            if self.settings.os in ["Linux", "FreeBSD"]:
                self.cpp_info.components["wayland-server"].system_libs = ["pthread", "m"]

            self.cpp_info.components["wayland-server"].resdirs = ["res"]
            if self.settings.os == "Linux":
                self.cpp_info.components["wayland-server"].system_libs += ["rt"]
            self.cpp_info.components["wayland-server"].set_property("component_version", self.version)
            pkgconfig_variables = {
                'datarootdir': '${prefix}/res',
                'pkgdatadir': '${datarootdir}/wayland',
            }
            self.cpp_info.components["wayland-server"].set_property(
                "pkg_config_custom_content",
                "\n".join(f"{key}={value}" for key, value in pkgconfig_variables.items()))

            self.cpp_info.components["wayland-client"].libs = ["wayland-client"]
            self.cpp_info.components["wayland-client"].set_property("pkg_config_name", "wayland-client")
            self.cpp_info.components["wayland-client"].requires = ["libffi::libffi"]
            if self.settings.os in ["Linux", "FreeBSD"]:
                self.cpp_info.components["wayland-client"].system_libs = ["pthread", "m"]
            self.cpp_info.components["wayland-client"].resdirs = ["res"]
            if self.settings.os == "Linux":
                self.cpp_info.components["wayland-client"].system_libs += ["rt"]
            self.cpp_info.components["wayland-client"].set_property("component_version", self.version)
            pkgconfig_variables = {
                'datarootdir': '${prefix}/res',
                'pkgdatadir': '${datarootdir}/wayland',
            }
            self.cpp_info.components["wayland-client"].set_property(
                "pkg_config_custom_content",
                "\n".join(f"{key}={value}" for key, value in pkgconfig_variables.items()))

            self.cpp_info.components["wayland-cursor"].libs = ["wayland-cursor"]
            self.cpp_info.components["wayland-cursor"].set_property("pkg_config_name", "wayland-cursor")
            self.cpp_info.components["wayland-cursor"].requires = ["wayland-client"]
            self.cpp_info.components["wayland-cursor"].set_property("component_version", self.version)

            self.cpp_info.components["wayland-egl"].libs = ["wayland-egl"]
            self.cpp_info.components["wayland-egl"].set_property("pkg_config_name", "wayland-egl")
            self.cpp_info.components["wayland-egl"].requires = ["wayland-client"]
            self.cpp_info.components["wayland-egl"].set_property("component_version", "18.1.0")

            self.cpp_info.components["wayland-egl-backend"].set_property("pkg_config_name", "wayland-egl-backend")
            self.cpp_info.components["wayland-egl-backend"].set_property("component_version", "3")
