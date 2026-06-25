from typing import TYPE_CHECKING

from thirdparty._internal.subsystems import deduce_subsystem, subsystem_path

if TYPE_CHECKING:
    from thirdparty._internal.model.recipe import RecipeBase


def unix_path(recipe: RecipeBase, path: str, scope: str = "build") -> str | None:
    subsystem = deduce_subsystem(recipe, scope=scope)
    return subsystem_path(subsystem, path)
