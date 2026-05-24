import os

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.tools.files import copy, get
from thirdparty.tools.scm import Version
from thirdparty.tools.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "pyside6"
    version = "6.11.1"
    license = "LGPL-3.0-only"

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
        self.requires("qt")
        self.requires("cpython")
        self.requires("llvm")

    def build_requirements(self):
        self.tool_requires("cmake")
        self.tool_requires("cpython")

    def latest_version(self):
        repo = GithubRepository(self, "qt/pyside-setup")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://download.qt.io/official_releases/QtForPython/pyside6/PySide6-6.11.1-src/pyside-setup-everywhere-src-6.11.1.tar.xz",
            sha256="6ffd9835bb0dd2c56f061d62f1616bb1707cfc0202b80e3165d6be087f3965e2",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        llvm_pkg = self.dependencies["llvm"].package_folder
        cpython_pkg = self.dependencies["cpython"].package_folder

        tc = CMakeToolchain(self)
        tc.variables["BUILD_TESTS"] = False
        tc.variables["INSTALL_TESTS"] = False
        tc.variables["CMAKE_VERBOSE_MAKEFILE"] = False

        tc.variables["CLANG_INSTALL_DIR"] = llvm_pkg.replace("\\", "/")

        python_root = cpython_pkg.replace("\\", "/")
        tc.variables["Python3_ROOT_DIR"] = python_root
        tc.variables["Python3_FIND_STRATEGY"] = "LOCATION"
        if self.settings.os == "Windows":
            tc.variables["Python3_FIND_REGISTRY"] = "NEVER"

        tc.generate()

        deps = CMakeDeps(self)
        deps.set_property("qt", "cmake_find_mode", "none")
        deps.set_property("llvm", "cmake_find_mode", "none")
        deps.set_property("cpython", "cmake_find_mode", "none")
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE.FDL", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy(self, "LICENSE.GPL2", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy(self, "LICENSE.GPL3", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy(self, "LICENSE.LGPL3", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.set_property("cmake_find_mode", "none")
        self.cpp_info.builddirs = [
            os.path.join("lib", "cmake", "Shiboken6"),
            os.path.join("lib", "cmake", "PySide6"),
        ]

        # Shiboken6 runtime library
        shiboken = self.cpp_info.components["shiboken6"]
        shiboken.set_property("cmake_file_name", "Shiboken6")
        shiboken.set_property("cmake_target_name", "Shiboken6::libshiboken")
        shiboken.libs = ["shiboken6"]
        shiboken.includedirs = [os.path.join("include", "shiboken6")]
        shiboken.requires = ["cpython::cpython"]
        if self.settings.os in ("Linux", "FreeBSD"):
            shiboken.system_libs = ["pthread", "dl"]

        # Expose the shiboken6 generator location via conf
        self.conf_info.define(
            "user.pyside6:shiboken6_generator",
            os.path.join(self.package_folder, "bin", "shiboken6"),
        )
        self.conf_info.define(
            "user.pyside6:pyside6_dir",
            self.package_folder,
        )

        bin_dir = os.path.join(self.package_folder, "bin")
        self.buildenv_info.prepend_path("PATH", bin_dir)
