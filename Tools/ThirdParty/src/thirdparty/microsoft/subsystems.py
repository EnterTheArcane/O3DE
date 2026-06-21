from thirdparty._internal.subsystems import deduce_subsystem, subsystem_path


def unix_path(recipe, path, scope="build"):
    subsystem = deduce_subsystem(recipe, scope=scope)
    return subsystem_path(subsystem, path)


def unix_path_package_info_legacy(recipe, path, path_flavor=None):
    message = "The use of 'unix_path_legacy_compat' is deprecated in Recipe 2.0 and does not " \
              "perform path conversions. This is retained for compatibility with Recipe 1.x " \
              "and will be removed in a future version."
    recipe.output.warning(message, warn_tag="deprecated")
    return path

