import os

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.tools.files import copy, get
from thirdparty.tools.scm import Version
from thirdparty.tools.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "gz-utils"
    version = "4.0.0"
    license = "Apache-2.0"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": True,
        "fPIC": True,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def requirements(self):
        self.requires("gz-cmake")
        self.requires("spdlog")

    def build_requirements(self):
        self.tool_requires("cmake")
        self.tool_requires("gz-cmake")

    def latest_version(self):
        repo = GithubRepository(self, "gazebosim/gz-utils")
        tag = repo.latest_release
        return Version(tag.split("_", 1)[-1])

    def source(self):
        get(
            self,
            url="https://github.com/gazebosim/gz-utils/archive/refs/tags/gz-utils4_4.0.0.tar.gz",
            sha256="b06a179ea4297be8b8d09ea7a5d3d45059a3e4030c1bd256afc62f997cc992ed",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_TESTING"] = False
        tc.variables["SKIP_PYBIND11"] = True
        tc.variables["SKIP_SWIG"] = True
        # Use bundled CLI11 to avoid external dependency
        tc.variables["GZ_UTILS_VENDOR_CLI11"] = True
        tc.generate()

        deps = CMakeDeps(self)
        deps.set_property("gz-cmake", "cmake_find_mode", "none")
        deps.set_property("spdlog", "cmake_file_name", "spdlog")
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        # Keep gz-utils cmake config files; consumers use find_package(gz-utils) via CMAKE_PREFIX_PATH
        self.cpp_info.set_property("cmake_find_mode", "none")
        self.cpp_info.builddirs = [""]

        # gz-utils major version suffix in library name: gz-utils4
        version_major = self.version.split(".")[0]
        lib_suffix = version_major

        self.cpp_info.components["core"].libs = [f"gz-utils{lib_suffix}"]
        self.cpp_info.components["core"].set_property("cmake_target_name", "gz-utils::gz-utils")
        self.cpp_info.components["core"].requires = ["spdlog::spdlog"]

        self.cpp_info.components["cli"].libs = [f"gz-utils{lib_suffix}-cli"]
        self.cpp_info.components["cli"].set_property("cmake_target_name", "gz-utils::gz-utils-cli")
        self.cpp_info.components["cli"].requires = ["core"]

        self.cpp_info.components["log"].libs = [f"gz-utils{lib_suffix}-log"]
        self.cpp_info.components["log"].set_property("cmake_target_name", "gz-utils::gz-utils-log")
        self.cpp_info.components["log"].requires = ["core", "spdlog::spdlog"]
