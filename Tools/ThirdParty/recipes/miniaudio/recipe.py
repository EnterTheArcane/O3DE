import os

from thirdparty import RecipeBase
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import copy, get
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "miniaudio"
    version = "0.11.25"
    license = "Unlicense"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "header_only": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "header_only": True,
    }

    def configure(self):
        if self.options.header_only or self.options.shared:
            self.options.rm_safe("fPIC")
        if self.options.header_only:
            del self.options.shared
        self.settings.rm_safe("compiler.libcxx")
        self.settings.rm_safe("compiler.cppstd")

    def requirements(self):
        if not self.options.header_only:
            self.requires_tool("cmake")

    def latest_version(self):
        repo = GithubRepository(self, "mackron/miniaudio")
        return Version(repo.latest_release)

    def source(self):
        get(
            self,
            url="https://github.com/mackron/miniaudio/archive/0.11.25.tar.gz",
            sha256="b900edcffe979816e2560a0580b9b1216d674b4f17fbadeca8f777a7f8ab0274",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        if self.options.header_only:
            return
        tc = CMakeToolchain(self)
        tc.variables["MINIAUDIO_SRC_DIR"] = self.folders.source.as_posix()
        tc.variables["MINIAUDIO_VERSION_STRING"] = self.version
        tc.generate()

    def build(self):
        if self.options.header_only:
            return
        cmake = CMake(self)
        cmake.configure(build_script_folder=os.path.join(self.folders.source, os.pardir))
        cmake.build()

    def package(self):
        copy(self, "LICENSE", dst=os.path.join(self.folders.package, "licenses"), src=self.folders.source)
        copy(
            self,
            pattern="**",
            dst=os.path.join(self.folders.package, "include", "extras"),
            src=os.path.join(self.folders.source, "extras"),
        )
        if self.options.header_only:
            copy(self, "miniaudio.h", dst=os.path.join(self.folders.package, "include"), src=self.folders.source)
            copy(
                self,
                pattern="miniaudio.*",
                dst=os.path.join(self.folders.package, "include", "extras", "miniaudio_split"),
                src=os.path.join(self.folders.source, "extras", "miniaudio_split"),
            )
        else:
            cmake = CMake(self)
            cmake.install()

    def package_info(self):
        if self.options.get_safe("header_only"):
            self.info.bindirs = []
            self.info.libdirs = []
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.info.system_libs.extend(["m", "pthread"])
        if self.settings.os == "Linux":
            self.info.system_libs.append("dl")
        if self.settings.os == "Mac":
            self.info.frameworks.extend(["CoreFoundation", "CoreAudio", "AudioUnit"])
            self.info.defines.append("MA_NO_RUNTIME_LINKING=1")
