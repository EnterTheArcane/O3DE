from thirdparty import RecipeBase, RecipeOptions
from thirdparty.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.files import copy, get
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class _Options(RecipeOptions):
    build_cube: bool = True
    build_vulkaninfo: bool = True
    build_icd: bool = True


class Recipe(RecipeBase[_Options]):
    name = "vulkan-tools"
    version = "1.4.350.1"
    license = "Apache-2.0"

    def latest_version(self):
        repo = GithubRepository(self, "KhronosGroup/Vulkan-Tools")
        return Version(repo.latest_tag("vulkan-sdk-").removeprefix("vulkan-sdk-"))

    def requirements(self):
        self.requires_tool("cmake")
        self.requires("vulkan-headers")
        self.requires("vulkan-loader")

    def source(self):
        get(
            self,
            url=f"https://github.com/KhronosGroup/Vulkan-Tools/archive/refs/tags/vulkan-sdk-{self.version}.tar.gz",
            sha256="502b53a585f49036e45372724f652bacc1fad2c62396e321bc8f5fbc031c14d5",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        vulkan_headers_pkg = self.dependencies["vulkan-headers"].folders.package
        tc.variables["VULKAN_HEADERS_INSTALL_DIR"] = vulkan_headers_pkg.as_posix()
        tc.variables["BUILD_CUBE"] = self.options.build_cube
        tc.variables["BUILD_VULKANINFO"] = self.options.build_vulkaninfo
        tc.variables["BUILD_ICD"] = self.options.build_icd
        tc.variables["VULKAN_TOOLS_TESTS"] = False
        if self.settings.os == "Mac":
            # Use system ICD discovery instead of requiring MoltenVK source tree layout
            tc.variables["APPLE_USE_SYSTEM_ICD"] = True
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE*", src=self.folders.source, dst=self.folders.package / "licenses")
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.info.includedirs = []
        self.info.libdirs = []
        self.info.bindirs = ["bin"]
