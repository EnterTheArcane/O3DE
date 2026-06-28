from thirdparty import RecipeBase, RecipeOptions
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import copy, get
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class _Options(RecipeOptions):
    shared: bool = False
    fPIC: bool = True
    with_sse: bool = True


class Recipe(RecipeBase[_Options]):
    name = "basis-universal"
    version = "2.1.0"
    license = "Apache-2.0"

    def latest_version(self):
        repo = GithubRepository(self, "BinomialLLC/basis_universal")
        return Version(repo.latest_release.lstrip("v").replace("_", "."))

    @property
    def _has_sse(self):
        return self.settings.arch in ["X64"]

    def configure(self):
        if not self._has_sse:
            del self.options.with_sse

    def source(self):
        get(
            self,
            url="https://github.com/BinomialLLC/basis_universal/archive/refs/tags/v2_1_0.tar.gz",
            sha256="ee1dbeb4c16699b577a0c78dce337bbede268e04bd2d463946971f8cb1e9c8df",
            destination=self.folders.source,
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
        copy(self, "LICENSE", src=self.folders.source, dst=self.folders.package / "licenses")
        copy(
            self, "*.h", src=self.folders.source / "transcoder",
            dst=self.folders.package / "include")
        copy(
            self, "*.h", src=self.folders.source / "encoder",
            dst=self.folders.package / "include")
        copy(self, "*.a", src=self.folders.build, dst=self.folders.package / "lib", keep_path=False)
        copy(self, "*.lib", src=self.folders.build, dst=self.folders.package / "lib", keep_path=False)

    def package_info(self):
        self.info.libs = ["basisu_encoder"]
        if self.settings.os == "Windows":
            self.info.defines = ["BASISU_NO_ITERATOR_DEBUG_LEVEL"]
        elif self.settings.os in ["Linux", "FreeBSD"]:
            self.info.system_libs = ["m", "pthread"]
