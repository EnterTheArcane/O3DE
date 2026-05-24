import os

from thirdparty import RecipeBase
from thirdparty.tools.env import VirtualBuildEnv
from thirdparty.tools.files import copy, get, rmdir
from thirdparty.tools.meson import Meson, MesonToolchain
from thirdparty.tools.scm import Version
from thirdparty.tools.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "directx-headers"
    version = "1.619.1"
    license = "MIT"

    @property
    def _min_cppstd(self):
        return 11

    @property
    def _compilers_minimum_version(self):
        return {
            "apple-clang": "10",
            "clang": "5",
            "gcc": "6",
            "msvc": "191",
            "Visual Studio": "15",
        }

    def build_requirements(self):
        self.tool_requires("meson")

    def latest_version(self):
        repo = GithubRepository(self, "microsoft/DirectX-Headers")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/microsoft/DirectX-Headers/archive/refs/tags/v1.619.1.tar.gz",
            sha256="6193774904c940eebb9b0c51b816b93dd776cfeb25a951f0f4a58f22387e5008",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        tc = MesonToolchain(self)
        tc.project_options["build-test"] = False
        tc.generate()
        virtual_build_env = VirtualBuildEnv(self)
        virtual_build_env.generate()

    def build(self):
        meson = Meson(self)
        meson.configure()
        meson.build()

    def package(self):
        copy(self, "LICENSE", self.source_folder, os.path.join(self.package_folder, "licenses"))
        meson = Meson(self)
        meson.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))

    def package_info(self):
        if self.settings.os == "Linux" or self.settings.get_safe("os.subsystem") == "wsl":
            self.cpp_info.includedirs.append(os.path.join("include", "wsl", "stubs"))
        self.cpp_info.libs = ["d3dx12-format-properties", "DirectX-Guids"]
        self.cpp_info.set_property("cmake_file_name", "DirectX-Headers")
        self.cpp_info.set_property("cmake_target_name", "Microsoft::DirectX-Headers")
        self.cpp_info.set_property("pkg_config_name", "DirectX-Headers")
        if self.settings.os == "Windows":
            self.cpp_info.system_libs.append("d3d12")
        if self.settings.compiler == "msvc":
            self.cpp_info.system_libs.append("dxcore")
