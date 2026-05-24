import os

from thirdparty import RecipeBase
from thirdparty.tools.build import cross_building, stdcpp_library
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.tools.env import VirtualBuildEnv
from thirdparty.tools.files import apply_patches, copy, get, replace_in_file, rename, rm, rmdir, save
from thirdparty.tools.microsoft import is_msvc, is_msvc_static_runtime


class Recipe(RecipeBase):
    name = "libx265"
    version = "4.2"
    license = ("GPL-2.0-only", "commercial")  # https://bitbucket.org/multicoreware/x265/src/default/COPYING

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "assembly": [True, False],
        "bit_depth": [8, 10, 12],
        "HDR10": [True, False],
        "SVG_HEVC_encoder": [True, False],
        "with_numa": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "assembly": True,
        "bit_depth": 8,
        "HDR10": False,
        "SVG_HEVC_encoder": False,
        "with_numa": False,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC
        if self.settings.os != "Linux":
            del self.options.with_numa
        # FIXME: Disable assembly by default if host is arm and compiler apple-clang for the moment.
        # Indeed, apple-clang is not able to understand some asm instructions of libx265
        # FIXME: Disable assembly by default if host is Android for the moment. It fails to build
        if (self.settings.compiler == "apple-clang" and "arm" in self.settings.arch) or self.settings.os == "Android":
            self.options.assembly = False
        if is_msvc(self) and self.settings.arch == "armv8":
            # Build errors, possibly unsupported
            self.options.assembly = False

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def requirements(self):
        if self.options.get_safe("with_numa", False):
            self.requires("libnuma")

    def build_requirements(self):
        if self.options.assembly:
            if self.settings.arch in ["x86", "x86_64"]:
                self.tool_requires("nasm")

    def source(self):
        get(
            self,
            url="https://downloads.videolan.org/videolan/x265/x265_4.2.tar.gz",
            sha256="40b1ea0453e0309f0eba934e0ddf533f8f6295966679e8894e8f1c1c8d5e1210",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        env = VirtualBuildEnv(self)
        env.generate()
        tc = CMakeToolchain(self)
        tc.variables["ENABLE_PIC"] = self.options.get_safe("fPIC", True)
        tc.variables["ENABLE_SHARED"] = self.options.shared
        tc.variables["ENABLE_ASSEMBLY"] = self.options.assembly
        tc.variables["ENABLE_LIBNUMA"] = self.options.get_safe("with_numa", False)
        if self.settings.os == "Macos":
            tc.variables["CMAKE_SHARED_LINKER_FLAGS"] = "-Wl,-read_only_relocs,suppress"
        tc.variables["HIGH_BIT_DEPTH"] = self.options.bit_depth != 8
        tc.variables["MAIN12"] = self.options.bit_depth == 12
        tc.variables["ENABLE_HDR10_PLUS"] = self.options.HDR10
        tc.variables["ENABLE_SVT_HEVC"] = self.options.SVG_HEVC_encoder
        if is_msvc(self):
            tc.variables["STATIC_LINK_CRT"] = is_msvc_static_runtime(self)
            tc.cache_variables["CMAKE_POLICY_DEFAULT_CMP0091"] = "NEW"
        if self.settings.os == "Linux":
            tc.variables["PLATFORM_LIBS"] = "dl"
        if "arm" in self.settings.arch:
            tc.variables["CROSS_COMPILE_ARM"] = cross_building(self)
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def _patch_sources(self):
        apply_patches(self)
        cmakelists = os.path.join(self.source_folder, "source", "CMakeLists.txt")
        replace_in_file(self, cmakelists,
                                "if((WIN32 AND ENABLE_CLI) OR (WIN32 AND ENABLE_SHARED))",
                                "if(FALSE)")
        if self.settings.os == "Android":
            replace_in_file(self, cmakelists, "list(APPEND PLATFORM_LIBS pthread)", "")
            replace_in_file(self, cmakelists, "list(APPEND PLATFORM_LIBS rt)", "")
        # The finite-math-only optimization has no effect and can cause linking errors
        # when linked against glibc >= 2.31
        replace_in_file(self, cmakelists,
                        "add_definitions(-ffast-math)",
                        "add_definitions(-ffast-math -fno-finite-math-only)")

    def build(self):
        self._patch_sources()
        cmake = CMake(self)
        cmake.configure(build_script_folder=os.path.join(self.source_folder, "source"))
        cmake.build()

    def package(self):
        copy(self, "COPYING", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()

        if self.options.shared:
            if is_msvc(self):
                static_lib = "x265-static.lib"
            else:
                static_lib = "libx265.a"
            os.unlink(os.path.join(self.package_folder, "lib", static_lib))

        if is_msvc(self):
            name = "libx265.lib" if self.options.shared else "x265-static.lib"
            rename(self, os.path.join(self.package_folder, "lib", name),
                         os.path.join(self.package_folder, "lib", "x265.lib"))

        if self.settings.os == "Windows" and self.options.shared:
            rm(self, "*[!.dll]", os.path.join(self.package_folder, "bin"))
        else:
            rmdir(self, os.path.join(self.package_folder, "bin"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))

    def package_info(self):
        self.cpp_info.set_property("pkg_config_name", "x265")
        self.cpp_info.libs = ["x265"]
        if self.settings.os == "Windows":
            if self.options.shared:
                self.cpp_info.defines.append("X265_API_IMPORTS")
            if not self.options.shared:
                self.cpp_info.system_libs.extend(["advapi32"])
        elif self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs.extend(["dl", "pthread", "m", "rt"])
            if not self.options.shared:
                self.cpp_info.sharedlinkflags = ["-Wl,-Bsymbolic,-znoexecstack"]
        elif self.settings.os == "Android":
            self.cpp_info.system_libs.extend(["dl", "m"])
        if not self.options.shared:
            libcxx = stdcpp_library(self)
            if libcxx:
                if self.settings.os == "Android" and self.settings.compiler.libcxx == "c++_static":
                    self.cpp_info.system_libs.append("c++abi")
                self.cpp_info.system_libs.append(libcxx)
