from thirdparty import RecipeBase
from thirdparty.errors import RecipeInvalidConfiguration
from thirdparty.files import copy, get
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "radeon-gpu-analyzer"
    version = "2.14.1"
    license = "MIT"

    def latest_version(self):
        repo = GithubRepository(self, "GPUOpen-Tools/radeon_gpu_analyzer")
        return Version(repo.latest_release)

    def validate(self):
        if self.settings.os not in ["Windows", "Linux"]:
            raise RecipeInvalidConfiguration(f"{self.name} has no prebuilt binaries for {self.settings.os}")

    def build(self):
        if self.settings.os == "Windows":
            url = "https://github.com/GPUOpen-Tools/radeon_gpu_analyzer/releases/download/2.14.1/rga-windows-x64-2.14.1.zip"
            sha256 = "5c2d2b557b063b54d60bd7d9739c0734eb8cd4ae82676c02a9fb043e06a92799"
        elif self.settings.os == "Linux":
            url = "https://github.com/GPUOpen-Tools/radeon_gpu_analyzer/releases/download/2.14.1/rga-linux-2.14.1.tgz"
            sha256 = "34ab6ab30caf8cdd426cf7f5202bfda31f9ad6a87e96053e817a0656dffacc86"
        else:
            raise RecipeInvalidConfiguration(f"{self.name} has no prebuilt binaries for {self.settings.os}")
        get(self, url=url, sha256=sha256, destination=self.folders.build, strip_root=True)

    def package(self):
        copy(self, "*", src=self.folders.build, dst=self.folders.package / "bin")

    def package_info(self):
        self.info.libdirs = []
        self.info.includedirs = []
        self.buildenv_info.prepend_path("PATH", self.folders.package / "bin")
