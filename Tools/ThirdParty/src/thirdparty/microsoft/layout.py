import os

from thirdparty.errors import RecipeException
from thirdparty.microsoft.visual import msvc_platform_from_arch

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from thirdparty._internal.model.recipe import RecipeBase


def vs_layout(recipe: RecipeBase):
    """
    Initialize a layout for a typical Visual Studio project.

    :param recipe: ``< RecipeBase object >`` The current recipe object. Always use ``self``.
    """
    subproject = recipe.folders.subproject
    recipe.folders._source = subproject or "."
    recipe.folders._generators = os.path.join(subproject, "recipe") if subproject else "recipe"
    recipe.folders._build = subproject or "."
    recipe.infos.source.includedirs = ["include"]

    try:
        build_type = str(recipe.settings.build_type)
    except RecipeException:
        raise RecipeException("The 'vs_layout' requires the 'build_type' setting")
    try:
        arch = str(recipe.settings.arch)
    except RecipeException:
        raise RecipeException("The 'vs_layout' requires the 'arch' setting")

    if arch != "None":
        msvc_arch = msvc_platform_from_arch(arch)
        if not msvc_arch:
            raise RecipeException(
                f"The 'vs_layout' doesn't work with the arch '{arch}'. "
                "Accepted architectures: 'X64', 'ARM'")
        bindirs = os.path.join(msvc_arch, build_type)
    else:
        bindirs = build_type

    recipe.infos.build.libdirs = [bindirs]
    recipe.infos.build.bindirs = [bindirs]
