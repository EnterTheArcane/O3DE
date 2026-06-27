from thirdparty._internal.graph import RECIPE_CONSUMER, RECIPE_EDITABLE
from thirdparty.errors import RecipeException
from thirdparty.recipe import RecipeBase


def is_consumer(recipe: RecipeBase) -> bool:
    try:
        return recipe._recipe_node.recipe in (RECIPE_CONSUMER, RECIPE_EDITABLE)  # noqa
    except AttributeError:
        return False


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
            if tmp and var == "arch":
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
