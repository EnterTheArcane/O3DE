from thirdparty import RecipeBase, RecipeOptions
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import copy, get
from thirdparty.scm import GithubRepository, Version


class _Options(RecipeOptions):
    shared: bool = False
    pic: bool = True


class Recipe(RecipeBase[_Options]):
    name = "poly2tri"
    version = "20130502"
    license = "BSD-3-Clause"

    def latest_version(self):
        return Version(GithubRepository(self, "greenm01/poly2tri").latest_commit_date())

    def requirements(self):
        self.requires_tool("cmake")

    def source(self):
        get(
            self,
            url="https://github.com/greenm01/poly2tri/archive/88de49021b6d9bef6faa1bc94ceb3fbd85c3c204.zip",
            sha256="2bd25eb2b8f467382c5bc3384c8c62ab3e6c10de26be8019aa94d93f7b65806d",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["POLY2TRI_SRC_DIR"] = (self.folders.source / "poly2tri").as_posix()
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure(build_script_folder=self.folders.recipe)
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.folders.source, dst=self.folders.package / "licenses")
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.info.libs = ["poly2tri"]
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.info.system_libs.append("m")
