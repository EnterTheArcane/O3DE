# Simplified openimageio recipe — uses external fmt/pugixml/tsl-robin-map recipes
import os

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.tools.files import apply_patches, copy, get, replace_in_file, rm, rmdir


class Recipe(RecipeBase):
    name = "openimageio"
    version = "3.1.13.1"
    license = "Apache-2.0"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    def requirements(self) -> list[str]:
        return [
            "zlib",
            "libtiff",
            "imath",
            "openexr",
            "libjpeg-turbo",
            "libpng",
            "libwebp",
            "opencolorio",
            "libdeflate",
            "xz_utils",
            "zstd",
            "fmt",
            "pugixml",
            "tsl-robin-map",
        ]

    def source(self):
        get(
            url="https://github.com/AcademySoftwareFoundation/OpenImageIO/releases/download/v3.1.13.1/OpenImageIO-3.1.13.1.tar.gz",
            dest=self.source_folder,
            sha256="b0d81f4f041fd72f034bd2a6c5ad9db1880008f67af101233cf3992f9e59217f",
        )
        apply_patches(self)
        # Guard the unconditional get_target_property call for OpenColorIO;
        # when USE_OPENCOLORIO=False the target doesn't exist but cmake still tries
        # to query it, causing a fatal error.
        replace_in_file(
            os.path.join(self.source_folder, "src", "cmake", "externalpackages.cmake"),
            "if (NOT OPENCOLORIO_INCLUDES)\n    get_target_property(OPENCOLORIO_INCLUDES OpenColorIO::OpenColorIO INTERFACE_INCLUDE_DIRECTORIES)\nendif ()",
            "if (NOT OPENCOLORIO_INCLUDES AND TARGET OpenColorIO::OpenColorIO)\n    get_target_property(OPENCOLORIO_INCLUDES OpenColorIO::OpenColorIO INTERFACE_INCLUDE_DIRECTORIES)\nendif ()",
        )
        # Guard unconditional OpenColorIO::OpenColorIO target use in libOpenImageIO
        replace_in_file(
            os.path.join(self.source_folder, "src", "libOpenImageIO", "CMakeLists.txt"),
            "            OpenColorIO::OpenColorIO\n            $<TARGET_NAME_IF_EXISTS:OpenColorIO::OpenColorIOHeaders>",
            "            $<TARGET_NAME_IF_EXISTS:OpenColorIO::OpenColorIO>\n            $<TARGET_NAME_IF_EXISTS:OpenColorIO::OpenColorIOHeaders>",
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["CMAKE_DEBUG_POSTFIX"] = ""
        tc.variables["OIIO_BUILD_TOOLS"] = True
        # Set IMATH_INCLUDES and OPENEXR_INCLUDES explicitly: these are expected
        # by externalpackages.cmake's include_directories(BEFORE ...) call but
        # are normally set by the FindOpenEXR module which we bypass (config mode).
        imath_dep = self.dependencies.get("imath")
        if imath_dep:
            tc.cache_variables["IMATH_INCLUDES"] = os.path.join(
                imath_dep.package_folder, "include"
            ).replace("\\", "/")
        openexr_dep = self.dependencies.get("openexr")
        if openexr_dep:
            tc.cache_variables["OPENEXR_INCLUDES"] = os.path.join(
                openexr_dep.package_folder, "include"
            ).replace("\\", "/")
        tc.variables["OIIO_BUILD_TESTS"] = False
        tc.variables["BUILD_DOCS"] = False
        tc.variables["INSTALL_DOCS"] = False
        tc.variables["INSTALL_FONTS"] = False
        tc.variables["INSTALL_CMAKE_HELPER"] = False
        tc.variables["EMBEDPLUGINS"] = True
        tc.variables["USE_PYTHON"] = False
        # Use external recipes for fmt, pugixml, and tsl-robin-map
        tc.variables["USE_EXTERNAL_PUGIXML"] = True
        tc.cache_variables["OIIO_INTERNALIZE_FMT"] = False
        tc.cache_variables["BUILD_MISSING_ROBINMAP"] = False
        tc.variables["BUILD_TESTING"] = False
        # Enable supported image formats
        tc.variables["USE_JPEGTURBO"] = True
        tc.variables["USE_JPEG"] = True
        tc.variables["USE_LIBPNG"] = True
        tc.variables["USE_LIBWEBP"] = True
        tc.variables["USE_OPENCOLORIO"] = True
        tc.variables["USE_OPENCV"] = False
        tc.variables["USE_TBB"] = False
        tc.variables["USE_DCMTK"] = False
        tc.variables["USE_FIELD3D"] = False
        tc.variables["USE_GIF"] = False
        tc.variables["USE_LIBHEIF"] = False
        tc.variables["USE_LIBRAW"] = False
        tc.variables["USE_OPENVDB"] = False
        tc.variables["USE_PTEX"] = False
        tc.variables["USE_R3DSDK"] = False
        tc.variables["USE_NUKE"] = False
        tc.variables["USE_OPENGL"] = False
        tc.variables["USE_QT"] = False
        tc.variables["USE_FREETYPE"] = False
        tc.variables["USE_OPENJPEG"] = False
        tc.cache_variables["USE_OPENJPH"] = False
        tc.cache_variables["USE_JXL"] = False
        tc.cache_variables["USE_FFMPEG"] = False
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(
            "LICENSE.md",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        cmake = CMake(self)
        cmake.install()
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(os.path.join(self.package_folder, "share"))
