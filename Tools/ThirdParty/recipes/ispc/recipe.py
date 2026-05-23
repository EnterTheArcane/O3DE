from thirdparty import RecipeBase
from thirdparty.tools.scm.github import GithubRepository
from thirdparty.tools.scm import Version

class Recipe(RecipeBase):
    name = "ispc"
    version = "1.30.0"

    def latest_version(self):
        repo = GithubRepository(self, "ispc/ispc")
        return Version(repo.latest_release.removeprefix("v"))
