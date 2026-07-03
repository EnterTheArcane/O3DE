from thirdparty import RecipeBase, RecipeOptions
from thirdparty.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.files import copy, get, rm, rmdir
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class _Options(RecipeOptions):
    shared: bool = False
    pic: bool = True


class Recipe(RecipeBase[_Options]):
    name = "c4core"
    version = "0.4.0"
    license = "MIT",

    def latest_version(self):
        repo = GithubRepository(self, "biojppm/c4core")
        return Version(repo.latest_release.removeprefix("v"))

    def requirements(self):
        self.requires_tool("cmake")
        self.requires("fast-float")

    def source(self):
        get(
            self,
            url=f"https://github.com/biojppm/c4core/releases/download/v{self.version}/c4core-{self.version}-src.tgz",
            sha256="6703768e6ae3f623296d3fb5cff0fc74c08bfe45dc800234e0e42ba508e230a0",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["C4CORE_WITH_FASTFLOAT"] = True
        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, pattern="LICENSE*", dst=self.folders.package / "licenses", src=self.folders.source)
        cmake = CMake(self)
        cmake.install()
        rmdir(self, self.folders.package / "cmake")
        rmdir(self, self.folders.package / "lib" / "cmake")
        rm(self, "*.natvis", self.folders.package / "include", recursive=True)

    def package_info(self):
        self.info.libs = ["c4core"]
        self.info.set_property("cmake_file_name", "c4core")
        self.info.set_property("cmake_target_name", "c4core::c4core")

        if self.settings.os in ["Linux", "FreeBSD"]:
            self.info.system_libs.append("m")
