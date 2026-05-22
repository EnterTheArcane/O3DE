import os

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.tools.files import apply_patches, copy, get, replace_in_file, rmdir
from thirdparty.tools.scm import Version


class Recipe(RecipeBase):
    name = "openexr"
    version = "3.2.9"
    license = "BSD-3-Clause"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    def requirements(self) -> list[str]:
        return ["zlib", "imath", "libdeflate"]

    def source(self):
        get(
            url="https://github.com/AcademySoftwareFoundation/openexr/releases/download/v3.2.9/openexr-3.2.9.tar.gz",
            dest=self.source_folder,
            sha256="926fe5fba3ebeceaf4e07b1940f330213a4d790092a42acff28f636911536c3d",
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["OPENEXR_INSTALL_EXAMPLES"] = False
        tc.variables["BUILD_TESTING"] = False
        tc.variables["BUILD_WEBSITE"] = False
        tc.variables["DOCS"] = False
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def _patch_sources(self):
        apply_patches(self)
        # Suppress website example targets that appear in 3.2
        cml = os.path.join(self.source_folder, "CMakeLists.txt")
        replace_in_file(
            cml, "add_subdirectory(website/src)", "#  add_subdirectory(website/src)"
        )

    def build(self):
        self._patch_sources()
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
        rmdir(os.path.join(self.package_folder, "share"))
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "OpenEXR")
        self.cpp_info.set_property("cmake_target_name", "OpenEXR::OpenEXR")
        self.cpp_info.set_property("cmake_package_file", "lib/cmake/OpenEXR/OpenEXRConfig.cmake")
