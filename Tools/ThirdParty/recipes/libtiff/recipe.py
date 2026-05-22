# Ported from conan-center-index/libtiff by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.tools.files import apply_patches, copy, get, replace_in_file, rm, rmdir
from thirdparty.tools.microsoft import is_msvc
from thirdparty.tools.scm import Version
import os

class Recipe(RecipeBase):
    name = "libtiff"
    license = "libtiff"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "lzma": [True, False],
        "jpeg": [False, "libjpeg", "libjpeg-turbo", "mozjpeg"],
        "zlib": [True, False],
        "libdeflate": [True, False],
        "zstd": [True, False],
        "jbig": [True, False],
        "webp": [True, False],
        "cxx":  [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "lzma": True,
        "jpeg": "libjpeg-turbo",
        "zlib": True,
        "libdeflate": True,
        "zstd": True,
        "jbig": False,
        "webp": True,
        "cxx":  True,
    }

    def requirements(self) -> list[str]:
        return ["zlib", "libdeflate", "xz_utils", "libjpeg-turbo", "zstd", "libwebp"]

    def source(self):
        get(url=self.thirdparty_data["versions"][self.version]["url"], dest=self.source_folder, sha256=self.thirdparty_data["versions"][self.version]["sha256"])
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
        tc.variables["lerc"] = False # TODO: add lerc support for libtiff versions >= 4.3.0

        # Disable tools, test, contrib, man & html generation
        tc.variables["tiff-tools"] = False
        tc.variables["tiff-tests"] = False
        tc.variables["tiff-contrib"] = False
        tc.variables["tiff-docs"] = False
        cxx_option_name = "cxx" if Version(self.version) < "4.7.1" else "tiff-cxx"
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

        # remove FindXXXX for conan dependencies
        for module in ["Deflate", "JBIG", "JPEG", "LERC", "WebP", "ZSTD", "liblzma", "LibLZMA"]:
            rm(f"Find{module}.cmake", os.path.join(self.source_folder, "cmake"))

        # Export symbols of tiffxx for msvc shared
        replace_in_file(os.path.join(self.source_folder, "libtiff", "CMakeLists.txt"),
                              "set_target_properties(tiffxx PROPERTIES SOVERSION ${SO_COMPATVERSION})",
                              "set_target_properties(tiffxx PROPERTIES SOVERSION ${SO_COMPATVERSION} WINDOWS_EXPORT_ALL_SYMBOLS ON)")

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy("LICENSE.md", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"), keep_path=False)
        cmake = CMake(self)
        cmake.install()
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
