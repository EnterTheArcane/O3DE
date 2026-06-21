import os

from thirdparty.errors import RecipeException
from thirdparty.microsoft.visual import msvc_platform_from_arch


def vs_layout(recipe):
    """
    Initialize a layout for a typical Visual Studio project.

    :param recipe: ``< RecipeBase object >`` The current recipe object. Always use ``self``.
    """
    subproject = recipe.folders.subproject
    recipe.folders.source = subproject or "."
    recipe.folders.generators = os.path.join(subproject, "recipe") if subproject else "recipe"
    recipe.folders.build = subproject or "."
    recipe.cpp.source.includedirs = ["include"]

    try:
        build_type = str(recipe.settings.build_type)
    except RecipeException:
        raise RecipeException("The 'vs_layout' requires the 'build_type' setting")
    try:
        arch = str(recipe.settings.arch)
    except RecipeException:
        raise RecipeException("The 'vs_layout' requires the 'arch' setting")

    if arch != "None" and arch != "x86":
        msvc_arch = msvc_platform_from_arch(arch)
        if not msvc_arch:
            raise RecipeException(f"The 'vs_layout' doesn't work with the arch '{arch}'. "
                                 "Accepted architectures: 'x86', 'x86_64', 'armv7', 'armv8'")
        bindirs = os.path.join(msvc_arch, build_type)
    else:
        bindirs = build_type

    recipe.cpp.build.libdirs = [bindirs]
    recipe.cpp.build.bindirs = [bindirs]
