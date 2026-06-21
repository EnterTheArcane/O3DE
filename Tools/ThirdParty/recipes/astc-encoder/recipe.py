import os

from thirdparty import RecipeBase
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import copy, get, rmdir
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "astc-encoder"
    version = "5.4.0"
    license = "Apache-2.0"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "isa": ["avx2", "sse4.1", "sse2", "neon", "none", "native"],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "isa": "native",
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC
        if self.settings.arch in ["armv8", "armv8.3"]:
            self.options.isa = "neon"

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def latest_version(self):
        repo = GithubRepository(self, "ARM-software/astc-encoder")
        return Version(repo.latest_release)

    def source(self):
        get(
            self,
            url="https://github.com/ARM-software/astc-encoder/archive/refs/tags/5.4.0.tar.gz",
            sha256="3fbbb0b285367aaefe2ef33601d087a4ec2218b11a9876dc4dceac76f9f53e1e",
            destination=self.source_folder,
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
        copy(self, "LICENSE.txt", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))

    def package_info(self):
        isa = str(self.options.isa)
        suffix = "-static" if not self.options.shared else ""
        self.cpp_info.libs = [f"astcenc-{isa}{suffix}"]
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs.extend(["m", "pthread"])
