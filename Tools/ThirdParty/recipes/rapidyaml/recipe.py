import os

from thirdparty import RecipeBase
from thirdparty.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.files import get, copy, rm, rmdir
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "rapidyaml"
    version = "0.13.0"
    license = "MIT"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_default_callbacks": [True, False],
        "with_tab_tokens": [True, False],
        "with_default_callback_uses_exceptions": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_default_callbacks": True,
        "with_tab_tokens": False,
        "with_default_callback_uses_exceptions": False,
    }

    @property
    def _minimum_cpp_standard(self):
        return 11

    def configure(self):
        # with_default_callback_uses_exceptions should only be valid if with_default_callbacks is true
        if not self.options.with_default_callbacks:
            self.options.rm_safe("with_default_callback_uses_exceptions")

    def requirements(self):
        self.requires("c4core")

    def latest_version(self):
        repo = GithubRepository(self, "biojppm/rapidyaml")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/biojppm/rapidyaml/releases/download/v0.13.0/rapidyaml-0.13.0-src.tgz",
            sha256="b70b484b612152b0dbb2ca61178c9534d80c392fe36d4d54e75d127ec8864d52",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["RYML_DEFAULT_CALLBACKS"] = self.options.with_default_callbacks
        tc.variables["RYML_WITH_TAB_TOKENS"] = self.options.with_tab_tokens
        tc.variables["RYML_DEFAULT_CALLBACK_USES_EXCEPTIONS"] = self.options.with_default_callback_uses_exceptions
        tc.variables["RYML_USE_ASSERT"] = False
        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, pattern="LICENSE.txt", dst=os.path.join(self.folders.package, "licenses"), src=self.folders.source)
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.folders.package, "cmake"))
        rmdir(self, os.path.join(self.folders.package, "lib", "cmake"))
        rm(self, "*.natvis", os.path.join(self.folders.package, "include"))

    def package_info(self):
        self.info.set_property("cmake_file_name", "ryml")
        self.info.set_property("cmake_target_name", "ryml::ryml")
        self.info.libs = ["ryml"]
