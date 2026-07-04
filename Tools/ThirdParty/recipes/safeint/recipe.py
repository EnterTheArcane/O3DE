from thirdparty import RecipeBase
from thirdparty.files import get, copy
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "safeint"
    version = "3.0.28"
    license = "MIT"

    def latest_version(self):
        repo = GithubRepository(self, "dcleblanc/SafeInt")
        return Version(repo.latest_release)

    def source(self):
        get(
            self,
            url=f"https://github.com/dcleblanc/SafeInt/archive/refs/tags/{self.version}.tar.gz",
            sha256="d6b164bcea92a746e4d44132e505c7ab1816d1089ba99ebc674ccd4b70262ed5",
            destination=self.folders.source,
            strip_root=True)

    def package(self):
        copy(self, "LICENSE", src=self.folders.source, dst=self.folders.package / "licenses")
        copy(self, "SafeInt.hpp", src=self.folders.source, dst=self.folders.package / "include")

    def package_info(self):
        self.info.set_property("cmake_file_name", "safeint")
        self.info.set_property("cmake_target_name", "safeint::safeint")
        self.info.bindirs = []
        self.info.libdirs = []
