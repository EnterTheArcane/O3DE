from collections import OrderedDict
import unittest

from thirdparty._internal.graph import CONTEXT_BUILD, CONTEXT_HOST
from thirdparty._internal.loader import make_probe_recipe
from thirdparty._internal.model.conf import Conf
from thirdparty._internal.model.dependencies import RecipeDependencies
from thirdparty._internal.model.recipe import RecipeBase, RecipeState
from thirdparty._internal.model.settings import Settings


class Recipe(RecipeBase):
    name = "recipe-state-test"
    version = "1.0"
    license = "MIT"


def make_state(
    *,
    dependencies: RecipeDependencies | None = None,
    build_context: bool = False,
    settings: Settings | None = None,
    settings_build: Settings | None = None,
    conf: Conf | None = None,
) -> RecipeState:
    settings = settings if settings is not None else Settings("X64", "Release", "Mac")
    return RecipeState(
        dependencies=dependencies if dependencies is not None else RecipeDependencies(OrderedDict()),
        build_context=build_context,
        settings=settings,
        settings_build=settings_build if settings_build is not None else settings,
        conf=conf if conf is not None else Conf(),
    )


class RecipeStateTest(unittest.TestCase):
    def test_fresh_recipe_has_empty_read_only_dependencies(self):
        recipe = Recipe()

        self.assertIsInstance(recipe.dependencies, RecipeDependencies)
        self.assertFalse(recipe.dependencies)
        self.assertIsInstance(recipe.settings, Settings)
        self.assertIsInstance(recipe.settings_build, Settings)
        self.assertIsInstance(recipe.conf, Conf)
        self.assertEqual(recipe.context, CONTEXT_HOST)
        self.assertFalse(recipe.is_build_context)

        with self.assertRaises(AttributeError):
            recipe.dependencies = RecipeDependencies(OrderedDict())

    def test_internal_state_updates_recipe_facades(self):
        recipe = Recipe()
        dependencies = RecipeDependencies(OrderedDict())

        recipe._state = make_state(
            dependencies=dependencies,
            build_context=True,
        )

        self.assertIs(recipe.dependencies, dependencies)
        self.assertEqual(recipe.context, CONTEXT_BUILD)
        self.assertTrue(recipe.is_build_context)

    def test_settings_and_conf_are_read_only_state_facades(self):
        recipe = Recipe()
        settings = Settings("X64", "Release", "Mac")
        settings_build = Settings("ARM", "Debug", "Windows")
        conf = Conf()

        recipe._state = make_state(settings=settings, settings_build=settings_build, conf=conf)

        self.assertIs(recipe.settings, settings)
        self.assertIs(recipe.settings_build, settings_build)
        self.assertIs(recipe.conf, conf)

        with self.assertRaises(AttributeError):
            recipe.settings = settings
        with self.assertRaises(AttributeError):
            recipe.settings_build = settings_build
        with self.assertRaises(AttributeError):
            recipe.conf = conf

    def test_recipe_does_not_receive_graph_node_back_reference(self):
        recipe = Recipe()

        self.assertFalse(hasattr(recipe, "_node"))
        self.assertFalse(hasattr(recipe, "_set_runtime_state"))
        self.assertFalse(hasattr(recipe, "origin"))

    def test_probe_recipe_receives_complete_state(self):
        recipe = make_probe_recipe(Recipe, self._recipes_root(), "recipe-state-test", "1.0", "Release")

        self.assertIsInstance(recipe.dependencies, RecipeDependencies)
        self.assertFalse(recipe.is_build_context)
        self.assertIsInstance(recipe.settings, Settings)
        self.assertIsInstance(recipe.settings_build, Settings)
        self.assertIsInstance(recipe.conf, Conf)

    @staticmethod
    def _recipes_root():
        from pathlib import Path
        return Path(__file__).resolve().parents[1] / "recipes"


if __name__ == "__main__":
    unittest.main()
