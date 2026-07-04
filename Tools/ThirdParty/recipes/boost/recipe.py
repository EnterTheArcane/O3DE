from thirdparty import RecipeBase
from thirdparty.files import get, copy
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "boost"
    version = "1.91.0-1"
    license = "BSL-1.0"

    def latest_version(self):
        repo = GithubRepository(self, "boostorg/boost")
        return Version(repo.latest_release.removeprefix("boost-"))

    def source(self):
        get(
            self,
            url=f"https://github.com/boostorg/boost/releases/download/boost-{self.version}/boost-{self.version}-cmake.tar.gz",
            sha256="8a82bd11a720c70923806c36ee5c26dbd2d630c1eaa1d8fad9a7bd5529908a26",
            destination=self.folders.source,
            strip_root=True)

    def package(self):
        copy(self, "LICENSE_1_0.txt", src=self.folders.source, dst=self.folders.package / "licenses")
        # Header-only packaging: the whole Boost header tree lives under <root>/boost.
        # This satisfies onnxruntime's header-only Boost.MP11 usage (Boost::mp11).
        copy(self, "*", src=self.folders.source / "boost", dst=self.folders.package / "include" / "boost")

    def package_info(self):
        self.info.set_property("cmake_file_name", "Boost")
        self.info.set_property("cmake_target_name", "Boost::boost")
        self.info.bindirs = []
        self.info.libdirs = []

        # Aggregate header component
        self.info.components["headers"].set_property("cmake_target_name", "Boost::headers")
        self.info.components["headers"].bindirs = []
        self.info.components["headers"].libdirs = []

        # Boost.MP11 (header-only) - the only Boost lib onnxruntime consumes
        self.info.components["mp11"].set_property("cmake_target_name", "Boost::mp11")
        self.info.components["mp11"].requires = ["headers"]
        self.info.components["mp11"].bindirs = []
        self.info.components["mp11"].libdirs = []
