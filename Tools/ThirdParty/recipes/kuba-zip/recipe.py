from thirdparty import RecipeBase, RecipeOptions
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import copy, get, replace_in_file, rmdir
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class _Options(RecipeOptions):
    shared: bool = False
    fPIC: bool = True


class Recipe(RecipeBase[_Options]):
    name = "kuba-zip"
    version = "0.3.8"
    license = "Unlicense"

    def latest_version(self):
        repo = GithubRepository(self, "kuba--/zip")
        return Version(repo.latest_release.removeprefix("v"))

    def configure(self):
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def source(self):
        get(
            self,
            url="https://github.com/kuba--/zip/archive/v0.3.8.tar.gz",
            sha256="944656c33aa776dc2c882991d1a6a86c8408fec8b8a19bc5305bf7eabdd4d908",
            destination=self.folders.source,
            strip_root=True)
        replace_in_file(self, self.folders.source / "CMakeLists.txt", "-Werror", "")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["CMAKE_DISABLE_TESTING"] = True
        tc.variables["ZIP_STATIC_PIC"] = self.options.fPIC
        tc.variables["ZIP_BUILD_DOCS"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "UNLICENSE", src=self.folders.source, dst=self.folders.package / "licenses")
        cmake = CMake(self)
        cmake.install()
        rmdir(self, self.folders.package / "lib" / "cmake")

    def package_info(self):
        self.info.set_property("cmake_file_name", "zip")
        self.info.set_property("cmake_target_name", "zip::zip")

        self.info.libs = ["zip"]
        if self.options.shared:
            self.info.defines.append("ZIP_SHARED")
