from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.tools.files import apply_patches, copy, get, replace_in_file, rmdir
from thirdparty.tools.microsoft import is_msvc, is_msvc_static_runtime
from thirdparty.tools.scm import Version
import os


class Recipe(RecipeBase):
    name = "pcre2"
    version = "10.44"
    license = "BSD-3-Clause"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "build_pcre2_8": [True, False],
        "build_pcre2_16": [True, False],
        "build_pcre2_32": [True, False],
        "build_pcre2grep": [True, False],
        "with_zlib": [True, False],
        "with_bzip2": [True, False],
        "support_jit": [True, False],
        "grep_support_callout_fork": [True, False],
        "link_size": [2, 3, 4],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "build_pcre2_8": True,
        "build_pcre2_16": True,
        "build_pcre2_32": True,
        "build_pcre2grep": True,
        "with_zlib": True,
        "with_bzip2": True,
        "support_jit": False,
        "grep_support_callout_fork": True,
        "link_size": 2,
    }

    def requirements(self) -> list[str]:
        return ["zlib", "bzip2"]

    def source(self):
        get(
            url="https://github.com/PCRE2Project/pcre2/releases/download/pcre2-10.44/pcre2-10.44.tar.bz2",
            dest=self.source_folder,
            sha256="d34f02e113cf7193a1ebf2770d3ac527088d485d4e047ed10e5d217c6ef5de96",
        )

    def generate(self):
        tc = CMakeToolchain(self)
        # Mandatory because upstream CMakeLists overrides BUILD_SHARED_LIBS as a CACHE variable
        # (see https://github.com/conan-io/conan/issues/11840)
        tc.variables["BUILD_SHARED_LIBS"] = self.options.shared
        if Version(self.version) >= "10.38":
            tc.variables["BUILD_STATIC_LIBS"] = not self.options.shared
        tc.variables["PCRE2_BUILD_PCRE2GREP"] = self.options.build_pcre2grep
        tc.variables["PCRE2_SUPPORT_LIBZ"] = self.options.get("with_zlib", False)
        tc.variables["PCRE2_SUPPORT_LIBBZ2"] = self.options.get("with_bzip2", False)
        tc.variables["PCRE2_BUILD_TESTS"] = False
        if self.is_windows:
            tc.variables["PCRE2_STATIC_RUNTIME"] = False
        tc.variables["PCRE2_DEBUG"] = self.build_type == "Debug"
        tc.variables["PCRE2_BUILD_PCRE2_8"] = self.options.build_pcre2_8
        tc.variables["PCRE2_BUILD_PCRE2_16"] = self.options.build_pcre2_16
        tc.variables["PCRE2_BUILD_PCRE2_32"] = self.options.build_pcre2_32
        tc.variables["PCRE2_SUPPORT_JIT"] = self.options.support_jit
        tc.variables["PCRE2_LINK_SIZE"] = self.options.link_size
        tc.variables["PCRE2GREP_SUPPORT_CALLOUT_FORK"] = self.options.get(
            "grep_support_callout_fork", False
        )
        if Version(self.version) < "10.38":
            # relocatable shared libs on Macos
            tc.cache_variables["CMAKE_POLICY_DEFAULT_CMP0042"] = "NEW"
        if Version(self.version) < "10.43":
            tc.cache_variables["CMAKE_POLICY_VERSION_MINIMUM"] = (
                "3.5"  # CMake 4 support
            )
        tc.generate()

        cd = CMakeDeps(self)
        cd.generate()

    def _patch_sources(self):
        apply_patches(self)
        cmakelists = os.path.join(self.source_folder, "CMakeLists.txt")
        # Do not add ${PROJECT_SOURCE_DIR}/cmake because it contains a custom
        # FindPackageHandleStandardArgs.cmake which can break conan generators
        if Version(self.version) < "10.34":
            replace_in_file(
                cmakelists, "SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)", ""
            )
        else:
            replace_in_file(
                cmakelists,
                "LIST(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)",
                "",
            )
        # Avoid CMP0006 error (macos bundle)
        replace_in_file(
            cmakelists,
            "RUNTIME DESTINATION bin",
            "RUNTIME DESTINATION bin BUNDLE DESTINATION bin",
        )
        # pcre2-config does not correctly include '-static' in static library names
        if self.is_windows:
            replace = None
            if Version(self.version) > "10.42":
                replace = "configure_file(pcre2-config.in"
            elif Version(self.version) >= "10.38":
                replace = "CONFIGURE_FILE(pcre2-config.in"
            postfix = "-static" if not self.options.shared else ""
            if replace:
                if self.build_type == "Debug":
                    postfix += "d"
                replace_in_file(
                    cmakelists, replace, f'set(LIB_POSTFIX "{postfix}")\n{replace}'
                )

    def build(self):
        self._patch_sources()
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(
            "LICENCE",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        cmake = CMake(self)
        cmake.install()
        rmdir(os.path.join(self.package_folder, "cmake"))
        rmdir(os.path.join(self.package_folder, "man"))
        rmdir(os.path.join(self.package_folder, "share"))
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))

    def _lib_name(self, name):
        libname = name
        if (
            Version(self.version) >= "10.38"
            and self.is_windows
            and not self.options.shared
        ):
            libname += "-static"
        if self.is_windows:
            if self.build_type == "Debug":
                libname += "d"
            if False and self.options.shared:  # GCC-specific, not applicable on Windows
                libname += ".dll"
        return libname
