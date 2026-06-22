from pathlib import Path

from thirdparty._internal.graph.graph import CONTEXT_BUILD


class RecipeInterface:
    """
    This is just a protective wrapper to give consumers
    a limited view of recipe dependencies, "read" only,
    and only to some attributes, not methods
    """

    def __str__(self):
        return str(self._recipe)

    def __init__(self, recipe, consumer):
        self._recipe = recipe
        self._consumer = consumer

    def __eq__(self, other):
        """
        The recipe is a different entity per node, and recipe equality is identity
        :type other: RecipeInterface
        """
        return self._recipe == other._recipe

    def __hash__(self):
        return hash(self._recipe)

    @property
    def options(self):
        return self._recipe.options

    @property
    def recipe_folder(self):
        return self._recipe.recipe_folder

    @property
    def recipe_metadata_folder(self):
        return self._recipe.recipe_metadata_folder

    @property
    def package_folder(self):
        return self._recipe.package_folder

    @property
    def immutable_package_folder(self):
        return self._recipe.immutable_package_folder

    @property
    def package_metadata_folder(self):
        return self._recipe.package_metadata_folder

    @property
    def package_path(self) -> Path:
        assert self.package_folder is not None, "`package_folder` is `None`"
        return Path(self.package_folder)

    @property
    def ref(self):
        return self._recipe.ref

    @property
    def pref(self):
        return self._recipe.pref

    @property
    def buildenv_info(self):
        return self._recipe.buildenv_info

    @property
    def runenv_info(self):
        return self._recipe.runenv_info

    @property
    def cpp_info(self):
        # At the moment, not doing a full copy, not necessary as access is not concurrent
        self._recipe.cpp_info.set_consumer(self._consumer)
        return self._recipe.cpp_info

    @property
    def settings(self):
        return self._recipe.settings

    @property
    def settings_build(self):
        return self._recipe.settings_build

    @property
    def context(self):
        return self._recipe.context

    @property
    def conf_info(self):
        return self._recipe.conf_info

    @property
    def dependencies(self):
        return self._recipe.dependencies

    @property
    def folders(self):
        return self._recipe.folders

    @property
    def is_build_context(self):
        return self._recipe.context == CONTEXT_BUILD

    @property
    def info(self):
        return self._recipe.info

    def set_deploy_folder(self, deploy_folder):
        self._recipe.set_deploy_folder(deploy_folder)

    @property
    def license(self):
        return self._recipe.license

    @property
    def extension_properties(self):
        return getattr(self._recipe, "extension_properties", {})

    @property
    def recipe(self) -> str:
        # IMPORTANT: this should be used only for "informational" purposes, see GH#18996.
        return self._recipe._recipe_node.recipe  # noqa

    @property
    def conf(self):
        return self._recipe.conf
