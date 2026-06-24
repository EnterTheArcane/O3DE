from __future__ import annotations

from thirdparty._internal.model.version import Version
from thirdparty.errors import RecipeInvalidConfiguration, RecipeException

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from thirdparty._internal.model.recipe_base import RecipeBase


def check_min_compiler_version(recipe: RecipeBase, compiler_restrictions):
    """(Experimental) Checks if the current compiler and its version meet the minimum requirements.

    :param recipe: The current recipe object. Always use ``self``.
    :param compiler_restrictions:
        A list of tuples, where each tuple contains:

        - **compiler** (*str*): The name of the compiler (e.g., "gcc", "msvc").
        - **min_version** (*str*): The minimum required version as a string (e.g., "14", "19.0").
        - **reason** (*str*): A string explaining the reason for the version requirement.
    :raises RecipeException:
        If the 'compiler' or 'compiler.version' settings are not defined.
    :raises RecipeInvalidConfiguration:
        If the found compiler version is less than the specified minimum version for that compiler.

    :Example:
        .. code-block:: python

            def validate(self):
                compiler_restrictions = [
                    ("clang", "14", "requires C++20 coroutines support"),
                    ("gcc", "12", "requires C++20 modules support")
                ]
                check_min_compiler_version(self, compiler_restrictions)
    """
    compiler_value = recipe.settings.get_safe("compiler")
    if not compiler_value:
        raise RecipeException("Called check_min_compiler_version with no compiler defined")
    compiler_version = recipe.settings.get_safe("compiler.version")
    if not compiler_version:
        raise RecipeException("Called check_min_compiler_version with no compiler.version defined")

    for compiler, min_version, reason in compiler_restrictions:
        if compiler_value == compiler:
            if Version(compiler_version) < Version(min_version):
                ref = recipe.ref if hasattr(recipe, "ref") else recipe.name
                raise RecipeInvalidConfiguration(
                    f"{ref} requires {compiler} >= {min_version}, but {compiler} {compiler_version} was found\n"
                    f"Reason: {reason}")
            break
