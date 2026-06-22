import os

from thirdparty import RecipeBase
from thirdparty.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.files import apply_patches, copy, get, rm, rmdir
from thirdparty.microsoft import is_msvc, is_msvc_static_runtime
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "msdfgen"
    version = "1.13"
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

    def requirements(self):
        self.requires("freetype")
        self.requires("libpng")
        self.requires("tinyxml2")

    def latest_version(self):
        repo = GithubRepository(self, "Chlumsky/msdfgen")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/Chlumsky/msdfgen/archive/refs/tags/v1.13.tar.gz",
            sha256="93cd1ad8918c1a78c5c96e82d4f4c77f0eb86c2e7e8579a0967e54196c4b7167",
            destination=self.source_folder,
            strip_root=True)
        apply_patches(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["MSDFGEN_BUILD_MSDFGEN_STANDALONE"] = self.options.utility
        tc.cache_variables["MSDFGEN_USE_OPENMP"] = self.options.with_openmp
        tc.cache_variables["MSDFGEN_USE_CPP11"] = True
        tc.cache_variables["MSDFGEN_USE_SKIA"] = self.options.with_skia
        tc.cache_variables["MSDFGEN_INSTALL"] = True
        tc.cache_variables["MSDFGEN_USE_VCPKG"] = False
        # Because in upstream CMakeLists, project() is called after some logic based on BUILD_SHARED_LIBS
        tc.cache_variables["BUILD_SHARED_LIBS"] = self.options.shared
        tc.cache_variables["MSDFGEN_DYNAMIC_RUNTIME"] = not is_msvc_static_runtime(self)
        if self.settings.os == "Linux":
            # Workaround for upstream issue 13560
            libdirs_host = [l for dependency in self.dependencies.host.values() for l in dependency.cpp_info.aggregated_components().libdirs]
            tc.variables["CMAKE_BUILD_RPATH"] = ";".join(libdirs_host)
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
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

        includedir = os.path.join("include", "msdfgen")

        self.cpp_info.components["_msdfgen"].set_property("cmake_target_name", "msdfgen::msdfgen")
        self.cpp_info.components["_msdfgen"].includedirs.append(includedir)
        self.cpp_info.components["_msdfgen"].libs = ["msdfgen-core"]
        self.cpp_info.components["_msdfgen"].defines = ["MSDFGEN_USE_CPP11"]
        if self.options.shared and is_msvc(self):
            self.cpp_info.components["_msdfgen"].defines.append("MSDFGEN_PUBLIC=__declspec(dllimport)")
        else:
            self.cpp_info.components["_msdfgen"].defines.append("MSDFGEN_PUBLIC=")

        self.cpp_info.components["msdfgen-ext"].set_property("cmake_target_name", "msdfgen::msdfgen-ext")
        self.cpp_info.components["msdfgen-ext"].includedirs.append(includedir)
        self.cpp_info.components["msdfgen-ext"].libs = ["msdfgen-ext"]
        self.cpp_info.components["msdfgen-ext"].requires = [
            "_msdfgen", "freetype::freetype",
            "libpng::libpng",
            "tinyxml2::tinyxml2",
        ]

        if self.options.with_skia:
            self.cpp_info.components["msdfgen-ext"].defines.append("MSDFGEN_USE_SKIA")
