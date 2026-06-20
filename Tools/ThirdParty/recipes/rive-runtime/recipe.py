import os

from thirdparty import RecipeBase
from thirdparty.env import Environment, VirtualBuildEnv
from thirdparty.files import copy, get
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository
from thirdparty.premake import Premake, PremakeDeps, PremakeToolchain


class Recipe(RecipeBase):
    name = "rive-runtime"
    version = "0.1.99"
    license = "MIT"

    def build_requirements(self):
        self.tool_requires("premake5")

    def latest_version(self):
        repo = GithubRepository(self, "rive-app/rive-runtime")
        return Version(repo.latest_tag("runtime-v").removeprefix("runtime-v"))

    def source(self):
        get(
            self,
            url=f"https://github.com/rive-app/rive-runtime/archive/refs/tags/runtime-v{self.version}.tar.gz",
            sha256="869dadb8157fd062a8a745aa5e1db4d33ada6d8591e6b9a3d8eb54544fac3f15",
            destination=self.source_folder,
            strip_root=True,
        )

        deps_dir = os.path.join(self.source_folder, "dependencies")

        # rive-app fork of harfbuzz (branch rive_13.1.1, pinned to commit)
        get(
            self,
            url="https://github.com/rive-app/harfbuzz/archive/08d34675f64b1ac4880f4f10c9fd474a56dd4399.tar.gz",
            sha256="0c36546f9cadbed2a5c3d7b29cb94024bdbf553fb7cde7278a24a3938c195bc0",
            destination=os.path.join(deps_dir, "rive-app_harfbuzz_rive_13.1.1"),
            strip_root=True,
        )

        # SheenBidi (tag)
        get(
            self,
            url="https://github.com/Tehreer/SheenBidi/archive/refs/tags/v2.6.tar.gz",
            sha256="f538f51a7861dd95fb9e3f4ad885f39204b5c670867019b5adb7c4b410c8e0d9",
            destination=os.path.join(deps_dir, "Tehreer_SheenBidi_v2.6"),
            strip_root=True,
        )

        # rive-app fork of yoga (branch rive_changes_v2_0_1_2, pinned to commit)
        get(
            self,
            url="https://github.com/rive-app/yoga/archive/b827168e5e66c56b3117660a12607fe4f54ea33c.tar.gz",
            sha256="952d0a3900d04ae6a10ba5aa18a0c619c43271a5ace7240e510bfe6fb91ebd4c",
            destination=os.path.join(deps_dir, "rive-app_yoga_rive_changes_v2_0_1_2"),
            strip_root=True,
        )

        # rive-app fork of miniaudio (branch rive_changes_5, pinned to commit)
        get(
            self,
            url="https://github.com/rive-app/miniaudio/archive/3a8b070f80e203a35ec763c5118da20805a90d5a.tar.gz",
            sha256="a464609bba1294675e65e559277fe7ae0b1b7732011f537493133c486c609c76",
            destination=os.path.join(deps_dir, "rive-app_miniaudio_rive_changes_5"),
            strip_root=True,
        )

    def generate(self):
        deps = PremakeDeps(self)
        deps.generate()
        tc = PremakeToolchain(self)
        tc.generate()
        VirtualBuildEnv(self).generate()

    def build(self):
        config = "release" if str(self.settings.build_type) == "Release" else "debug"

        _arch_map = {"x86_64": "x64", "x86": "x86", "armv8": "arm64", "armv7": "arm"}

        premake = Premake(self)
        premake.luafile = os.path.join(self.source_folder, "premake5_v2.lua").replace("\\", "/")
        if premake.action == "vs2026":
            premake.action = "vs2022"
        premake.arguments["config"] = config
        # Put generated build files directly in build_folder so premake.build() can find them
        premake.arguments["out"] = "."
        premake.arguments["arch"] = _arch_map.get(str(self.settings.arch), "x64")
        premake.arguments["toolset"] = "msc" if self.settings.os == "Windows" else "clang"
        premake.arguments["with_rive_text"] = ""
        premake.arguments["with_rive_layout"] = ""

        # rive_build_config.lua is found via PREMAKE_PATH; dependency source via DEPENDENCIES
        env = Environment()
        env.define("PREMAKE_PATH", os.path.join(self.source_folder, "build").replace("\\", "/"))
        env.define("DEPENDENCIES", os.path.join(self.source_folder, "dependencies").replace("\\", "/"))

        # configure() injects --arch from CONAN_TO_PREMAKE_ARCH when the conan toolchain file exists,
        # but rive uses its own --arch option with different values (x64 not x86_64).
        # Hide the toolchain path so configure() takes the legacy code path and uses our arch arg.
        _real_tc = premake._premake_conan_toolchain
        premake._premake_conan_toolchain = _real_tc.parent / "_disabled_conantoolchain.premake5.lua"
        try:
            with env.vars(self).apply():
                premake.configure()
        finally:
            premake._premake_conan_toolchain = _real_tc

        premake.build(workspace="rive", targets=["rive"], configuration="default")

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy(self, "*.h", src=os.path.join(self.source_folder, "include"),
             dst=os.path.join(self.package_folder, "include"), keep_path=True)
        copy(self, "*.a", src=self.build_folder, dst=os.path.join(self.package_folder, "lib"), keep_path=False)
        copy(self, "*.lib", src=self.build_folder, dst=os.path.join(self.package_folder, "lib"), keep_path=False)

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "rive")
        self.cpp_info.set_property("cmake_target_name", "rive::rive")
        self.cpp_info.libs = ["rive"]
        if self.settings.os == "Windows":
            self.cpp_info.defines = ["_USE_MATH_DEFINES", "NOMINMAX"]
        elif self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs = ["m", "pthread"]
        elif self.settings.os == "Macos":
            self.cpp_info.frameworks = ["CoreText", "CoreGraphics", "CoreFoundation"]
