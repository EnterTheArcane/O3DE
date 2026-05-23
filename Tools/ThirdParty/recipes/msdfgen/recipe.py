from thirdparty import RecipeBase
from thirdparty.tools.build import check_min_cppstd
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.tools.files import apply_conandata_patches, copy, get, replace_in_file, rm, rmdir
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
        "utility": True,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def requirements(self):
        self.requires("freetype/2.13.2")
        if  Version(self.version) < "1.10":
            self.requires("lodepng/20200615")
        else:
            self.requires("libpng/[>=1.6 <2]")
        self.requires("tinyxml2/10.0.0")

    def source(self):
        get(self, url="https://github.com/Chlumsky/msdfgen/archive/refs/tags/v1.12.tar.gz", sha256="f058117496097217d12e4ea86adbff8467adaf6f12af793925d243b86b0c4f57", destination=self.source_folder, strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
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
            tc.cache_variables["MSDFGEN_DYNAMIC_RUNTIME"] = not is_msvc_static_runtime(self)
        if self.settings.os == "Linux":
            # Workaround for https://github.com/conan-io/conan/issues/13560
            libdirs_host = [l for dependency in self.dependencies.host.values() for l in dependency.cpp_info.aggregated_components().libdirs]
            tc.variables["CMAKE_BUILD_RPATH"] = ";".join(libdirs_host)
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def _patch_sources(self):
        apply_conandata_patches(self)

        if Version(self.version) < "1.10":
            # remove bundled lodepng & tinyxml2
            rmdir(self, os.path.join(self.source_folder, "lib"))
            rmdir(self, os.path.join(self.source_folder, "include"))

            # very weird but required for Visual Studio when libs are unvendored (at least for Ninja generator)
            if is_msvc(self):
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
        copy(self, "LICENSE.txt", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        rm(self, "*.pdb", os.path.join(self.package_folder, "bin"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "msdfgen")
        # Required to avoid some side effect in CMakeDeps generator of downstream recipes
        self.cpp_info.set_property("cmake_target_name", "msdfgen::msdgen-all-unofficial")

        self.cpp_info.names["cmake_find_package"] = "msdfgen"
        self.cpp_info.names["cmake_find_package_multi"] = "msdfgen"

        includedir = os.path.join("include", "msdfgen")

        self.cpp_info.components["_msdfgen"].set_property("cmake_target_name", "msdfgen::msdfgen")
        self.cpp_info.components["_msdfgen"].names["cmake_find_package"] = "msdfgen"
        self.cpp_info.components["_msdfgen"].names["cmake_find_package_multi"] = "msdfgen"
        self.cpp_info.components["_msdfgen"].includedirs.append(includedir)
        self.cpp_info.components["_msdfgen"].libs = ["msdfgen" if Version(self.version) < "1.10" else "msdfgen-core"]
        self.cpp_info.components["_msdfgen"].defines = ["MSDFGEN_USE_CPP11"]
        if Version(self.version) >= "1.10":
            if self.options.shared and is_msvc(self):
                self.cpp_info.components["_msdfgen"].defines.append("MSDFGEN_PUBLIC=__declspec(dllimport)")
            else:
                self.cpp_info.components["_msdfgen"].defines.append("MSDFGEN_PUBLIC=")

        self.cpp_info.components["msdfgen-ext"].set_property("cmake_target_name", "msdfgen::msdfgen-ext")
        self.cpp_info.components["msdfgen-ext"].names["cmake_find_package"] = "msdfgen-ext"
        self.cpp_info.components["msdfgen-ext"].names["cmake_find_package_multi"] = "msdfgen-ext"
        self.cpp_info.components["msdfgen-ext"].includedirs.append(includedir)
        self.cpp_info.components["msdfgen-ext"].libs = ["msdfgen-ext"]
        self.cpp_info.components["msdfgen-ext"].requires = [
            "_msdfgen", "freetype::freetype",
            "lodepng::lodepng" if Version(self.version) < "1.10" else "libpng::libpng",
            "tinyxml2::tinyxml2",
        ]

        if self.options.with_skia:
            self.cpp_info.components["msdfgen-ext"].defines.append("MSDFGEN_USE_SKIA")

        # TODO: to remove once conan v1 support dropped
        if self.options.utility:
            self.env_info.PATH.append(os.path.join(self.package_folder, "bin"))
