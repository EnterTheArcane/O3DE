from thirdparty import RecipeBase
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import copy, get
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "gz-cmake"
    version = "5.1.1"
    license = "Apache-2.0"

    def latest_version(self):
        repo = GithubRepository(self, "gazebosim/gz-cmake")
        tag = repo.latest_release
        return Version(tag.split("_", 1)[-1])

    def requirements(self):
        self.requires_tool("cmake")

    def source(self):
        get(
            self,
            url="https://github.com/gazebosim/gz-cmake/archive/refs/tags/gz-cmake5_5.1.1.tar.gz",
            sha256="5424e481b765e7e88347c167e87b1c89f152ded2f8bbc7f24c7559ea3694f83f",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_TESTING"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.folders.source, dst=self.folders.package / "licenses")
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        # gz-cmake installs only cmake modules — no compiled libraries
        self.info.set_property("cmake_find_mode", "none")
        self.info.libdirs = []
        self.info.bindirs = []
        # Point directly to the directory containing the real gz-cmake-config.cmake.
        # Using "" (package root) would cause CMake to find the auto-generated stub
        # at <root>/gz-cmake-config.cmake instead of the real one installed by gz-cmake
        # at share/cmake/gz-cmake/gz-cmake-config.cmake, which actually loads the cmake
        # modules (gz_configure_project, etc.) that downstream packages need.
        self.info.builddirs = ["share/cmake/gz-cmake"]
