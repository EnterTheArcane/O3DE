import os

from thirdparty import RecipeBase
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import copy, get, replace_in_file, rmdir
from thirdparty.microsoft import is_msvc


class Recipe(RecipeBase):
    name = "cpuinfo"
    version = "20251210"
    license = "BSD-2-Clause"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    def configure(self):
        if is_msvc(self):
            # Only static for msvc
            # Injecting CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS is not sufficient since there are global symbols
            del self.options.shared
        if self.options.get_safe("shared"):
            self.options.rm_safe("fPIC")
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def source(self):
        get(
            self,
            url="https://github.com/pytorch/cpuinfo/archive/ff24ffee8340fbd9001cce6a9ef41cdd16aa2bd3.tar.gz",
            sha256="59a0a35488762568c7b7575352d726cb11fee361455e451ad820bdf5a01b856e",
            destination=self.folders.source,
            strip_root=True)
        replace_in_file(
            self,
            os.path.join(self.folders.source, "CMakeLists.txt"),
            "LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}",
            "LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}")

    def generate(self):
        tc = CMakeToolchain(self)
        # cpuinfo
        tc.cache_variables["CPUINFO_LIBRARY_TYPE"] = "default"
        tc.cache_variables["CPUINFO_RUNTIME_TYPE"] = "default"
        tc.cache_variables["CPUINFO_LOG_LEVEL"] = "default"
        tc.variables["CPUINFO_BUILD_TOOLS"] = False
        tc.variables["CPUINFO_BUILD_UNIT_TESTS"] = False
        tc.variables["CPUINFO_BUILD_MOCK_TESTS"] = False
        tc.variables["CPUINFO_BUILD_BENCHMARKS"] = False
        # clog (always static)
        tc.cache_variables["CLOG_RUNTIME_TYPE"] = "default"
        tc.variables["CLOG_BUILD_TESTS"] = False
        tc.variables["CMAKE_POSITION_INDEPENDENT_CODE"] = self.options.get_safe("fPIC", True)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.folders.source, dst=os.path.join(self.folders.package, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.folders.package, "lib", "pkgconfig"))
        rmdir(self, os.path.join(self.folders.package, "share"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "cpuinfo")
        self.cpp_info.set_property("pkg_config_name", "libcpuinfo")

        self.cpp_info.components["cpuinfo"].set_property("cmake_target_name", "cpuinfo::cpuinfo")
        self.cpp_info.components["cpuinfo"].libs = ["cpuinfo"]
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.components["cpuinfo"].system_libs.append("pthread")

        if self.settings.os == "Android":
            self.cpp_info.components["cpuinfo"].system_libs.append("log")
