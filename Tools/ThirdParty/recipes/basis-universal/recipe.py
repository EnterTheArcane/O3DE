import os

from thirdparty import RecipeBase
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import copy, get
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "basis-universal"
    version = "2.1.0"
    license = "Apache-2.0"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_sse": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_sse": True,
    }

    @property
    def _has_sse(self):
        return self.settings.arch in ["X64"]

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC
        if not self._has_sse:
            del self.options.with_sse

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def latest_version(self):
        repo = GithubRepository(self, "BinomialLLC/basis_universal")
        return Version(repo.latest_release.lstrip("v").replace("_", "."))

    def source(self):
        get(
            self,
            url="https://github.com/BinomialLLC/basis_universal/archive/refs/tags/v2_1_0.tar.gz",
            sha256="ee1dbeb4c16699b577a0c78dce337bbede268e04bd2d463946971f8cb1e9c8df",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BASISU_SSE"] = self.options.get_safe("with_sse", False)
        tc.variables["BASISU_ZSTD"] = True
        tc.variables["BASISU_EXAMPLES"] = False
        tc.variables["BASISU_OPENCL"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy(
            self, "*.h", src=os.path.join(self.source_folder, "transcoder"),
            dst=os.path.join(self.package_folder, "include"))
        copy(
            self, "*.h", src=os.path.join(self.source_folder, "encoder"),
            dst=os.path.join(self.package_folder, "include"))
        copy(self, "*.a", src=self.build_folder, dst=os.path.join(self.package_folder, "lib"), keep_path=False)
        copy(self, "*.lib", src=self.build_folder, dst=os.path.join(self.package_folder, "lib"), keep_path=False)

    def package_info(self):
        self.cpp_info.libs = ["basisu_encoder"]
        if self.settings.os == "Windows":
            self.cpp_info.defines = ["BASISU_NO_ITERATOR_DEBUG_LEVEL"]
        elif self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs = ["m", "pthread"]
