from thirdparty import RecipeBase, RecipeOptions
from thirdparty.cmake import CMakeToolchain, CMakeDeps, CMake
from thirdparty.files import copy, get, rmdir
from thirdparty.scm import Version
from thirdparty.scm.gitlab import GitlabRepository


class _Options(RecipeOptions):
    shared: bool = False
    pic: bool = True
    minimal_build: bool = False
    with_neon: bool = True
    with_arm_crc32: bool = True
    with_neon_dotprod: bool = True
    with_neon_i8mm: bool = True
    with_neon_sve: bool = True
    with_neon_sve2: bool = True


class Recipe(RecipeBase[_Options]):
    name = "svt-av1"
    version = "4.1.0"
    license = "BSD-3-Clause"

    def latest_version(self):
        repo = GitlabRepository(self, "AOMediaCodec/SVT-AV1")
        return Version(repo.latest_release.removeprefix("v"))

    def configure(self):
        if self.settings.arch != "ARM":
            self.options.with_neon = False
            self.options.with_arm_crc32 = False
            self.options.with_neon_dotprod = False
            self.options.with_neon_i8mm = False
            self.options.with_neon_sve = False
            self.options.with_neon_sve2 = False

    def requirements(self):
        self.requires_tool("cmake")
        if self.settings.arch in ("X64",):
            self.requires_tool("nasm")

    def source(self):
        get(
            self,
            url=f"https://gitlab.com/AOMediaCodec/SVT-AV1/-/archive/v{self.version}/SVT-AV1-v{self.version}.tar.gz",
            sha256="6c4c0c44ff0ba3d136d6f57f3a707f9de8e9c866f50f809c1d22a43f0d8c9583",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["BUILD_APPS"] = False
        tc.cache_variables["MINIMAL_BUILD"] = self.options.minimal_build
        if self.settings.arch == "ARM":
            tc.cache_variables["ENABLE_NEON"] = self.options.with_neon
            tc.cache_variables["ENABLE_ARM_CRC32"] = self.options.with_arm_crc32
            tc.cache_variables["ENABLE_NEON_DOTPROD"] = self.options.with_neon_dotprod
            tc.cache_variables["ENABLE_NEON_I8MM"] = self.options.with_neon_i8mm
            tc.cache_variables["ENABLE_SVE"] = self.options.with_neon_sve
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
        rmdir(self, self.folders.package / "lib" / "cmake")

    def package_info(self):
        self.info.components["encoder"].libs = ["SvtAv1Enc"]
        self.info.components["encoder"].includedirs = ["include/svt-av1"]
        self.info.components["encoder"].set_property("pkg_config_name", "SvtAv1Enc")
        if self.settings.os in ("FreeBSD", "Linux"):
            self.info.components["encoder"].system_libs = ["pthread", "dl", "m"]
        if self.settings.os == "Android":
            self.info.components["encoder"].system_libs = ["m"]
