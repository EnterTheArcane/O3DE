from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.tools.env import VirtualBuildEnv, VirtualRunEnv
from thirdparty.tools.files import apply_conandata_patches, copy, get, replace_in_file, rmdir, save
from thirdparty.tools.gnu import PkgConfigDeps
from thirdparty.tools.microsoft import is_msvc, is_msvc_static_runtime
import os
import textwrap

class Recipe(RecipeBase):
    name = "glfw"
    version = "3.4"
    license = "Zlib"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "vulkan_static": [True, False],
        "with_x11": [True, False],
        "with_wayland": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "vulkan_static": False,
        "with_x11": True,
        "with_wayland": False,
    }

    @property
    def _has_build_profile(self):
        return hasattr(self, "settings_build")

    def config_options(self):
        if self.settings.os == "Windows":
            self.options.rm_safe("fPIC")
        if self.settings.os != "Linux":
            self.options.rm_safe("with_wayland")
        if self.settings.os not in ["Linux", "FreeBSD"]:
            self.options.rm_safe("with_x11")
        self.options.rm_safe("vulkan_static")

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

        if self.options.get_safe("with_wayland"):
            self.options["xkbcommon"].with_wayland = True
            self.options["wayland"].shared = True
            self.options["xkbcommon"].shared = True

    def requirements(self):
        # libs=False because glfw does not link to opengl, it
        # loads it via dlopen or equivalent
        self.requires("opengl", libs=False, transitive_headers=True)
        if self.options.get_safe("vulkan_static"):
            self.requires("vulkan-loader")
        if self.settings.os in ["Linux", "FreeBSD"]:
            if self.options.get_safe("with_x11", True):
                self.requires("xorg")
        if self.options.get_safe("with_wayland"):
            self.requires("wayland")
            self.requires("xkbcommon")

    def build_requirements(self):
        if self.options.get_safe("with_wayland"):
            self.tool_requires("wayland-protocols")
            if self._has_build_profile:
                self.tool_requires("wayland")
            if not self.conf.get("tools.gnu:pkg_config", check_type=str):
                self.tool_requires("pkgconf")

    def source(self):
        get(
            self,
            url="https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.zip",
            sha256="b5ec004b2712fd08e8861dc271428f048775200a2df719ccf575143ba749a3e9",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        env = VirtualBuildEnv(self)
        env.generate()
        if self.options.get_safe("with_wayland") and not self._has_build_profile:
            env = VirtualRunEnv(self)
            env.generate(scope="build")

        tc = CMakeToolchain(self)
        tc.cache_variables["GLFW_BUILD_DOCS"] = False
        tc.cache_variables["GLFW_BUILD_EXAMPLES"] = False
        tc.cache_variables["GLFW_BUILD_TESTS"] = False
        tc.cache_variables["GLFW_INSTALL"] = True
        tc.cache_variables["GLFW_BUILD_X11"] = self.options.get_safe("with_x11", False)
        tc.cache_variables["GLFW_BUILD_WAYLAND"] = self.options.get_safe("with_wayland", False)
        if is_msvc(self):
            tc.cache_variables["USE_MSVC_RUNTIME_LIBRARY_DLL"] = not is_msvc_static_runtime(self)
        tc.generate()
        cmake_deps = CMakeDeps(self)
        if self.options.get_safe("with_wayland"):
            cmake_deps.set_property("xkbcommon", "cmake_file_name", "XKBCommon")
        cmake_deps.generate()
        if self.options.get_safe("with_wayland"):
            pkg_config_deps = PkgConfigDeps(self)
            if self._has_build_profile:
                pkg_config_deps.build_context_activated = ["wayland-protocols"]
            else:
                # Manually generate pkgconfig file of wayland-protocols since
                # PkgConfigDeps.build_context_activated can't work with legacy 1 profile
                wp_prefix = self.dependencies.build["wayland-protocols"].package_folder
                wp_version = self.dependencies.build["wayland-protocols"].ref.version
                wp_pkg_content = textwrap.dedent(f"""\
                    prefix={wp_prefix}
                    datarootdir=${{prefix}}/res
                    pkgdatadir=${{datarootdir}}/wayland-protocols
                    Name: Wayland Protocols
                    Description: Wayland protocol files
                    Version: {wp_version}
                """)
                save(self, os.path.join(self.generators_folder, "wayland-protocols.pc"), wp_pkg_content)
            pkg_config_deps.generate()

    def _patch_sources(self):
        apply_conandata_patches(self)
        # don't force PIC
        replace_in_file(self, os.path.join(self.source_folder, "src", "CMakeLists.txt"),
                        "POSITION_INDEPENDENT_CODE ON", "")
        # don't force static link to libgcc if MinGW
        replace_in_file(self, os.path.join(self.source_folder, "src", "CMakeLists.txt"),
                        "target_link_libraries(glfw PRIVATE \"-static-libgcc\")", "")

        # Allow to link vulkan-loader into shared glfw
        if self.options.get_safe("vulkan_static"):
            cmakelists = os.path.join(self.source_folder, "CMakeLists.txt")
            replace_in_file(
                self,
                cmakelists,
                'message(FATAL_ERROR "You are trying to link the Vulkan loader static library into the GLFW shared library")',
                "",
            )
            vulkan_lib = self.dependencies["vulkan-loader"].cpp_info.libs[0]
            replace_in_file(
                self,
                cmakelists,
                'list(APPEND glfw_PKG_DEPS "vulkan")',
                f'list(APPEND glfw_PKG_DEPS "vulkan")\nlist(APPEND glfw_LIBRARIES "{vulkan_lib}")',
            )

    def build(self):
        self._patch_sources()
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE*", self.source_folder, os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
        self._create_cmake_module_alias_targets(
            os.path.join(self.package_folder, self._module_file_rel_path),
            {"glfw": "glfw::glfw"}
        )

    def _create_cmake_module_alias_targets(self, module_file, targets):
        content = ""
        for alias, aliased in targets.items():
            content += textwrap.dedent(f"""\
                if(TARGET {aliased} AND NOT TARGET {alias})
                    add_library({alias} INTERFACE IMPORTED)
                    set_property(TARGET {alias} PROPERTY INTERFACE_LINK_LIBRARIES {aliased})
                endif()
            """)
        save(self, module_file, content)

    @property
    def _module_file_rel_path(self):
        return os.path.join("lib", "cmake", f"conan-official-{self.name}-targets.cmake")

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "glfw3")
        self.cpp_info.set_property("cmake_target_name", "glfw")
        self.cpp_info.set_property("pkg_config_name", "glfw3")
        libname = "glfw"
        if self.settings.os == "Windows" or not self.options.shared:
            libname += "3"
        if self.settings.os == "Windows" and self.options.shared:
            libname += "dll"
            self.cpp_info.defines.append("GLFW_DLL")
        self.cpp_info.libs = [libname]
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs.extend(["m", "pthread", "dl", "rt"])
        elif self.settings.os == "Windows":
            self.cpp_info.system_libs.append("gdi32")
        elif self.settings.os == "Macos":
            self.cpp_info.frameworks.extend([
                "AppKit", "Cocoa", "CoreFoundation", "CoreGraphics",
                "CoreServices", "Foundation", "IOKit",
            ])
        self.cpp_info.requires = ["opengl::opengl"]
        if self.options.get_safe("vulkan_static"):
            self.cpp_info.requires.append("vulkan-loader::vulkan-loader")
        if self.settings.os in ["Linux", "FreeBSD"]:
            if self.options.get_safe("with_x11", True):
                # https://github.com/glfw/glfw/blob/3.4/src/CMakeLists.txt#L181-L218
                # https://github.com/glfw/glfw/blob/3.3.2/CMakeLists.txt#L196-L233
                self.cpp_info.requires.extend([
                    "xorg::x11", # Also includes Xkb and Xshape
                    "xorg::xrandr",
                    "xorg::xinerama",
                    "xorg::xcursor",
                    "xorg::xi",
                ])
        if self.options.get_safe("with_wayland"):
            # https://github.com/glfw/glfw/blob/3.4/src/CMakeLists.txt#L163-L167
            self.cpp_info.requires.extend([
                "wayland::wayland-client",
                "wayland::wayland-cursor",
                "wayland::wayland-egl",
                "xkbcommon::xkbcommon"
            ])

        # Starting with version 3.4, glfw loads the platform libraries at runtime
        # and hence does not need to link with them.
        self.cpp_info.requires = []
