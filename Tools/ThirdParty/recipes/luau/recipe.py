import os

from thirdparty import RecipeBase
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import get, copy
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "luau"
    version = "0.722"
    license = "MIT"

    def latest_version(self):
        repo = GithubRepository(self, "luau-lang/luau")
        return Version(repo.latest_release)

    def source(self):
        get(
            self,
            url="https://github.com/luau-lang/luau/archive/0.722.tar.gz",
            sha256="b69d7dd42540dc3892c5aa2f5c733897a8350ad64f09a0e0984a8c42ba29961b",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["LUAU_BUILD_CLI"] = False
        tc.variables["LUAU_BUILD_TESTS"] = False
        tc.variables["LUAU_BUILD_WEB"] = False
        tc.variables["LUAU_WERROR"] = False
        tc.variables["LUAU_STATIC_CRT"] = False
        tc.variables["LUAU_NATIVE"] = True
        tc.variables["LUAU_SRC_DIR"] = self.folders.source.replace("\\", "/")
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure(build_script_folder=os.path.join(self.folders.source, os.pardir))
        cmake.build()

    def package(self):
        copy(self, "lua_LICENSE*", src=self.folders.source, dst=os.path.join(self.folders.package, "licenses"))
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "Luau")
        self.cpp_info.set_property("cmake_target_name", "Luau::Luau")
        # Common
        self.cpp_info.components["Common"].libs = ["Luau.Common"]
        self.cpp_info.components["Common"].set_property("cmake_target_name", "Luau::Common")
        # Ast
        self.cpp_info.components["Ast"].libs = ["Luau.Ast"]
        self.cpp_info.components["Ast"].set_property("cmake_target_name", "Luau::Ast")
        self.cpp_info.components["Ast"].requires = ["Common"]
        # VM
        self.cpp_info.components["VM"].libs = ["Luau.VM"]
        self.cpp_info.components["VM"].set_property("cmake_target_name", "Luau::VM")
        self.cpp_info.components["VM"].requires = ["Common"]
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.components["VM"].system_libs = ["m"]
        # Analysis
        self.cpp_info.components["Analysis"].libs = ["Luau.Analysis"]
        self.cpp_info.components["Analysis"].set_property("cmake_target_name", "Luau::Analysis")
        self.cpp_info.components["Analysis"].requires = ["Ast", "Config", "Compiler", "VM"]
        # Config
        self.cpp_info.components["Config"].libs = ["Luau.Config"]
        self.cpp_info.components["Config"].set_property("cmake_target_name", "Luau::Config")
        self.cpp_info.components["Config"].requires = ["Ast", "Compiler", "VM"]
        # Compiler
        self.cpp_info.components["Compiler"].libs = ["Luau.Compiler"]
        self.cpp_info.components["Compiler"].set_property("cmake_target_name", "Luau::Compiler")
        self.cpp_info.components["Compiler"].requires = ["Ast"]
        # CodeGen
        self.cpp_info.components["CodeGen"].libs = ["Luau.CodeGen"]
        self.cpp_info.components["CodeGen"].set_property("cmake_target_name", "Luau::CodeGen")
        self.cpp_info.components["CodeGen"].requires = ["VM", "Common"]
        # Require
        self.cpp_info.components["Require"].libs = ["Luau.Require"]
        self.cpp_info.components["Require"].set_property("cmake_target_name", "Luau::Require")
        self.cpp_info.components["Require"].requires = ["Config", "VM"]
        # Web
        if self.options.with_web:
            self.cpp_info.components["Web"].libs = ["Luau.Web"]
            self.cpp_info.components["Web"].set_property("cmake_target_name", "Luau::Web")
            self.cpp_info.components["Web"].requires = ["Compiler", "VM", "Analysis"]
