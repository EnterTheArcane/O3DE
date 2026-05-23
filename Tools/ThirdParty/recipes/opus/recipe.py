from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import apply_conandata_patches, copy, get, rmdir
from thirdparty.tools.microsoft import check_min_vs, is_msvc, is_msvc_static_runtime
from thirdparty.tools.scm import Version
import os

class Recipe(RecipeBase):
    name = "opus"
    version = "1.5.2"
    license = "BSD-3-Clause"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "fixed_point": [True, False],
        "stack_protector": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "fixed_point": False,
        "stack_protector": True,
    }
    implements = ["auto_shared_fpic"]
    languages = "C"

    def build_requirements(self):
        self.tool_requires("cmake/[>=3.16 <5]")

    def source(self):
        get(self, url="https://github.com/xiph/opus/releases/download/v1.5.2/opus-1.5.2.tar.gz", sha256="65c1d2f78b9f2fb20082c38cbe47c951ad5839345876e46941612ee87f9a7ce1", destination=self.source_folder, strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["OPUS_BUILD_SHARED_LIBRARY"] = self.options.shared
        tc.cache_variables["OPUS_FIXED_POINT"] = self.options.fixed_point
        tc.cache_variables["OPUS_STACK_PROTECTOR"] = self.options.stack_protector
        if is_msvc(self):
            tc.cache_variables["OPUS_STATIC_RUNTIME"] = is_msvc_static_runtime(self)
        tc.generate()

    def build(self):
        apply_conandata_patches(self)
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "COPYING", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "Opus")
        self.cpp_info.set_property("cmake_target_name", "Opus::opus")
        self.cpp_info.set_property("pkg_config_name", "opus")
        # TODO: back to global scope in conan v2 once cmake_find_package_* generators removed
        self.cpp_info.components["libopus"].libs = ["opus"]
        self.cpp_info.components["libopus"].includedirs.append(os.path.join("include", "opus"))
        if self.settings.os in ["Linux", "FreeBSD", "Android"]:
            self.cpp_info.components["libopus"].system_libs.append("m")
        if self.settings.os == "Windows" and self.settings.compiler == "gcc":
            self.cpp_info.components["libopus"].system_libs.append("ssp")

        # TODO: to remove in conan v2 once cmake_find_package_* generators removed
        self.cpp_info.components["libopus"].set_property("cmake_target_name", "Opus::opus")
        self.cpp_info.components["libopus"].set_property("pkg_config_name", "opus")
