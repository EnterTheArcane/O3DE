from __future__ import annotations

from thirdparty.errors import RecipeException

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from thirdparty._internal.model.recipe import RecipeBase

universal_arch_separator = '|'


def is_universal_arch(settings_value, valid_definitions):
    if (settings_value is None or valid_definitions is None or universal_arch_separator not in settings_value):
        return False

    parts = settings_value.split(universal_arch_separator)

    if parts != sorted(parts):
        raise RecipeException(f"Architectures must be in alphabetical order separated by {universal_arch_separator}")

    valid_macos_values = [val for val in valid_definitions if ("arm" in val or "x86" in val)]

    return all(part in valid_macos_values for part in parts)


def raise_on_universal_arch(recipe: RecipeBase):
    if is_universal_arch(
        recipe.settings.get_safe("arch"), recipe.settings.possible_values().get("arch")):
        raise RecipeException("Universal binaries not supported by toolchain.")
