from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.tools.files import apply_patches, copy, get, load, save
import os

class Recipe(RecipeBase):
    name = "minizip"
    version = "1.3.1"
    license = "Zlib"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "bzip2": [True, False],
        "tools": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "bzip2": True,
        "tools": False,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def requirements(self):
        self.requires("zlib", transitive_headers=True)
        if self.options.bzip2:
            self.requires("bzip2", transitive_headers=True)

    def source(self):
        get(
            self,
            url="https://zlib.net/fossils/zlib-1.3.1.tar.gz",
            sha256="9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["MINIZIP_SRC_DIR"] = os.path.join(self.source_folder, "contrib", "minizip").replace("\\", "/")
        tc.variables["MINIZIP_ENABLE_BZIP2"] = self.options.bzip2
        tc.variables["MINIZIP_BUILD_TOOLS"] = self.options.tools
        # fopen64 and similar are unavailable before API level 24: https://github.com/madler/zlib/pull/436
        if self.settings.os == "Android" and int(str(self.settings.os.api_level)) < 24:
            tc.preprocessor_definitions["IOAPI_NO_64"] = "1"
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        apply_patches(self)
        cmake = CMake(self)
        cmake.configure(build_script_folder=os.path.join(self.source_folder, os.pardir))
        cmake.build()

    def _extract_license(self):
        zlib_h = load(self, os.path.join(self.source_folder, "zlib.h"))
        return zlib_h[2:zlib_h.find("*/", 1)]

    def package(self):
        save(self, os.path.join(self.package_folder, "licenses", "LICENSE"), self._extract_license())
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["minizip"]
        self.cpp_info.includedirs.append(os.path.join("include", "minizip"))
        if self.options.bzip2:
            self.cpp_info.defines.append("HAVE_BZIP2")
