import os

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import get, save
from thirdparty.tools.scm import Version


class Recipe(RecipeBase):
    name = "mikktspace"
    version = "2020.03.25"
    license = "Zlib"

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
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def build_requirements(self):
        self.tool_requires("cmake")

    def latest_version(self):
        return Version(self.version)

    def source(self):
        get(
            self,
            url="https://github.com/mmikk/MikkTSpace/archive/3e895b49d05ea07e4c2133156cfa94369e19e409.tar.gz",
            sha256="aeba65ddee85a679133d510d71d00b108f603752f90b15b9ecf7777033f6e351",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["MIKKTSPACE_SRC_DIR"] = self.source_folder.replace("\\", "/")
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure(build_script_folder=os.path.join(self.source_folder, os.pardir))
        cmake.build()

    @property
    def _extracted_license(self):
        with open(os.path.join(self.source_folder, "mikktspace.h")) as f:
            lines = f.readlines()
        return "".join(line[4:] for line in lines[4:21])

    def package(self):
        save(self, os.path.join(self.package_folder, "licenses", "LICENSE"), self._extracted_license)
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["mikktspace"]
        if not self.options.shared and self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs = ["m"]
