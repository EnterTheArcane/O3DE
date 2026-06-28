from typing import Literal

from thirdparty import RecipeBase, RecipeOptions
from thirdparty.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.files import apply_patches, copy, get, replace_in_file, rm, rmdir
from thirdparty.microsoft import is_msvc
from thirdparty.scm import Version
from thirdparty.scm.gitlab import GitlabRepository


class _Options(RecipeOptions):
    shared: bool = False
    fPIC: bool = True
    lzma: bool = True
    jpeg: Literal[False, "libjpeg", "libjpeg-turbo", "mozjpeg"] = "libjpeg"
    zlib: bool = True
    libdeflate: bool = False
    zstd: bool = False
    jbig: bool = False
    webp: bool = False
    cxx: bool = True


class Recipe(RecipeBase[_Options]):
    name = "libtiff"
    version = "4.7.1"
    license = "libtiff"

    def latest_version(self):
        repo = GitlabRepository(self, "libtiff/libtiff")
        return Version(repo.latest_release.removeprefix("v"))

    def configure(self):
        if not self.options.cxx:
            self.settings.rm_safe("compiler.cppstd")
            self.settings.rm_safe("compiler.libcxx")

    def requirements(self):
        if self.options.zlib:
            self.requires("zlib")
        if self.options.libdeflate:
            self.requires("libdeflate")
        if self.options.lzma:
            self.requires("xz_utils")
        if self.options.jpeg == "libjpeg":
            self.requires("libjpeg")
        elif self.options.jpeg == "libjpeg-turbo":
            self.requires("libjpeg-turbo")
        elif self.options.jpeg == "mozjpeg":
            self.requires("mozjpeg")
        if self.options.jbig:
            self.requires("jbig")
        if self.options.zstd:
            self.requires("zstd")
        if self.options.webp:
            self.requires("libwebp")

    def source(self):
        get(
            self,
            url="https://download.osgeo.org/libtiff/tiff-4.7.1.tar.xz",
            sha256="b92017489bdc1db3a4c97191aa4b75366673cb746de0dce5d7a749d5954681ba",
            destination=self.folders.source,
            strip_root=True)
        self._patch_sources()

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["lzma"] = self.options.lzma
        tc.variables["jpeg"] = bool(self.options.jpeg)
        tc.variables["jpeg12"] = False
        tc.variables["jbig"] = self.options.jbig
        tc.variables["zlib"] = self.options.zlib
        tc.variables["libdeflate"] = self.options.libdeflate
        tc.variables["zstd"] = self.options.zstd
        tc.variables["webp"] = self.options.webp
        tc.variables["lerc"] = False  # TODO: add lerc support for libtiff versions >= 4.3.0

        # Disable tools, test, contrib, man & html generation
        tc.variables["tiff-tools"] = False
        tc.variables["tiff-tests"] = False
        tc.variables["tiff-contrib"] = False
        tc.variables["tiff-docs"] = False
        cxx_option_name = "tiff-cxx"
        tc.variables[cxx_option_name] = self.options.cxx
        # BUILD_SHARED_LIBS must be set in command line because defined upstream before project()
        tc.cache_variables["BUILD_SHARED_LIBS"] = bool(self.options.shared)
        tc.cache_variables["CMAKE_FIND_PACKAGE_PREFER_CONFIG"] = True
        tc.cache_variables["HAVE_JPEGTURBO_DUAL_MODE_8_12"] = self.options.jpeg == "libjpeg-turbo"
        tc.generate()
        deps = CMakeDeps(self)
        deps.set_property("jbig", "cmake_file_name", "JBIG")
        deps.set_property("jbig", "cmake_target_name", "JBIG::JBIG")
        deps.set_property("xz_utils", "cmake_file_name", "liblzma")
        deps.set_property("xz_utils", "cmake_target_name", "liblzma::liblzma")
        deps.set_property("libdeflate", "cmake_file_name", "Deflate")
        deps.set_property("libdeflate", "cmake_target_name", "Deflate::Deflate")
        deps.set_property("zstd", "cmake_file_name", "ZSTD")
        deps.generate()

    def _patch_sources(self):
        apply_patches(self)

        # remove FindXXXX for recipe dependencies
        for module in ["Deflate", "JBIG", "JPEG", "LERC", "WebP", "ZSTD", "liblzma", "LibLZMA"]:
            rm(self, f"Find{module}.cmake", self.folders.source / "cmake")

        # Export symbols of tiffxx for msvc shared
        replace_in_file(
            self,
            self.folders.source / "libtiff" / "CMakeLists.txt",
            "set_target_properties(tiffxx PROPERTIES SOVERSION ${SO_COMPATVERSION})",
            "set_target_properties(tiffxx PROPERTIES SOVERSION ${SO_COMPATVERSION} WINDOWS_EXPORT_ALL_SYMBOLS ON)")

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE.md", src=self.folders.source, dst=self.folders.package / "licenses", ignore_case=True, keep_path=False)
        cmake = CMake(self)
        cmake.install()
        rmdir(self, self.folders.package / "lib" / "cmake")
        rmdir(self, self.folders.package / "lib" / "pkgconfig")

    def package_info(self):
        self.info.set_property("cmake_file_name", "TIFF")
        self.info.set_property("cmake_target_name", "TIFF::TIFF")
        self.info.set_property("pkg_config_name", f"libtiff-{Version(self.version).major}")
        suffix = "d" if is_msvc(self) and self.settings.build_type == "Debug" else ""
        if self.options.cxx:
            self.info.libs.append(f"tiffxx{suffix}")
        self.info.libs.append(f"tiff{suffix}")
        if self.settings.os in ["Linux", "Android", "FreeBSD", "SunOS", "AIX"]:
            self.info.system_libs.append("m")

        requires: list[str] = []
        if self.options.zlib:
            requires.append("zlib::zlib")
        if self.options.libdeflate:
            requires.append("libdeflate::libdeflate")
        if self.options.lzma:
            requires.append("xz_utils::xz_utils")
        if self.options.jpeg == "libjpeg":
            requires.append("libjpeg::libjpeg")
        elif self.options.jpeg == "libjpeg-turbo":
            requires.append("libjpeg-turbo::jpeg")
        elif self.options.jpeg == "mozjpeg":
            requires.append("mozjpeg::libjpeg")
        if self.options.jbig:
            requires.append("jbig::jbig")
        if self.options.zstd:
            requires.append("zstd::zstd")
        if self.options.webp:
            requires.append("libwebp::webp")
        self.info.requires = requires
