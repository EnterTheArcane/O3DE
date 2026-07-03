import os

from thirdparty import RecipeBase, RecipeOptions
from thirdparty.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.files import apply_patches, get, load, save


class _Options(RecipeOptions):
    shared: bool = False
    pic: bool = True


class Recipe(RecipeBase[_Options]):
    name = "minizip"
    version = "1.3.1"
    license = "Zlib"

    def configure(self):
        self.settings.compiler_cxx_standard = None
        self.settings.compiler_libcxx = None

    def requirements(self):
        self.requires_tool("cmake")
        self.requires("bzip2")
        self.requires("zlib")

    def source(self):
        get(
            self,
            url=f"https://github.com/madler/zlib/archive/refs/tags/v{self.version}.tar.gz",
            sha256="17e88863f3600672ab49182f217281b6fc4d3c762bde361935e436a95214d05c",
            destination=self.folders.source,
            strip_root=True)
        apply_patches(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["MINIZIP_SRC_DIR"] = (self.folders.source / "contrib" / "minizip").as_posix()
        tc.variables["MINIZIP_ENABLE_BZIP2"] = True
        tc.variables["MINIZIP_BUILD_TOOLS"] = True
        # fopen64 and similar are unavailable before API level 24: https://github.com/madler/zlib/pull/436
        if self.settings.os == "Android" and int(str(self.settings.os_api_level)) < 24:
            tc.preprocessor_definitions["IOAPI_NO_64"] = "1"
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure(build_script_folder=self.folders.recipe)
        cmake.build()

    def package(self):
        save(self, self.folders.package / "licenses" / "LICENSE", self._extract_license())
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.info.libs = ["minizip"]
        self.info.includedirs.append(os.path.join("include", "minizip"))
        self.info.defines.append("HAVE_BZIP2")

    def _extract_license(self):
        zlib_h = load(self, self.folders.source / "zlib.h")
        return zlib_h[2:zlib_h.find("*/", 1)]
