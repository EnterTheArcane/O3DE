from thirdparty import RecipeBase, RecipeOptions
from thirdparty.cmake import CMakeToolchain, CMakeDeps, CMake
from thirdparty.files import copy, get, rmdir
from thirdparty.scm import Version
from thirdparty.scm.gitlab import GitlabRepository


class _Options(RecipeOptions):
    shared: bool = False
    fPIC: bool = True
    build_encoder: bool = True
    build_decoder: bool = True
    minimal_build: bool = False
    with_neon: bool = True
    with_arm_crc32: bool = True
    with_neon_dotprod: bool = True
    with_neon_i8mm: bool = True
    with_neon_sve: bool = True
    with_neon_sve2: bool = True


class Recipe(RecipeBase[_Options]):
    name = "svt-av1"
    version = "2.2.1"
    license = "BSD-3-Clause"

    def latest_version(self):
        repo = GitlabRepository(self, "AOMediaCodec/SVT-AV1")
        return Version(repo.latest_release.removeprefix("v"))

    def config_options(self):
        del self.options.build_decoder
        if self.settings.arch not in ("ARM",):
            del self.options.with_neon
            del self.options.with_arm_crc32
            del self.options.with_neon_dotprod
            del self.options.with_neon_i8mm
            del self.options.with_neon_sve
            del self.options.with_neon_sve2

    def requirements(self):
        self.requires("cpuinfo")
        self.requires_tool("cmake")
        if self.settings.arch in ("X64",):
            self.requires_tool("nasm")

    def source(self):
        get(
            self,
            url="https://gitlab.com/AOMediaCodec/SVT-AV1/-/archive/v2.2.1/SVT-AV1-v2.2.1.tar.gz",
            sha256="d02b54685542de0236bce4be1b50912aba68aff997c43b350d84a518df0cf4e5",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["BUILD_APPS"] = False
        tc.cache_variables["BUILD_ENC"] = self.options.build_encoder
        tc.cache_variables["USE_EXTERNAL_CPUINFO"] = True
        if self.settings.arch in ("X64",):
            tc.cache_variables["ENABLE_NASM"] = True
        tc.cache_variables["MINIMAL_BUILD"] = self.options.minimal_build
        if "with_neon" in self.options:
            tc.cache_variables["ENABLE_NEON"] = self.options.with_neon
        if "with_arm_crc32" in self.options:
            tc.cache_variables["ENABLE_ARM_CRC32"] = self.options.with_arm_crc32
        if "with_neon_dotprod" in self.options:
            tc.cache_variables["ENABLE_NEON_DOTPROD"] = self.options.with_neon_dotprod
        if "with_neon_i8mm" in self.options:
            tc.cache_variables["ENABLE_NEON_i8MM"] = self.options.with_neon_i8mm
        if "with_neon_sve" in self.options:
            tc.cache_variables["ENABLE_SVE"] = self.options.with_neon_sve
        if "with_neon_sve2" in self.options:
            tc.cache_variables["ENABLE_SVE2"] = self.options.with_neon_sve2
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        for license_file in ("LICENSE.md", "PATENTS.md"):
            copy(self, license_file, self.folders.source, dst=self.folders.package / "licenses")
        cmake = CMake(self)
        cmake.configure()
        cmake.install()
        rmdir(self, self.folders.package / "lib" / "pkgconfig")

    def package_info(self):
        if self.options.build_encoder:
            self.info.components["encoder"].libs = ["SvtAv1Enc"]
            self.info.components["encoder"].includedirs = ["include/svt-av1"]
            self.info.components["encoder"].set_property("pkg_config_name", "SvtAv1Enc")
            self.info.components["encoder"].requires = ["cpuinfo::cpuinfo"]
            if self.settings.os in ("FreeBSD", "Linux"):
                self.info.components["encoder"].system_libs = ["pthread", "dl", "m"]
            if self.settings.os == "Android":
                self.info.components["encoder"].system_libs = ["m"]
        if self.options.get_safe("build_decoder"):
            self.info.components["decoder"].libs = ["SvtAv1Dec"]
            self.info.components["decoder"].includedirs = ["include/svt-av1"]
            self.info.components["decoder"].set_property("pkg_config_name", "SvtAv1Dec")
            self.info.components["decoder"].requires = ["cpuinfo::cpuinfo"]
            if self.settings.os in ("FreeBSD", "Linux"):
                self.info.components["decoder"].system_libs = ["pthread", "dl", "m"]
            if self.settings.os == "Android":
                self.info.components["decoder"].system_libs = ["m"]
