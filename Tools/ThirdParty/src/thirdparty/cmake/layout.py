import os
import tempfile

from thirdparty._internal.graph import RECIPE_CONSUMER, RECIPE_EDITABLE
from thirdparty.errors import RecipeException
from thirdparty.recipe import RecipeBase


def is_consumer(recipe: RecipeBase) -> bool:
    try:
        return recipe._recipe_node.recipe in (RECIPE_CONSUMER, RECIPE_EDITABLE)  # noqa
    except AttributeError:
        return False


def cmake_layout(recipe: RecipeBase, generator=None, src_folder: str = ".", build_folder: str = "build"):
    """
    :param recipe: The current recipe object. Always use ``self``.
    :param generator: Allow defining the CMake generator. In most cases it doesn't need to be passed,
                      as it will get the value from the configuration
                      ``tools.cmake.cmaketoolchain:generator``, or it will automatically deduce
                      the generator from the ``settings``
    :param src_folder: Value for ``recipe.folders.source``, change it if your source code
                       (and CMakeLists.txt) is in a subfolder.
    :param build_folder: Specify the name of the "base" build folder. The default is "build", but
                        if that folder name is used by the project, a different one can be defined
    """
    gen = recipe.conf.get("tools.cmake.cmaketoolchain:generator", default=generator)
    if gen:
        multi = "Visual" in gen or "Xcode" in gen or "Multi-Config" in gen
    else:
        compiler = recipe.settings.get_safe("compiler")
        if compiler == "msvc":
            multi = True
        else:
            multi = False

    subproject = recipe.folders.subproject
    recipe.folders._source = src_folder if not subproject else os.path.join(subproject, src_folder)
    try:
        build_type = str(recipe.settings.build_type)
    except RecipeException:
        raise RecipeException("'build_type' setting not defined, it is necessary for cmake_layout()")

    if is_consumer(recipe):
        folder = "build_folder"
        build_folder = recipe.conf.get(f"tools.cmake.cmake_layout:{folder}") or build_folder
        if build_folder == "$TMP":
            build_folder = tempfile.mkdtemp()

    build_folder = build_folder if not subproject else os.path.join(subproject, build_folder)
    config_build_folder, user_defined_build = get_build_folder_custom_vars(recipe)
    if config_build_folder:
        build_folder = os.path.join(build_folder, config_build_folder)
    if not multi and not user_defined_build:
        build_folder = os.path.join(build_folder, build_type)
    recipe.folders._build = build_folder

    recipe.folders._generators = os.path.join(recipe.folders._build, "generators")

    recipe.infos.source.includedirs = ["include"]

    if multi:
        recipe.infos.build.libdirs = [build_type]
        recipe.infos.build.bindirs = [build_type]
    else:
        recipe.infos.build.libdirs = ["."]
        recipe.infos.build.bindirs = ["."]


def get_build_folder_custom_vars(recipe: RecipeBase):
    recipe_vars = recipe.folders.build_folder_vars
    build_vars = recipe.conf.get("tools.cmake.cmake_layout:build_folder_vars", check_type=list)
    if is_consumer(recipe):
        if build_vars is None:
            build_vars = recipe_vars or []
    else:
        build_vars = recipe_vars or []

    ret = []
    for s in build_vars:
        group, var = s.split(".", 1)
        tmp = None
        if group == "settings":
            tmp = recipe.settings.get_safe(var)
            if tmp and var == "arch":  # handle Apple multi-arch/universal binaries
                tmp = tmp.replace("|", "_")
        elif group == "options":
            value = recipe.options.get_safe(var)
            if value is not None:
                if var == "shared":
                    tmp = "shared" if value else "static"
                else:
                    tmp = f"{var}_{value}"
        elif group == "self":
            tmp = getattr(recipe, var, None)
        elif group == "const":
            tmp = var
        else:
            raise RecipeException(
                "Invalid 'tools.cmake.cmake_layout:build_folder_vars' value, it has"
                f" to start with 'settings.', 'options.', 'self.' or 'const.': {s}")
        if tmp:
            ret.append(tmp.lower())

    user_defined_build = "settings.build_type" in build_vars
    return "-".join(ret), user_defined_build
