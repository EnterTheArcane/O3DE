from thirdparty import RecipeBase
from thirdparty.tools.scm import Version
from thirdparty.tools.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "ispc"
    version = "1.30.0"

    def latest_version(self):
        repo = GithubRepository(self, "ispc/ispc")
        return Version(repo.latest_release.removeprefix("v"))
