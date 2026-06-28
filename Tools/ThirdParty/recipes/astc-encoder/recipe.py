from typing import Literal

from thirdparty import RecipeBase, RecipeOptions
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import copy, get, rmdir
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class _Options(RecipeOptions):
    shared: bool = False
    fPIC: bool = True
    isa: Literal["avx2", "sse4.1", "sse2", "neon", "none", "native"] = "native"


class Recipe(RecipeBase[_Options]):
    name = "astc-encoder"
    version = "5.5.0"
    license = "Apache-2.0"

    def latest_version(self):
        repo = GithubRepository(self, "ARM-software/astc-encoder")
        return Version(repo.latest_release)

    def configure(self):
        if self.settings.arch in ["ARM"]:
            self.options.isa = "neon"

    def requirements(self):
        self.requires_tool("cmake")

    def source(self):
        get(
            self,
            url="https://github.com/ARM-software/astc-encoder/archive/refs/tags/5.5.0.tar.gz",
            sha256="9a3b884b040dcd5e70bc48d685c888b4aa166b559012eee059289690ac9d9958",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["ASTCENC_CLI"] = False
        tc.variables["ASTCENC_WERROR"] = False
        isa = str(self.options.isa)
        tc.variables["ASTCENC_ISA_AVX2"] = isa == "avx2"
        tc.variables["ASTCENC_ISA_SSE41"] = isa == "sse4.1"
        tc.variables["ASTCENC_ISA_SSE2"] = isa == "sse2"
        tc.variables["ASTCENC_ISA_NEON"] = isa == "neon"
        tc.variables["ASTCENC_ISA_NONE"] = isa == "none"
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE.txt", src=self.folders.source, dst=self.folders.package / "licenses")
        cmake = CMake(self)
        cmake.install()
        rmdir(self, self.folders.package / "lib" / "cmake")

    def package_info(self):
        isa = str(self.options.isa)
        suffix = "-static" if not self.options.shared else ""
        self.info.libs = [f"astcenc-{isa}{suffix}"]
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.info.system_libs.extend(["m", "pthread"])
