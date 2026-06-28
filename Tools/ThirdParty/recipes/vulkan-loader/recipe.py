from thirdparty import RecipeBase, RecipeOptions
from thirdparty.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.files import copy, get, replace_in_file, rmdir
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class _Options(RecipeOptions):
    shared: bool = True
    fPIC: bool = True


class Recipe(RecipeBase[_Options]):
    name = "vulkan-loader"
    version = "1.4.355"
    license = "Apache-2.0"

    def latest_version(self):
        repo = GithubRepository(self, "KhronosGroup/Vulkan-Loader")
        return Version(repo.latest_tag("vulkan-sdk-").removeprefix("vulkan-sdk-"))

    def configure(self):
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def requirements(self):
        self.requires_tool("cmake")
        self.requires("vulkan-headers")

    def source(self):
        get(
            self,
            url="https://github.com/KhronosGroup/Vulkan-Loader/archive/refs/tags/v1.4.355.tar.gz",
            sha256="dd52a5dea4c1a52607849ff187da2af6df6889d59ae7efb0b1fafed1715ea3ee",
            destination=self.folders.source,
            strip_root=True)
        replace_in_file(
            self,
            self.folders.source / "CMakeLists.txt",
            "set(CMAKE_MSVC_RUNTIME_LIBRARY \"MultiThreaded$<$<CONFIG:Debug>:Debug>\")",
            "")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_TESTS"] = False
        tc.variables["LOADER_CODEGEN"] = False
        vulkan_headers = self.dependencies["vulkan-headers"].folders.package.as_posix()
        tc.variables["VULKAN_HEADERS_INSTALL_DIR"] = vulkan_headers
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE.txt", src=self.folders.source, dst=self.folders.package / "licenses")
        cmake = CMake(self)
        cmake.install()
        rmdir(self, self.folders.package / "lib" / "cmake")
        rmdir(self, self.folders.package / "lib" / "pkgconfig")
        rmdir(self, self.folders.package / "loader")

    def package_info(self):
        self.info.set_property("cmake_file_name", "VulkanLoader")
        self.info.set_property("cmake_target_name", "Vulkan::Loader")
        self.info.set_property("cmake_target_aliases", ["Vulkan::Vulkan"])
        self.info.includedirs = []
        if self.settings.os == "Windows":
            self.info.libs = ["vulkan-1"]
        else:
            self.info.libs = ["vulkan"]
