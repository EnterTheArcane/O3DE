from thirdparty import RecipeBase
from thirdparty.tools.build import check_min_cppstd
from thirdparty.tools.files import apply_patches, get, copy
from thirdparty.tools.github import GithubRepository
from thirdparty.tools.scm import Version
import os

class Recipe(RecipeBase):
    name = "wil"
    version = "1.0.260126.7"
    license = "MIT"
    # only arch is aplicable, windows library
    

    @property
    def _min_cppstd(self):
        return 11

    @property
    def _compilers_minimum_version(self):
        # About compiler version: https://github.com/microsoft/wil/issues/207#issuecomment-991722592 
        return {
            "Visual Studio": "15",
            "msvc": "191"
        }

    def latest_version(self):
        repo = GithubRepository(self, "microsoft/wil")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/microsoft/wil/archive/refs/tags/v1.0.260126.7.tar.gz",
            sha256="de9e03b38ff0ff8d22048f00b111cb631d21c550328f12530ccba71c05c9e361",
            destination=self.source_folder,
            strip_root=True)

    def build(self):
        apply_patches(self)

    def package(self):
        copy(self, pattern="LICENSE", dst=os.path.join(self.package_folder, "licenses"), src=self.source_folder)
        copy(
            self,
            pattern="*.h",
            dst=os.path.join(self.package_folder, "include"),
            src=os.path.join(self.source_folder, "include"),
        )

    def package_info(self):
        # Folders not used for header-only
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []

        # https://github.com/microsoft/wil/blob/56e3e5aa79234f8de3ceeeaf05b715b823bc2cca/CMakeLists.txt#L53
        self.cpp_info.set_property("cmake_file_name", "wil")
        self.cpp_info.set_property("cmake_target_name", "WIL::WIL")

