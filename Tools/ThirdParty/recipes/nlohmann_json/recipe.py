from thirdparty import RecipeBase
from thirdparty.files import get, copy
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "nlohmann_json"
    version = "3.11.3"
    license = "MIT"

    def latest_version(self):
        repo = GithubRepository(self, "nlohmann/json")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url=f"https://github.com/nlohmann/json/archive/v{self.version}.tar.gz",
            sha256="0d8ef5af7f9794e3263480193c491549b2ba6cc74bb018906202ada498a79406",
            destination=self.folders.source,
            strip_root=True)

    def package(self):
        copy(self, "LICENSE*", src=self.folders.source, dst=self.folders.package / "licenses")
        copy(self, "*", src=self.folders.source / "include", dst=self.folders.package / "include")

    def package_info(self):
        self.info.set_property("cmake_file_name", "nlohmann_json")
        self.info.set_property("cmake_target_name", "nlohmann_json::nlohmann_json")
        self.info.set_property("pkg_config_name", "nlohmann_json")
        self.info.bindirs = []
        self.info.libdirs = []
