from thirdparty import RecipeBase, RecipeOptions
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import get, copy, rmdir, replace_in_file
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class _Options(RecipeOptions):
    shared: bool = False
    pic: bool = True


class Recipe(RecipeBase[_Options]):
    name = "flatbuffers"
    version = "23.5.26"
    license = "Apache-2.0"

    def latest_version(self):
        repo = GithubRepository(self, "google/flatbuffers")
        return Version(repo.latest_release.removeprefix("v"))

    def requirements(self):
        self.requires_tool("cmake")

    def source(self):
        get(
            self,
            url=f"https://github.com/google/flatbuffers/archive/v{self.version}.tar.gz",
            sha256="1cce06b17cddd896b6d73cc047e36a254fb8df4d7ea18a46acf16c4c0cd3f3f3",
            destination=self.folders.source,
            strip_root=True)
        cmakelists = self.folders.source / "CMakeLists.txt"
        # Inject the version manually in generate() instead of calling git.
        replace_in_file(self, cmakelists, "include(CMake/Version.cmake)", "")
        # No warnings as errors.
        replace_in_file(self, cmakelists, "/WX", "")
        replace_in_file(self, cmakelists, "-Werror ", "")

    def generate(self):
        version = Version(self.version)
        tc = CMakeToolchain(self)
        tc.variables["FLATBUFFERS_BUILD_TESTS"] = False
        tc.variables["FLATBUFFERS_INSTALL"] = True
        tc.variables["FLATBUFFERS_BUILD_FLATLIB"] = not self.options.shared
        tc.variables["FLATBUFFERS_BUILD_FLATC"] = False
        tc.variables["FLATBUFFERS_STATIC_FLATC"] = False
        tc.variables["FLATBUFFERS_BUILD_FLATHASH"] = False
        tc.variables["FLATBUFFERS_BUILD_SHAREDLIB"] = self.options.shared
        tc.variables["FLATBUFFERS_LIBCXX_WITH_CLANG"] = False
        # Mimic upstream CMake/Version.cmake removed in source().
        tc.cache_variables["VERSION_MAJOR"] = str(version.major)
        tc.cache_variables["VERSION_MINOR"] = str(version.minor or "0")
        tc.cache_variables["VERSION_PATCH"] = str(version.patch or "0")
        tc.cache_variables["VERSION_COMMIT"] = "0"
        tc.variables["CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS"] = True
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE*", src=self.folders.source, dst=self.folders.package / "licenses")
        cmake = CMake(self)
        cmake.install()
        rmdir(self, self.folders.package / "lib" / "cmake")
        rmdir(self, self.folders.package / "lib" / "pkgconfig")

    def package_info(self):
        self.info.set_property("cmake_file_name", "flatbuffers")
        # onnxruntime does find_package(Flatbuffers CONFIG); register both casings so both
        # flatbuffers_DIR and Flatbuffers_DIR are emitted.
        self.info.set_property("cmake_file_name_variants", ["flatbuffers", "Flatbuffers"])
        cmake_target = "flatbuffers_shared" if self.options.shared else "flatbuffers"
        self.info.set_property("cmake_target_name", f"flatbuffers::{cmake_target}")
        self.info.set_property("pkg_config_name", "flatbuffers")
        self.info.libs = ["flatbuffers"]
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.info.system_libs = ["m"]
