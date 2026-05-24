import os

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.tools.files import copy, get, replace_in_file, rm, rmdir
from thirdparty.tools.scm import Version
from thirdparty.tools.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "hdf5"
    version = "1.14.6"
    license = "BSD-3-Clause"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "enable_cxx": [True, False],
        "hl": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "enable_cxx": True,
        "hl": True,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        if not self.options.enable_cxx:
            self.settings.rm_safe("compiler.cppstd")
            self.settings.rm_safe("compiler.libcxx")

    def requirements(self):
        self.requires("zlib/[>=1.2.11 <2]")

    def build_requirements(self):
        self.tool_requires("cmake")

    def latest_version(self):
        repo = GithubRepository(self, "HDFGroup/hdf5")
        return Version(repo.latest_release)

    def source(self):
        get(
            self,
            url="https://github.com/HDFGroup/hdf5/archive/refs/tags/hdf5_1.14.6.tar.gz",
            sha256="09ee1c671a87401a5201c06106650f62badeea5a3b3941e9b1e2e1e08317357f",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()

        tc = CMakeToolchain(self)
        tc.variables["HDF5_EXTERNALLY_CONFIGURED"] = True
        tc.variables["HDF5_EXTERNAL_LIB_PREFIX"] = ""
        tc.variables["HDF5_USE_FOLDERS"] = False
        tc.variables["HDF5_NO_PACKAGES"] = True
        tc.variables["ONLY_SHARED_LIBS"] = self.options.shared
        tc.variables["BUILD_STATIC_LIBS"] = not self.options.shared
        tc.variables["BUILD_STATIC_EXECS"] = False
        tc.variables["HDF5_ENABLE_COVERAGE"] = False
        tc.variables["HDF5_ENABLE_USING_MEMCHECKER"] = False
        tc.variables["HDF5_MEMORY_ALLOC_SANITY_CHECK"] = False
        tc.variables["HDF5_ENABLE_PREADWRITE"] = True
        tc.variables["HDF5_ENABLE_DEPRECATED_SYMBOLS"] = True
        tc.variables["HDF5_BUILD_GENERATORS"] = False
        tc.variables["HDF5_ENABLE_TRACE"] = False
        tc.variables["HDF5_ENABLE_PARALLEL"] = False
        tc.variables["HDF5_ENABLE_Z_LIB_SUPPORT"] = True
        tc.variables["HDF5_ENABLE_SZIP_SUPPORT"] = False
        tc.variables["HDF5_ENABLE_SZIP_ENCODING"] = False
        tc.variables["HDF5_USE_ZLIB_NG"] = False
        tc.variables["HDF5_PACKAGE_EXTLIBS"] = False
        tc.variables["HDF5_ENABLE_THREADSAFE"] = False
        tc.variables["HDF5_ENABLE_DEBUG_APIS"] = False
        tc.variables["BUILD_TESTING"] = False
        tc.variables["HDF5_INSTALL_INCLUDE_DIR"] = "include/hdf5"
        tc.variables["HDF5_BUILD_TOOLS"] = False
        tc.variables["HDF5_BUILD_EXAMPLES"] = False
        tc.variables["HDF5_BUILD_HL_LIB"] = self.options.hl
        tc.variables["HDF5_BUILD_FORTRAN"] = False
        tc.variables["HDF5_BUILD_CPP_LIB"] = self.options.enable_cxx
        tc.variables["HDF5_BUILD_JAVA"] = False
        tc.variables["ALLOW_UNSUPPORTED"] = False
        tc.generate()

    def build(self):
        replace_in_file(
            self,
            os.path.join(self.source_folder, "CMakeLists.txt"),
            "set (CMAKE_POSITION_INDEPENDENT_CODE ON)",
            "")
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "COPYING", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
        rm(self, "libhdf5.settings", os.path.join(self.package_folder, "lib"))
        rm(self, "*.pdb", os.path.join(self.package_folder, "bin"))
        if self.options.shared:
            for root, _, files in os.walk(os.path.join(self.package_folder, "lib")):
                for f in files:
                    if f.endswith(".a") and not f.endswith(".dll.a"):
                        os.remove(os.path.join(root, f))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "HDF5")
        self.cpp_info.set_property("cmake_target_name", "HDF5::HDF5")

        def _lib_name(lib):
            if self.settings.os == "Windows" and self.settings.compiler != "gcc" and not self.options.shared:
                lib = "lib" + lib
            if self.settings.build_type == "Debug":
                debug_postfix = "_D" if self.settings.os == "Windows" else "_debug"
                return lib + debug_postfix
            return lib

        self.cpp_info.components["hdf5_c"].set_property("cmake_target_name", "HDF5::C")
        self.cpp_info.components["hdf5_c"].libs = [_lib_name("hdf5")]
        self.cpp_info.components["hdf5_c"].requires = ["zlib::zlib"]
        self.cpp_info.components["hdf5_c"].includedirs = ["include", os.path.join("include", "hdf5")]
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.components["hdf5_c"].system_libs.extend(["dl", "m"])
        elif self.settings.os == "Windows":
            self.cpp_info.components["hdf5_c"].system_libs.append("Shlwapi")
        if self.options.shared:
            self.cpp_info.components["hdf5_c"].defines.append("H5_BUILT_AS_DYNAMIC_LIB")

        if self.options.enable_cxx:
            self.cpp_info.components["hdf5_cpp"].set_property("cmake_target_name", "HDF5::CXX")
            self.cpp_info.components["hdf5_cpp"].libs = [_lib_name("hdf5_cpp")]
            self.cpp_info.components["hdf5_cpp"].requires = ["hdf5_c"]
            self.cpp_info.components["hdf5_cpp"].includedirs = ["include", os.path.join("include", "hdf5")]

        if self.options.hl:
            self.cpp_info.components["hdf5_hl"].set_property("cmake_target_name", "HDF5::HL")
            self.cpp_info.components["hdf5_hl"].libs = [_lib_name("hdf5_hl")]
            self.cpp_info.components["hdf5_hl"].requires = ["hdf5_c"]
            self.cpp_info.components["hdf5_hl"].includedirs = ["include", os.path.join("include", "hdf5")]
            if self.options.enable_cxx:
                self.cpp_info.components["hdf5_hl_cpp"].set_property("cmake_target_name", "HDF5::HL_CXX")
                self.cpp_info.components["hdf5_hl_cpp"].libs = [_lib_name("hdf5_hl_cpp")]
                self.cpp_info.components["hdf5_hl_cpp"].requires = ["hdf5_c", "hdf5_cpp", "hdf5_hl"]
                self.cpp_info.components["hdf5_hl_cpp"].includedirs = ["include", os.path.join("include", "hdf5")]
