import os

from thirdparty import RecipeBase
from thirdparty.apple import fix_apple_shared_install_name
from thirdparty.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.files import get, load, save, apply_patches, collect_libs


class Recipe(RecipeBase):
    name = "lua"
    version = "5.5.0"
    license = "MIT"

    options = {
        "shared": [False, True],
        "fPIC": [True, False],
        "compile_as_cpp": [True, False],
        "with_tools": [True, False],
        "with_readline": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "compile_as_cpp": False,
        "with_tools": False,
        "with_readline": False,
    }

    def configure(self):
        if not self.options.compile_as_cpp:
            self.options.rm_safe("compiler.libcxx")
            self.options.rm_safe("compiler.cppstd")

    def requirements(self):
        if self.options.with_tools and self.options.with_readline:
            self.requires("readline")

    def source(self):
        get(
            self,
            url="https://www.lua.org/ftp/lua-5.5.0.tar.gz",
            sha256="57ccc32bbbd005cab75bcc52444052535af691789dba2b9016d5c50640d68b3d",
            destination=self.folders.source,
            strip_root=True)
        apply_patches(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["LUA_SRC_DIR"] = self.folders.source.replace("\\", "/")
        tc.variables["COMPILE_AS_CPP"] = self.options.compile_as_cpp
        tc.variables["SKIP_INSTALL_TOOLS"] = not self.options.with_tools
        tc.variables["WITH_READLINE"] = self.options.with_readline
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure(build_script_folder=os.path.join(self.folders.source, os.pardir))
        cmake.build()

    def package(self):
        # Extract the License/s from the header to a file
        tmp = load(self, os.path.join(self.folders.source, "src", "lua.h"))
        license_contents = tmp[tmp.find("/***", 1):tmp.find("****/", 1)]
        save(self, os.path.join(self.folders.package, "licenses", "COPYING.txt"), license_contents)
        cmake = CMake(self)
        cmake.install()
        fix_apple_shared_install_name(self)

    def package_info(self):
        self.cpp_info.libs = collect_libs(self)
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs = ["dl", "m"]
        if self.settings.os in ["Linux", "Mac"]:
            self.cpp_info.defines.extend(["LUA_USE_DLOPEN", "LUA_USE_POSIX"])
        elif self.settings.os == "Windows" and self.options.shared:
            self.cpp_info.defines.append("LUA_BUILD_AS_DLL")
