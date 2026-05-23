import os

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.tools.files import copy, get, rm, rmdir
from thirdparty.tools.scm.github import GithubRepository
from thirdparty.tools.scm import Version


class Recipe(RecipeBase):
    name = "slang"
    version = "2026.9.1"
    license = ("Apache-2.0", "MIT")

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
        self.requires("lz4")
        self.requires("lua") # TODO
        self.requires("miniz")
        self.requires("spirv-headers")
        self.requires("unordered-dense")
        self.requires("vulkan-headers")
        self.requires("zstd")

    def latest_version(self):
        repo = GithubRepository(self, "shader-slang/slang")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/shader-slang/slang/archive/refs/tags/v2026.9.1.tar.gz",
            sha256="53c0bf21eff7ba8e3825395ee3a4d7564c2a330fa32e47e165926527c7994303",
            destination=self.source_folder,
            strip_root=True)
        get(
            self,
            url="https://github.com/lua/lua/archive/3fe7be956f23385aa1950dc31e2f25127ccfc0ea.tar.gz",
            sha256="4776526f89abeea61cce41a056577859180dbb2d4cb6c1dad00955872a1007bb",
            destination=os.path.join(self.source_folder, "external", "lua"),
            strip_root=True)
        get(
            self,
            url="https://github.com/swiftlang/swift-cmark/archive/924936d0427cb25a61169739a7660230bffa6ea6.tar.gz",
            sha256="1c51659bd47c34df1c8976f893adc43ba039a98f6eac4fa95d53d1e08ba6072a",
            destination=os.path.join(self.source_folder, "external", "cmark"),
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["SLANG_LIB_TYPE"] = "SHARED" if self.options.shared else "STATIC"
        tc.cache_variables["SLANG_SLANG_LLVM_FLAVOR"] = "DISABLE"
        tc.cache_variables["SLANG_STANDARD_MODULE_DIR_NAME"] = "slang-standard-module"
        tc.variables["SLANG_ENABLE_GFX"] = False
        tc.variables["SLANG_ENABLE_SLANGD"] = False
        tc.variables["SLANG_ENABLE_SLANGRT"] = False
        tc.variables["SLANG_ENABLE_SLANG_GLSLANG"] = False
        tc.variables["SLANG_ENABLE_TESTS"] = False
        tc.variables["SLANG_ENABLE_EXAMPLES"] = False
        tc.variables["SLANG_ENABLE_SLANG_RHI"] = False
        tc.variables["SLANG_ENABLE_SLANGI"] = False
        tc.variables["SLANG_ENABLE_REPLAYER"] = False
        tc.variables["SLANG_ENABLE_SLANGC"] = True
        tc.variables["SLANG_ENABLE_DXIL"] = False
        tc.variables["SLANG_EXCLUDE_DAWN"] = True
        tc.variables["SLANG_EXCLUDE_TINT"] = True
        tc.variables["SLANG_USE_SYSTEM_LZ4"] = True
        tc.variables["SLANG_USE_SYSTEM_MINIZ"] = True
        tc.variables["SLANG_USE_SYSTEM_SPIRV_HEADERS"] = True
        tc.variables["SLANG_USE_SYSTEM_UNORDERED_DENSE"] = True
        tc.variables["SLANG_USE_SYSTEM_VULKAN_HEADERS"] = True
        tc.generate()
        deps = CMakeDeps(self)
        deps.set_property("lz4", "cmake_target_name", "LZ4::lz4")
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        rmdir(self, os.path.join(self.package_folder, "cmake"))
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        rm(self, "*.pdb", os.path.join(self.package_folder, "bin"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "slang")
        self.cpp_info.set_property("cmake_target_name", "slang::slang")
        self.cpp_info.libs = ["slang-compiler"]
