import os

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.tools.files import get, copy, load, save, apply_patches, collect_libs
from thirdparty.tools.apple import fix_apple_shared_install_name


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

    def requirements(self) -> list[str]:
        return []  # readline is Unix-only

    def source(self):
        get(
            url="https://www.lua.org/ftp/lua-5.5.0.tar.gz",
            dest=self.source_folder,
            sha256="57ccc32bbbd005cab75bcc52444052535af691789dba2b9016d5c50640d68b3d",
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["LUA_SRC_DIR"] = self.source_folder.replace("\\", "/")
        tc.variables["COMPILE_AS_CPP"] = self.options.compile_as_cpp
        tc.variables["SKIP_INSTALL_TOOLS"] = not self.options.with_tools
        tc.variables["WITH_READLINE"] = self.options.with_readline
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        import shutil

        shutil.copy(
            os.path.join(os.path.dirname(os.path.abspath(__file__)), "CMakeLists.txt"),
            os.path.normpath(
                os.path.join(self.source_folder, os.pardir, "CMakeLists.txt")
            ),
        )
        apply_patches(self)
        cmake = CMake(self)
        cmake.configure(build_script_folder=os.path.join(self.source_folder, os.pardir))
        cmake.build()

    def package(self):
        # Extract the License/s from the header to a file
        tmp = load(os.path.join(self.source_folder, "src", "lua.h"))
        license_contents = tmp[tmp.find("/***", 1) : tmp.find("****/", 1)]
        save(
            os.path.join(self.package_folder, "licenses", "COPYING.txt"),
            license_contents,
        )
        cmake = CMake(self)
        cmake.install()
        fix_apple_shared_install_name(self)
