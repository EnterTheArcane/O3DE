from thirdparty import RecipeBase, RecipeOptions
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.env import VirtualBuildEnv
from thirdparty.files import apply_patches, copy, get, rmdir
from thirdparty.scm import Version
from thirdparty.scm.google import GoogleSourceRepository


class _Options(RecipeOptions):
    shared: bool = False
    fPIC: bool = True
    assembly: bool = False


class Recipe(RecipeBase[_Options]):
    name = "libaom-av1"
    version = "3.14.1"
    license = "BSD-2-Clause"

    def latest_version(self):
        repo = GoogleSourceRepository(self, "https://aomedia.googlesource.com/aom")
        return Version(repo.latest_release.removeprefix("v"))

    def config_options(self):
        if self.settings.arch not in ("X64",):
            del self.options.assembly

    def configure(self):
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def requirements(self):
        if self.options.get_safe("assembly", False):
            self.requires_tool("nasm")
        if self.settings.os == "Windows":
            self.requires_tool("strawberryperl")

    def source(self):
        get(
            self,
            url="https://storage.googleapis.com/aom-releases/libaom-3.14.1.tar.gz",
            sha256="44bf90dbd23e734d50e70a8c41c285193922938bd0d3bc2ee56764d181d55ef5",
            destination=self.folders.source,
            strip_root=True)
        apply_patches(self)

    def generate(self):
        VirtualBuildEnv(self).generate()
        tc = CMakeToolchain(self)
        tc.variables["ENABLE_EXAMPLES"] = False
        tc.variables["ENABLE_TESTS"] = False
        tc.variables["ENABLE_DOCS"] = False
        tc.variables["ENABLE_TOOLS"] = False
        if not self.options.get_safe("assembly", False):
            # make non-assembly build
            tc.variables["AOM_TARGET_CPU"] = "generic"
        # libyuv is used for examples, tests and non-essential 'dump_obu' tool so it is disabled
        # required to be 1/0 instead of False
        tc.variables["CONFIG_LIBYUV"] = 0
        # webm is not yet packaged
        tc.variables["CONFIG_WEBM_IO"] = 0
        # Requires C11 or higher 
        tc.variables["CMAKE_C_STANDARD"] = "11"
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.folders.source, dst=self.folders.package / "licenses")
        cmake = CMake(self)
        cmake.install()
        rmdir(self, self.folders.package / "lib" / "pkgconfig")

    def package_info(self):
        self.info.set_property("pkg_config_name", "aom")
        self.info.set_property("cmake_file_name", "AOM")
        self.info.set_property("cmake_target_name", "AOM::aom")
        if not self.options.shared:
            self.info.set_property("cmake_target_aliases", ["AOM::aom_static"])
        lib = "aom"
        if self.settings.os == "Windows" and self.options.shared:
            lib = "aom_dll"
        self.info.libs = [lib]
        if self.settings.os in ("FreeBSD", "Linux"):
            self.info.system_libs = ["pthread", "m"]
