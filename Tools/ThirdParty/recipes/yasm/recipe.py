from thirdparty import RecipeBase
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import apply_patches, copy, get, rmdir
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "yasm"
    version = "1.3.0"
    license = "BSD-2-Clause"

    def latest_version(self):
        repo = GithubRepository(self, "yasm/yasm")
        return Version(repo.latest_release.removeprefix("v"))

    def requirements(self):
        self.requires_tool("cmake")

    def source(self):
        get(
            self,
            url="https://github.com/yasm/yasm/archive/refs/tags/v1.3.0.tar.gz",
            sha256="f708be0b7b8c59bc1dbe7134153cd2f31faeebaa8eec48676c10f972a1f13df3",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["YASM_BUILD_TESTS"] = False
        tc.cache_variables["BUILD_SHARED_LIBS"] = False
        tc.cache_variables["ENABLE_NLS"] = False
        tc.generate()

    def build(self):
        apply_patches(self)
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "BSD.txt", src=self.folders.source, dst=self.folders.package / "licenses")
        copy(self, "COPYING", src=self.folders.source, dst=self.folders.package / "licenses")
        cmake = CMake(self)
        cmake.install()
        rmdir(self, self.folders.package / "include")
        rmdir(self, self.folders.package / "lib")

    def package_info(self):
        self.info.includedirs = []
        self.info.libdirs = []
