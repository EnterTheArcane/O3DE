from thirdparty import RecipeBase, RecipeOptions
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import apply_patches, get, rmdir, copy, rm
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class _Options(RecipeOptions):
    shared: bool = False
    fPIC: bool = True


class Recipe(RecipeBase[_Options]):
    name = "zlib"
    version = "1.3.2"
    license = "Zlib"

    def configure(self):
        self.settings.rm_safe("compiler.libcxx")
        self.settings.rm_safe("compiler.cppstd")

    def latest_version(self):
        repo = GithubRepository(self, "madler/zlib")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://zlib.net/fossils/zlib-1.3.2.tar.gz",
            sha256="bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16",
            destination=self.folders.source,
            strip_root=True)
        apply_patches(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["ZLIB_BUILD_TESTING"] = False
        tc.cache_variables["ZLIB_BUILD_SHARED"] = self.options.shared
        tc.cache_variables["ZLIB_BUILD_STATIC"] = not self.options.shared
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.folders.source, dst=self.folders.package / "licenses")
        cmake = CMake(self)
        cmake.install()
        rmdir(self, self.folders.package / "share")
        rmdir(self, self.folders.package / "lib" / "cmake")
        rmdir(self, self.folders.package / "lib" / "pkgconfig")
        rm(self, "*.pdb", self.folders.package / "bin")

    def package_info(self):
        self.info.set_property("cmake_file_name", "ZLIB")
        self.info.set_property("cmake_target_name", "ZLIB::ZLIB")
        self.info.set_property("pkg_config_name", "zlib")

        if self.settings.os == "Windows" and self.settings.get_safe("compiler.runtime"):
            # The recipe patches the CMakeLists.txt to generate different filenames when CMake
            # detects MINGW (clang, gcc with compiler.runtime undefined and compiler.libcxx defined)
            libname = "zdll" if self.options.shared else "zlib"
        else:
            libname = "z"
        self.info.libs = [libname]
