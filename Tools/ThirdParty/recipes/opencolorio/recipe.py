# Ported from conan-center-index/opencolorio by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.microsoft import is_msvc
from thirdparty.tools.apple import is_apple_os
from thirdparty.tools.files import apply_patches, get, copy, rm, rmdir
from thirdparty.tools.scm import Version
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
import os

class Recipe(RecipeBase):
    name = "opencolorio"
    license = "BSD-3-Clause"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "use_sse": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "use_sse": True,
    }

    def requirements(self) -> list[str]:
        return ["expat", "openexr", "imath", "pystring", "yaml-cpp", "minizip-ng", "lcms"]

    def source(self):
        get(url=self.thirdparty_data["versions"][self.version]["url"], dest=self.source_folder, sha256=self.thirdparty_data["versions"][self.version]["sha256"])

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["CMAKE_VERBOSE_MAKEFILE"] = True
        tc.variables["OCIO_BUILD_PYTHON"] = False

        tc.variables["OCIO_USE_SSE"] = self.options.get("use_sse", False)

        # openexr 2.x provides Half library
        tc.variables["OCIO_USE_OPENEXR_HALF"] = True

        tc.variables["OCIO_BUILD_APPS"] = True
        tc.variables["OCIO_BUILD_DOCS"] = False
        tc.variables["OCIO_BUILD_TESTS"] = False
        tc.variables["OCIO_BUILD_GPU_TESTS"] = False
        tc.variables["OCIO_USE_BOOST_PTR"] = False

        # avoid downloading dependencies
        tc.variables["OCIO_INSTALL_EXT_PACKAGE"] = "NONE"

        if self.is_windows and not self.options.shared:
            # define any value because ifndef is used
            tc.variables["OpenColorIO_SKIP_IMPORTS"] = True

        tc.cache_variables["CMAKE_POLICY_DEFAULT_CMP0077"] = "NEW"
        tc.cache_variables["CMAKE_POLICY_DEFAULT_CMP0091"] = "NEW"

        if self.is_linux:
            # Workaround for: https://github.com/conan-io/conan/issues/13560
            libdirs_host = [l for dependency in self.dependencies.host.values() for l in dependency.cpp_info.aggregated_components().libdirs]
            tc.variables["CMAKE_BUILD_RPATH"] = ";".join(libdirs_host)

        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()

    def _patch_sources(self):
        apply_patches(self)

        for module in ("expat", "lcms2", "pystring", "yaml-cpp", "Imath", "minizip-ng"):
            rm("Find"+module+".cmake", os.path.join(self.source_folder, "share", "cmake", "modules"))

    def build(self):
        self._patch_sources()

        cm = CMake(self)
        cm.configure()
        cm.build()

    def package(self):
        cm = CMake(self)
        cm.install()

        if not self.options.shared:
            copy("*",
                src=os.path.join(self.package_folder, "lib", "static"),
                dst=os.path.join(self.package_folder, "lib"))
            rmdir(os.path.join(self.package_folder, "lib", "static"))

        rmdir(os.path.join(self.package_folder, "cmake"))
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(os.path.join(self.package_folder, "share"))
        # nop for 2.x
        rm("OpenColorIOConfig*.cmake", self.package_folder)
        rm("*.pdb", os.path.join(self.package_folder, "bin"))
        copy(pattern="LICENSE", dst=os.path.join(self.package_folder, "licenses"), src=self.source_folder)
