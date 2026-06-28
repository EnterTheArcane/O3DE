from thirdparty import RecipeBase, RecipeOptions
from thirdparty.build import stdcpp_library
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import copy, get, rmdir
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class _Options(RecipeOptions):
    shared: bool = False
    fPIC: bool = True


class Recipe(RecipeBase[_Options]):
    name = "lerc"
    version = "4.1.0"
    license = "Apache-2.0"

    def latest_version(self):
        repo = GithubRepository(self, "Esri/lerc")
        return Version(repo.latest_release.removeprefix("v"))

    def requirements(self):
        self.requires_tool("cmake")

    def source(self):
        get(
            self,
            url="https://github.com/Esri/lerc/archive/refs/tags/v4.1.0.tar.gz",
            sha256="f05b24d2368becab9144873878655bb718910631550d4f786262378c16ab94a7",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.folders.source, dst=self.folders.package / "licenses")
        cmake = CMake(self)
        cmake.install()
        rmdir(self, self.folders.package / "lib" / "pkgconfig")

    def package_info(self):
        self.info.libs = ["Lerc"]
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.info.system_libs.append("m")
        if not self.options.shared:
            self.info.defines = ["LERC_STATIC"]
            lib = stdcpp_library(self)
            if lib:
                self.info.system_libs.append(lib)
