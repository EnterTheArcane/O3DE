import os

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.tools.files import apply_patches, copy, get, rm
from thirdparty.tools.microsoft import is_msvc
from thirdparty.tools.scm import Version
from thirdparty.tools.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "openvdb"
    version = "13.0.0"
    license = "Apache-2.0"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def requirements(self):
        self.requires("onetbb")
        self.requires("imath")
        self.requires("zlib")

    def build_requirements(self):
        self.tool_requires("cmake")

    def latest_version(self):
        repo = GithubRepository(self, "AcademySoftwareFoundation/openvdb")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/AcademySoftwareFoundation/openvdb/archive/refs/tags/v13.0.0.tar.gz",
            sha256="4d6a91df5f347017496fe8d22c3dbb7c4b5d7289499d4eb4d53dd2c75bb454e1",
            destination=self.source_folder,
            strip_root=True)
        apply_patches(self)
        rm(self, "Find*.cmake", os.path.join(self.source_folder, "cmake"), recursive=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["OPENVDB_BUILD_AX"] = False
        tc.variables["OPENVDB_BUILD_BINARIES"] = False
        tc.variables["OPENVDB_BUILD_CORE"] = True
        tc.variables["OPENVDB_BUILD_DOCS"] = False
        tc.variables["OPENVDB_BUILD_HOUDINI_ABITESTS"] = False
        tc.variables["OPENVDB_BUILD_HOUDINI_PLUGIN"] = False
        tc.variables["OPENVDB_BUILD_MAYA_PLUGIN"] = False
        tc.variables["OPENVDB_BUILD_NANOVDB"] = False
        tc.variables["OPENVDB_BUILD_PYTHON_MODULE"] = False
        tc.variables["OPENVDB_CORE_SHARED"] = self.options.shared
        tc.variables["OPENVDB_CORE_STATIC"] = not self.options.shared
        tc.variables["OPENVDB_CXX_STRICT"] = False
        tc.variables["OPENVDB_ENABLE_RPATH"] = True
        tc.variables["OPENVDB_ENABLE_UNINSTALL"] = False
        tc.variables["OPENVDB_INSTALL_CMAKE_MODULES"] = False
        tc.variables["OPENVDB_SIMD"] = "None"
        tc.variables["OPENVDB_USE_DELAYED_LOADING"] = False
        tc.variables["USE_AX"] = False
        tc.variables["USE_BLOSC"] = False
        tc.variables["USE_COLORED_OUTPUT"] = False
        tc.variables["USE_EXPLICIT_INSTANTIATION"] = False
        tc.variables["USE_EXR"] = False
        tc.variables["USE_HOUDINI"] = False
        tc.variables["USE_IMATH_HALF"] = True
        tc.variables["USE_LOG4CPLUS"] = False
        tc.variables["USE_MAYA"] = False
        tc.variables["USE_NANOVDB"] = False
        tc.variables["USE_PKGCONFIG"] = False
        tc.variables["USE_PNG"] = False
        tc.variables["USE_STATIC_DEPENDENCIES"] = False
        tc.variables["USE_TBB"] = True
        tc.variables["USE_ZLIB"] = True
        tc.generate()

        deps = CMakeDeps(self)
        deps.set_property("onetbb", "cmake_file_name", "TBB")
        deps.set_property("onetbb", "cmake_target_name", "TBB::tbb")
        deps.set_property("imath", "cmake_file_name", "Imath")
        deps.set_property("imath", "cmake_target_name", "Imath::Imath")
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.set_property("cmake_find_mode", "both")
        self.cpp_info.set_property("cmake_file_name", "OpenVDB")
        self.cpp_info.set_property("cmake_target_name", "OpenVDB::openvdb")

        comp = self.cpp_info.components["openvdb-core"]
        lib_prefix = "lib" if is_msvc(self) and not self.options.shared else ""
        comp.libs = [lib_prefix + "openvdb"]
        if self.options.shared:
            comp.defines = ["OPENVDB_DLL"]
        else:
            comp.defines = ["OPENVDB_STATICLIB"]
        if self.settings.os == "Windows":
            comp.defines += ["_WIN32", "NOMINMAX"]
        if self.settings.os in ("Linux", "FreeBSD"):
            comp.system_libs = ["pthread"]
        comp.requires = ["onetbb::onetbb", "imath::imath", "zlib::zlib"]
