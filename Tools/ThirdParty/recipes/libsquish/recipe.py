import os

from thirdparty import RecipeBase
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import apply_patches, copy, get


class Recipe(RecipeBase):
    name = "libsquish"
    version = "1.15"
    license = "MIT"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "openmp": [True, False],
        "sse2_intrinsics": [True, False],
        "altivec_intrinsics": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "openmp": False,
        "sse2_intrinsics": False,
        "altivec_intrinsics": False,
    }

    @property
    def _sse2_compliant_archs(self):
        return ["X64"]

    @property
    def _altivec_compliant_archs(self):
        return ["ppc32be", "ppc32", "ppc64le", "ppc64"]

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC
        if self.settings.arch not in self._sse2_compliant_archs:
            del self.options.sse2_intrinsics
        if self.settings.arch not in self._altivec_compliant_archs:
            del self.options.altivec_intrinsics

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def source(self):
        get(
            self,
            url="https://sourceforge.net/projects/libsquish/files/libsquish-1.15.tgz",
            sha256="628796eeba608866183a61d080d46967c9dda6723bc0a3ec52324c85d2147269",
            destination=self.source_folder)
        apply_patches(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_SQUISH_WITH_OPENMP"] = self.options.openmp
        tc.variables["BUILD_SQUISH_WITH_SSE2"] = self.options.get_safe("sse2_intrinsics") or False
        tc.variables["BUILD_SQUISH_WITH_ALTIVEC"] = self.options.get_safe("altivec_intrinsics") or False
        tc.variables["BUILD_SQUISH_EXTRA"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE.txt", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["squishd" if self.settings.build_type == "Debug" else "squish"]
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs.append("m")
