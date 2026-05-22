from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import copy, get, rmdir
import os


class Recipe(RecipeBase):
    name = "pugixml"
    version = "1.15"
    license = "MIT"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    def source(self):
        get(
            url="https://github.com/zeux/pugixml/releases/download/v1.15/pugixml-1.15.tar.gz",
            dest=self.source_folder,
            sha256="655ade57fa703fb421c2eb9a0113b5064bddb145d415dd1f88c79353d90d511a",
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_TESTING"] = False
        if self.is_windows:
            # Avoid linking against MSVC's static CRT, which conflicts with
            # the rest of the build that uses the dynamic CRT.
            tc.variables["PUGIXML_STATIC_CRT"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(
            "LICENSE.md",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        cmake = CMake(self)
        cmake.install()
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(os.path.join(self.package_folder, "lib", "cmake"))

    def package_info(self):
        self.cpp_info.libs = ["pugixml"]
        self.cpp_info.set_property("cmake_file_name", "pugixml")
        self.cpp_info.set_property("cmake_target_name", "pugixml::pugixml")
