from thirdparty import RecipeBase, RecipeOptions
from thirdparty.cmake import CMakeToolchain, CMakeDeps, CMake
from thirdparty.files import copy, get, rmdir
from thirdparty.scm import Version
from thirdparty.scm.gitlab import GitlabRepository


class _Options(RecipeOptions):
    shared: bool = False
    fPIC: bool = True
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

    def config_options(self):
        if self.settings.arch != "ARM":
            del self.options.with_neon
            del self.options.with_arm_crc32
            del self.options.with_neon_dotprod
            del self.options.with_neon_i8mm
            del self.options.with_neon_sve
            del self.options.with_neon_sve2

    def requirements(self):
        self.requires_tool("cmake")
        if self.settings.arch in ("X64",):
            self.requires_tool("nasm")

    def source(self):
        get(
            self,
            url="https://gitlab.com/AOMediaCodec/SVT-AV1/-/archive/v4.1.0/SVT-AV1-v4.1.0.tar.gz",
            sha256="6c4c0c44ff0ba3d136d6f57f3a707f9de8e9c866f50f809c1d22a43f0d8c9583",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["BUILD_APPS"] = False
        tc.cache_variables["MINIMAL_BUILD"] = self.options.minimal_build
        if "with_neon" in self.options:
            tc.cache_variables["ENABLE_NEON"] = self.options.with_neon
        if "with_arm_crc32" in self.options:
            tc.cache_variables["ENABLE_ARM_CRC32"] = self.options.with_arm_crc32
        if "with_neon_dotprod" in self.options:
            tc.cache_variables["ENABLE_NEON_DOTPROD"] = self.options.with_neon_dotprod
        if "with_neon_i8mm" in self.options:
            tc.cache_variables["ENABLE_NEON_I8MM"] = self.options.with_neon_i8mm
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
        rmdir(self, self.folders.package / "lib" / "cmake")

    def package_info(self):
        self.info.components["encoder"].libs = ["SvtAv1Enc"]
        self.info.components["encoder"].includedirs = ["include/svt-av1"]
        self.info.components["encoder"].set_property("pkg_config_name", "SvtAv1Enc")
        if self.settings.os in ("FreeBSD", "Linux"):
            self.info.components["encoder"].system_libs = ["pthread", "dl", "m"]
        if self.settings.os == "Android":
            self.info.components["encoder"].system_libs = ["m"]
