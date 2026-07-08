from thirdparty import RecipeBase, RecipeOptions
from thirdparty.build import cross_building
from thirdparty.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.env import VirtualBuildEnv
from thirdparty.files import copy, get
from thirdparty.pkgconfig import PkgConfigDeps
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
        
    def configure(self):
        if self.settings.os == "Mac" and cross_building(self):
            self.options.build_cube = False

    def requirements(self):
        self.requires_tool("cmake")
        self.requires("vulkan-headers")
        self.requires("vulkan-loader")
        # vulkaninfo/cube use the X11/XCB/Wayland WSI system libraries on Linux.
        if self.settings.os in ("Linux", "FreeBSD"):
            self.requires("libxcb")
            self.requires("libx11")
            self.requires("libxrandr")
            self.requires("wayland")
            self.requires("wayland-protocols")
            self.requires("xkbcommon")
            if not self.conf.tools.gnu.pkg_config:
                self.requires_tool("pkgconf")

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
        if self.settings.os in ("Linux", "FreeBSD"):
            # cube/vulkaninfo include vulkan.h + X11/Xlib.h with the platform macros defined, so the
            # xcb/X11 headers (in package dirs, not /usr/include) must be visible to every target.
            include_flags = []
            for dep in self.dependencies.host.topological_sort.values():
                inc = dep.folders.package / "include"
                if inc.is_dir():
                    include_flags.append(f"-I{inc.as_posix()}")
            tc.extra_cflags.extend(include_flags)
            tc.extra_cxxflags.extend(include_flags)
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()
        if self.settings.os in ("Linux", "FreeBSD"):
            VirtualBuildEnv(self).generate()
            PkgConfigDeps(self).generate()

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
