from thirdparty import RecipeBase
from thirdparty.tools.build import check_min_cppstd
from thirdparty.tools.files import copy, get, rmdir
from thirdparty.tools.meson import Meson, MesonToolchain
from thirdparty.tools.scm import Version
import os

class Recipe(RecipeBase):
    name = "directx-headers"
    version = "1.618.2"
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
        self.tool_requires("meson/1.2.2")

    def source(self):
        get(self, url="https://github.com/microsoft/DirectX-Headers/archive/refs/tags/v1.618.2.tar.gz", sha256="62004f45e2ab00cbb5c7f03c47262632c22fbce0a237383fc458d9324c44cf36", destination=self.source_folder, strip_root=True)

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
