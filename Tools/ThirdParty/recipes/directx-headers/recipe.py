import os

from thirdparty import RecipeBase
from thirdparty.env import VirtualBuildEnv
from thirdparty.files import copy, get, rmdir
from thirdparty.meson import Meson, MesonToolchain
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "directx-headers"
    version = "1.619.1"
    license = "MIT"

    def latest_version(self):
        repo = GithubRepository(self, "microsoft/DirectX-Headers")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/microsoft/DirectX-Headers/archive/refs/tags/v1.619.1.tar.gz",
            sha256="6193774904c940eebb9b0c51b816b93dd776cfeb25a951f0f4a58f22387e5008",
            destination=self.folders.source,
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
        copy(self, "LICENSE", self.folders.source, self.folders.package / "licenses")
        meson = Meson(self)
        meson.install()
        rmdir(self, self.folders.package / "lib" / "pkgconfig")

    def package_info(self):
        if self.settings.os == "Linux" or self.settings.get_safe("os.subsystem") == "wsl":
            self.info.includedirs.append(os.path.join("include", "wsl", "stubs"))
        self.info.libs = ["d3dx12-format-properties", "DirectX-Guids"]
        self.info.set_property("cmake_file_name", "DirectX-Headers")
        self.info.set_property("cmake_target_name", "Microsoft::DirectX-Headers")
        self.info.set_property("pkg_config_name", "DirectX-Headers")
        if self.settings.os == "Windows":
            self.info.system_libs.append("d3d12")
        if self.settings.compiler == "msvc":
            self.info.system_libs.append("dxcore")
