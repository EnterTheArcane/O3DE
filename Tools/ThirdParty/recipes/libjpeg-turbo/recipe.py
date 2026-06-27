from thirdparty import RecipeBase, RecipeOptions
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.env import VirtualBuildEnv
from thirdparty.files import copy, get, replace_in_file, rm, rmdir, apply_patches
from thirdparty.microsoft import is_msvc
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class _Options(RecipeOptions):
    shared: bool = False
    fPIC: bool = True
    SIMD: bool = True
    arithmetic_encoder: bool = True
    arithmetic_decoder: bool = True
    libjpeg7_compatibility: bool = True
    libjpeg8_compatibility: bool = True
    mem_src_dst: bool = True
    turbojpeg: bool = True
    java: bool = False
    enable12bit: bool = False


class Recipe(RecipeBase[_Options]):
    name = "libjpeg-turbo"
    version = "3.1.4.1"
    license = "IJG", "BSD-3-Clause", "Zlib"

    def latest_version(self):
        repo = GithubRepository(self, "libjpeg-turbo/libjpeg-turbo")
        return Version(repo.latest_release)

    def config_options(self):
        del self.options.enable12bit
        del self.options.mem_src_dst

    def configure(self):
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

        if self.options.get_safe("enable12bit"):
            del self.options.java
            del self.options.turbojpeg
        if self.options.get_safe("enable12bit") or self.settings.os == "Emscripten":
            del self.options.SIMD
        if self.options.get_safe("enable12bit") or self.options.libjpeg7_compatibility or self.options.libjpeg8_compatibility:
            del self.options.arithmetic_encoder
            del self.options.arithmetic_decoder
        if self.options.libjpeg8_compatibility:
            self.options.rm_safe("mem_src_dst")

    def requirements(self):
        if self.options.get_safe("SIMD") and self.settings.arch in ["X64"]:
            self.requires_tool("nasm")

    def source(self):
        get(
            self,
            url="https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/3.1.4.1/libjpeg-turbo-3.1.4.1.tar.gz",
            sha256="ecae8008e2cc9ade2f2c1bb9d5e6d4fb73e7c433866a056bd82980741571a022",
            destination=self.folders.source,
            strip_root=True)
        apply_patches(self)
        replace_in_file(
            self,
            self.folders.source / "sharedlib" / "CMakeLists.txt",
            """string(REGEX REPLACE "/MT" "/MD" ${var} "${${var}}")""",
            "")

    @property
    def _is_arithmetic_encoding_enabled(self):
        return self.options.get_safe("arithmetic_encoder", False) or \
            self.options.libjpeg7_compatibility or self.options.libjpeg8_compatibility

    @property
    def _is_arithmetic_decoding_enabled(self):
        return self.options.get_safe("arithmetic_decoder", False) or \
            self.options.libjpeg7_compatibility or self.options.libjpeg8_compatibility

    def generate(self):
        env = VirtualBuildEnv(self)
        env.generate()

        tc = CMakeToolchain(self)
        tc.variables["ENABLE_STATIC"] = not self.options.shared
        tc.variables["ENABLE_SHARED"] = self.options.shared
        tc.variables["WITH_SIMD"] = self.options.get_safe("SIMD", False)
        tc.variables["WITH_ARITH_ENC"] = self._is_arithmetic_encoding_enabled
        tc.variables["WITH_ARITH_DEC"] = self._is_arithmetic_decoding_enabled
        tc.variables["WITH_JPEG7"] = self.options.libjpeg7_compatibility
        tc.variables["WITH_JPEG8"] = self.options.libjpeg8_compatibility
        tc.variables["WITH_TURBOJPEG"] = self.options.get_safe("turbojpeg", False)
        tc.variables["WITH_JAVA"] = self.options.get_safe("java", False)
        tc.cache_variables["WITH_TOOLS"] = False
        if is_msvc(self):
            tc.variables["WITH_CRT_DLL"] = True  # avoid replacing /MD by /MT in compiler flags
        if self.options.get_safe("java", False):
            tc.cache_variables["CMAKE_INSTALL_JAVADIR"] = self.folders.package / "lib" / "java"
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE.md", src=self.folders.source, dst=self.folders.package / "licenses")
        copy(self, "README.ijg", src=self.folders.source, dst=self.folders.package / "licenses")
        cmake = CMake(self)
        cmake.install()
        # remove unneeded directories
        rmdir(self, self.folders.package / "share")
        rmdir(self, self.folders.package / "lib" / "pkgconfig")
        rmdir(self, self.folders.package / "lib" / "cmake")
        rmdir(self, self.folders.package / "doc")
        # remove binaries and pdb files
        for pattern_to_remove in ["cjpeg*", "djpeg*", "jpegtran*", "tjbench*", "wrjpgcom*", "rdjpgcom*", "*.pdb"]:
            rm(self, pattern_to_remove, self.folders.package / "bin")

    def package_info(self):
        self.info.set_property("cmake_file_name", "libjpeg-turbo")

        cmake_target_suffix = "-static" if not self.options.shared else ""
        lib_suffix = "-static" if is_msvc(self) and not self.options.shared else ""

        self.info.components["jpeg"].set_property("cmake_target_name", f"libjpeg-turbo::jpeg{cmake_target_suffix}")
        self.info.components["jpeg"].set_property("pkg_config_name", "libjpeg")
        self.info.components["jpeg"].libs = [f"jpeg{lib_suffix}"]

        if self.options.get_safe("turbojpeg"):
            self.info.components["turbojpeg"].set_property("cmake_target_name", f"libjpeg-turbo::turbojpeg{cmake_target_suffix}")
            self.info.components["turbojpeg"].set_property("pkg_config_name", "libturbojpeg")
            self.info.components["turbojpeg"].libs = [f"turbojpeg{lib_suffix}"]
