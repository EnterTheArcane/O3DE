from thirdparty import RecipeBase
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import copy, get, rmdir
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "spirv-headers"
    version = "1.4.352.0"
    license = "MIT-KhronosGroup"

    def latest_version(self):
        repo = GithubRepository(self, "KhronosGroup/SPIRV-Headers")
        return Version(repo.latest_release)

    def source(self):
        get(
            self,
            url="https://github.com/KhronosGroup/SPIRV-Headers/archive/fe44b2002bf7871e2e92fc001bc9f6e09f92194f.tar.gz",
            sha256="78a19a22810130602110761b7eaa47e49b778ea61bc7b05377c9794f54d2a426",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["SPIRV_HEADERS_SKIP_EXAMPLES"] = True
        tc.variables["SPIRV_HEADERS_ENABLE_TESTS"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE*", src=self.folders.source, dst=self.folders.package / "licenses")
        cmake = CMake(self)
        cmake.install()
        rmdir(self, self.folders.package / "lib")
        rmdir(self, self.folders.package / "share")

    def package_info(self):
        self.info.set_property("cmake_file_name", "SPIRV-Headers")
        self.info.set_property("cmake_target_name", "SPIRV-Headers::SPIRV-Headers")
        self.info.set_property("pkg_config_name", "SPIRV-Headers")
        self.info.bindirs = []
        self.info.libdirs = []
