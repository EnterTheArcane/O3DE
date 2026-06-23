from __future__ import annotations

from typing import TYPE_CHECKING

from thirdparty._internal.subsystems import deduce_subsystem, subsystem_path

if TYPE_CHECKING:
    from thirdparty._internal.model.recipe_base import RecipeBase


def unix_path(recipe: RecipeBase, path: str, scope: str = "build") -> str | None:
    subsystem = deduce_subsystem(recipe, scope=scope)
    return subsystem_path(subsystem, path)


def unix_path_package_info_legacy(recipe: RecipeBase, path: str,
                                  path_flavor: str | None = None) -> str:
    message = "The use of 'unix_path_legacy_compat' is deprecated in Recipe 2.0 and does not " \
              "perform path conversions. This is retained for compatibility with Recipe 1.x " \
              "and will be removed in a future version."
    recipe.output.warning(message, warn_tag="deprecated")
    return path
