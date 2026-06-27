from thirdparty import RecipeBase, RecipeOptions
from thirdparty.apple import fix_apple_shared_install_name
from thirdparty.env import VirtualBuildEnv
from thirdparty.files import copy, get, rm, rmdir
from thirdparty.meson import Meson, MesonToolchain
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class _Options(RecipeOptions):
    shared: bool = False
    fPIC: bool = True


class Recipe(RecipeBase[_Options]):
    name = "lcms"
    version = "2.19.1"
    license = "MIT"

    def latest_version(self):
        repo = GithubRepository(self, "mm2/Little-CMS")
        return Version(repo.latest_release.removeprefix("lcms"))

    def configure(self):
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def source(self):
        get(
            self,
            url="https://github.com/mm2/Little-CMS/releases/download/lcms2.19.1/lcms2-2.19.1.tar.gz",
            sha256="bfc54f7bab59fbc921012014a8032e4cba4abd46db47d46b76416a8c0b2815c8",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        tc = MesonToolchain(self)
        tc.generate()
        env = VirtualBuildEnv(self)
        env.generate()

    def build(self):
        meson = Meson(self)
        meson.configure()
        meson.build()

    def package(self):
        copy(self, "LICENSE", src=self.folders.source, dst=self.folders.package / "licenses")
        meson = Meson(self)
        meson.install()
        rm(self, "*.pdb", self.folders.package / "bin")
        rmdir(self, self.folders.package / "lib" / "pkgconfig")
        fix_apple_shared_install_name(self)

    def package_info(self):
        self.info.set_property("pkg_config_name", "lcms2")
        self.info.libs = ["lcms2"]
        if self.settings.os == "Windows" and self.options.shared:
            self.info.defines.append("CMS_DLL")
        if self.settings.os in ("FreeBSD", "Linux"):
            self.info.system_libs.extend(["m", "pthread"])
