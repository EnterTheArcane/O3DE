import re

from thirdparty import RecipeBase, RecipeOptions
from thirdparty.apple import fix_apple_shared_install_name
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import copy, get, rmdir, load
from thirdparty.microsoft import is_msvc
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class _Options(RecipeOptions):
    shared: bool = False
    fPIC: bool = True
    zlib_compat: bool = False


class Recipe(RecipeBase[_Options]):
    name = "zlib-ng"
    version = "2.3.3"
    license = "Zlib"

    def latest_version(self):
        repo = GithubRepository(self, "zlib-ng/zlib-ng")
        return Version(repo.latest_release)

    @property
    def _is_windows(self):
        return self.settings.os in ["Windows", "WindowsStore"]

    @property
    def _zlib_compat_version(self):
        return "1.3.1"

    def config_options(self):
        if self._is_windows:
            del self.options.fPIC

    def configure(self):
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def source(self):
        get(
            self,
            url="https://github.com/zlib-ng/zlib-ng/archive/refs/tags/2.3.3.tar.gz",
            sha256="f9c65aa9c852eb8255b636fd9f07ce1c406f061ec19a2e7d508b318ca0c907d1",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["BUILD_TESTING"] = False

        tc.variables["ZLIB_COMPAT"] = self.options.zlib_compat
        tc.variables["WITH_GZFILEOP"] = True
        tc.variables["WITH_OPTIM"] = True
        tc.variables["WITH_NEW_STRATEGIES"] = True
        tc.variables["WITH_NATIVE_INSTRUCTIONS"] = True
        tc.variables["WITH_REDUCED_MEM"] = False
        tc.variables["WITH_RUNTIME_CPU_DETECTION"] = True
        tc.generate()

    def _get_zlib_header_version(self):
        zlib_h = load(self, self.folders.source / "zlib.h.in")
        match = re.search(r'#define\s+ZLIB_VERSION\s+"([0-9]+\.[0-9]+\.[0-9]+)\.zlib-ng"', zlib_h)
        return match.group(1) if match and match.group(1) else None

    def build(self):
        header_version = self._get_zlib_header_version()
        if header_version and header_version != self._zlib_compat_version:
            raise RecipeException(f"the zlib compatibility version ({header_version}) is not correctly recorded in the recipe for this zlib-ng version ({self.version})")

        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        license_folder = self.folders.package / "licenses"
        copy(self, "LICENSE.md", src=self.folders.source, dst=license_folder)
        cmake = CMake(self)
        cmake.install()
        rmdir(self, self.folders.package / "lib" / "pkgconfig")
        rmdir(self, self.folders.package / "lib" / "cmake")
        # upstream CMakeLists intentionally hardcodes install_name with full
        # install path (to match autootools behavior), instead of @rpath
        fix_apple_shared_install_name(self)

    def package_info(self):
        # FIXME: CMake targets are https://github.com/zlib-ng/zlib-ng/blob/29fd4672a2279a0368be936d7cd44d013d009fae/CMakeLists.txt#L914
        suffix = "" if self.options.zlib_compat else "-ng"
        self.info.set_property("pkg_config_name", f"zlib{suffix}")
        if self._is_windows:
            # The library name of zlib-ng is complicated in zlib-ng>=2.0.4:
            # https://github.com/zlib-ng/zlib-ng/blob/2.0.4/CMakeLists.txt#L994-L1016
            base = "zlib" if is_msvc(self) or self.options.shared else "z"
            static_flag = "static" if is_msvc(self) and not self.options.shared else ""
            build_type = "d" if self.settings.build_type == "Debug" else ""
            self.info.libs = [f"{base}{static_flag}{suffix}{build_type}"]
        else:
            self.info.libs = [f"z{suffix}"]
        if self.options.zlib_compat:
            self.info.defines.append("ZLIB_COMPAT")
            # copied from zlib
            self.info.set_property("cmake_file_name", "ZLIB")
            self.info.set_property("cmake_target_name", "ZLIB::ZLIB")
            self.info.set_property("system_package_version", self._zlib_compat_version)
        self.info.defines.append("WITH_GZFILEOP")
