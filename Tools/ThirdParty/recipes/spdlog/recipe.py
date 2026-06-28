from thirdparty import RecipeBase, RecipeOptions
from thirdparty.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.files import copy, get, replace_in_file, rmdir
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class _Options(RecipeOptions):
    shared: bool = False
    fPIC: bool = True
    wchar_support: bool = False
    wchar_filenames: bool = False
    wchar_console: bool = False


class Recipe(RecipeBase[_Options]):
    name = "spdlog"
    version = "1.17.0"
    license = "MIT"

    def latest_version(self):
        repo = GithubRepository(self, "gabime/spdlog")
        return Version(repo.latest_release.removeprefix("v"))

    def configure(self):
        if self.settings.os != "Windows":
            del self.options.wchar_support
            del self.options.wchar_filenames
            del self.options.wchar_console

    def requirements(self):
        self.requires("fmt")

    def source(self):
        get(
            self,
            url="https://github.com/gabime/spdlog/archive/v1.17.0.tar.gz",
            sha256="d8862955c6d74e5846b3f580b1605d2428b11d97a410d86e2fb13e857cd3a744",
            destination=self.folders.source,
            strip_root=True)
        replace_in_file(self, self.folders.source / "cmake" / "utils.cmake", "/WX", "")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["SPDLOG_BUILD_EXAMPLE"] = False
        tc.cache_variables["SPDLOG_BUILD_EXAMPLE_HO"] = False
        tc.cache_variables["SPDLOG_BUILD_TESTS"] = False
        tc.cache_variables["SPDLOG_BUILD_TESTS_HO"] = False
        tc.cache_variables["SPDLOG_BUILD_BENCH"] = False
        fmt = self.dependencies["fmt"]
        tc.cache_variables["SPDLOG_FMT_EXTERNAL"] = not fmt.options.header_only
        tc.cache_variables["SPDLOG_FMT_EXTERNAL_HO"] = fmt.options.header_only
        tc.cache_variables["SPDLOG_BUILD_SHARED"] = self.options.shared
        tc.cache_variables["SPDLOG_WCHAR_SUPPORT"] = self.options.get_safe("wchar_support", False)
        tc.cache_variables["SPDLOG_WCHAR_FILENAMES"] = self.options.get_safe("wchar_filenames", False)
        tc.cache_variables["SPDLOG_WCHAR_CONSOLE"] = self.options.get_safe("wchar_console", False)
        tc.cache_variables["SPDLOG_INSTALL"] = True
        tc.cache_variables["SPDLOG_NO_EXCEPTIONS"] = True
        tc.cache_variables["SPDLOG_USE_STD_FORMAT"] = False
        tc.cache_variables["CMAKE_POLICY_DEFAULT_CMP0091"] = "NEW"
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", dst=self.folders.package / "licenses", src=self.folders.source)
        cmake = CMake(self)
        cmake.install()
        rmdir(self, self.folders.package / "lib" / "cmake")
        rmdir(self, self.folders.package / "lib" / "pkgconfig")
        rmdir(self, self.folders.package / "lib" / "spdlog" / "cmake")

    def package_info(self):
        self.info.set_property("cmake_file_name", "spdlog")
        self.info.set_property("cmake_target_name", "spdlog::spdlog")

        self.info.components["libspdlog"].set_property("cmake_target_name", "spdlog::spdlog")
        self.info.components["libspdlog"].requires = ["fmt::fmt"]
        self.info.components["libspdlog"].defines.append("SPDLOG_FMT_EXTERNAL")

        suffix = "d" if self.settings.build_type == "Debug" else ""
        self.info.components["libspdlog"].libs = [f"spdlog{suffix}"]
        self.info.components["libspdlog"].defines.append("SPDLOG_COMPILED_LIB")

        if self.options.get_safe("wchar_support"):
            self.info.components["libspdlog"].defines.append("SPDLOG_WCHAR_TO_UTF8_SUPPORT")
        if self.options.get_safe("wchar_filenames"):
            self.info.components["libspdlog"].defines.append("SPDLOG_WCHAR_FILENAMES")
        if self.options.get_safe("wchar_console"):
            self.info.components["libspdlog"].defines.append("SPDLOG_UTF8_TO_WCHAR_CONSOLE")
        self.info.components["libspdlog"].defines.append("SPDLOG_NO_EXCEPTIONS")
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.info.components["libspdlog"].system_libs = ["pthread"]
