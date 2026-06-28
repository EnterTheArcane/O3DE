from thirdparty import RecipeBase, RecipeOptions
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import copy, get
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class _Options(RecipeOptions):
    shared: bool = False
    pic: bool = True
    with_pes_ts: bool = True
    with_new_parser_api: bool = False


class Recipe(RecipeBase[_Options]):
    name = "libwebm"
    version = "1.0.0.32"
    license = "BSD-3-Clause"

    def latest_version(self):
        repo = GithubRepository(self, "webmproject/libwebm")
        return Version(repo.latest_release.removeprefix("libwebm-"))

    def requirements(self):
        self.requires_tool("cmake")

    def source(self):
        get(
            self,
            url="https://github.com/webmproject/libwebm/archive/refs/tags/libwebm-1.0.0.32.tar.gz",
            sha256="7fd5e085bda9f8031cf2ad2a1e52d9b7b29cba9c0b96ad2ce794ce89e4249eb8",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["ENABLE_WEBMTS"] = self.options.with_pes_ts
        tc.variables["ENABLE_WEBM_PARSER"] = self.options.with_new_parser_api
        tc.variables["ENABLE_WEBMINFO"] = False
        tc.variables["ENABLE_SAMPLE_PROGRAMS"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
        copy(self, "LICENSE.TXT", src=self.folders.source, dst=self.folders.package / "licenses")

    def package_info(self):
        self.info.set_property("cmake_file_name", "webm")
        self.info.set_property("cmake_target_name", "webm::webm")
        self.info.set_property("pkg_config_name", "webm")
        self.info.libs = ["webm"]
        self.info.includedirs.append("include/webm")

        if self.settings.os in ["Linux", "FreeBSD", "Android"]:
            self.info.system_libs.append("m")
