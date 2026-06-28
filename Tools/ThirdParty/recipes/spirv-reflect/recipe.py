from thirdparty import RecipeBase, RecipeOptions
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import copy, get
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class _Options(RecipeOptions):
    fPIC: bool = True


class Recipe(RecipeBase[_Options]):
    name = "spirv-reflect"
    version = "1.4.350.0"
    license = "Apache-2.0"

    def latest_version(self):
        repo = GithubRepository(self, "KhronosGroup/SPIRV-Reflect")
        return Version(repo.latest_release.removeprefix("vulkan-sdk-"))

    def requirements(self):
        self.requires_tool("cmake")
        self.requires(f"spirv-headers")

    def source(self):
        get(
            self,
            url="https://github.com/KhronosGroup/SPIRV-Reflect/archive/refs/tags/vulkan-sdk-1.4.350.0.tar.gz",
            sha256="c81ea49449d77189574ce0ff1374350533c283f41a9dd1b59f351c26961302b9",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["SPIRV_REFLECT_STATIC_LIB"] = True
        tc.variables["SPIRV_REFLECT_EXAMPLES"] = False
        tc.variables["SPIRV_REFLECT_BUILD_TESTS"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE*", src=self.folders.source, dst=self.folders.package / "licenses")
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.info.set_property("cmake_file_name", "spirv-reflect-static")
        self.info.set_property("cmake_target_name", "spirv-reflect-static")
        self.info.libs = ["spirv-reflect-static"]
        self.info.defines.append("SPIRV_REFLECT_USE_SYSTEM_SPIRV_H")
