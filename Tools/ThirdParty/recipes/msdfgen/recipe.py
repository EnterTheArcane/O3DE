from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.tools.files import apply_patches, copy, get, replace_in_file, rm, rmdir
from thirdparty.tools.microsoft import is_msvc, is_msvc_static_runtime
from thirdparty.tools.scm import Version
import os


class Recipe(RecipeBase):
    name = "msdfgen"
    version = "1.12"
    license = "MIT"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_openmp": [True, False],
        "with_skia": [True, False],
        "utility": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_openmp": False,
        "with_skia": False,
        "utility": False,
    }

    def requirements(self) -> list[str]:
        return [
            "freetype",
            "libpng",
            "tinyxml2",
            "zlib",
        ]  # lodepng is bundled; zlib needed by libpng cmake

    def source(self):
        get(
            url="https://github.com/Chlumsky/msdfgen/archive/refs/tags/v1.12.tar.gz",
            dest=self.source_folder,
            sha256="f058117496097217d12e4ea86adbff8467adaf6f12af793925d243b86b0c4f57",
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["MSDFGEN_BUILD_STANDALONE"] = self.options.utility
        tc.cache_variables["MSDFGEN_BUILD_MSDFGEN_STANDALONE"] = self.options.utility
        tc.cache_variables["MSDFGEN_USE_OPENMP"] = self.options.with_openmp
        tc.cache_variables["MSDFGEN_USE_CPP11"] = True
        tc.cache_variables["MSDFGEN_USE_SKIA"] = self.options.with_skia
        tc.cache_variables["MSDFGEN_INSTALL"] = True
        if Version(self.version) >= "1.10":
            tc.cache_variables["MSDFGEN_USE_VCPKG"] = False
            # Because in upstream CMakeLists, project() is called after some logic based on BUILD_SHARED_LIBS
            tc.cache_variables["BUILD_SHARED_LIBS"] = self.options.shared
        if Version(self.version) >= "1.11":
            tc.cache_variables["MSDFGEN_DYNAMIC_RUNTIME"] = not False
        if self.is_linux:
            # Workaround for https://github.com/conan-io/conan/issues/13560
            libdirs_host = [
                l
                for dependency in self.dependencies.host.values()
                for l in dependency.cpp_info.aggregated_components().libdirs
            ]
            tc.variables["CMAKE_BUILD_RPATH"] = ";".join(libdirs_host)
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def _patch_sources(self):
        apply_patches(self)

        if Version(self.version) < "1.10":
            # remove bundled lodepng & tinyxml2
            rmdir(os.path.join(self.source_folder, "lib"))
            rmdir(os.path.join(self.source_folder, "include"))

            # very weird but required for Visual Studio when libs are unvendored (at least for Ninja generator)
            if self.is_windows:
                replace_in_file(
                    self,
                    os.path.join(self.source_folder, "CMakeLists.txt"),
                    "set_target_properties(msdfgen-standalone PROPERTIES ARCHIVE_OUTPUT_DIRECTORY archive OUTPUT_NAME msdfgen)",
                    "set_target_properties(msdfgen-standalone PROPERTIES OUTPUT_NAME msdfgen IMPORT_PREFIX foo)",
                )

    def build(self):
        self._patch_sources()
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(
            "LICENSE.txt",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        cmake = CMake(self)
        cmake.install()
        rm("*.pdb", os.path.join(self.package_folder, "bin"))
