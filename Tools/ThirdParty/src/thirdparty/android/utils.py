from thirdparty.errors import RecipeException


def android_abi(recipe, context="host"):
    """
    Returns Android-NDK ABI

    :param recipe: RecipeBase instance
    :param context: either "host", "build" or "target"
    :return: Android-NDK ABI
    """
    if context not in ("host", "build", "target"):
        raise RecipeException(f"context argument must be either 'host', 'build' or 'target', "
                             f"was '{context}'")

    try:
        settings = getattr(recipe, f"settings_{context}")
    except AttributeError:
        if context == "host":
            settings = recipe.settings
        else:
            raise RecipeException(f"settings_{context} not declared in recipe")
    if settings is None:
        raise RecipeException(f"settings_{context}=None in recipe")
    arch = settings.get_safe("arch")
    # https://cmake.org/cmake/help/latest/variable/CMAKE_ANDROID_ARCH_ABI.html
    return {
        "armv5el": "armeabi",
        "armv5hf": "armeabi",
        "armv5": "armeabi",
        "armv6": "armeabi-v6",
        "armv7": "armeabi-v7a",
        "armv7hf": "armeabi-v7a",
        "armv8": "arm64-v8a",
    }.get(arch, arch)
