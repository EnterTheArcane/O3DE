from thirdparty._internal.model.options import _PackageOption
from thirdparty._internal.output import Output
from thirdparty.build.flags import cppstd_msvc_flag, disable_flag
from thirdparty.recipe import RecipeBase

# https://mesonbuild.com/Reference-tables.html#operating-system-names
_meson_system_map = {
    'Android': 'android', 'Mac': 'darwin', 'iOS': 'darwin', 'tvOS': 'darwin', 'visionOS': 'darwin', 'Emscripten': 'emscripten', 'Linux': 'linux', 'SunOS': 'sunos', 'Windows': 'windows', 'WindowsCE': 'windows', 'WindowsStore': 'windows',
}

# https://mesonbuild.com/Reference-tables.html#cpu-families
_meson_cpu_family_map = {
    'X64': ('x86_64', 'x86_64', 'little'), 'ARM': ('aarch64', 'aarch64', 'little'),
}


def get_apple_subsystem(apple_sdk):
    return {
        "iphoneos": "ios", "iphonesimulator": "ios-simulator", "appletvos": "tvos", "appletvsimulator": "tvos-simulator", "watchos": "watchos", "watchsimulator": "watchos-simulator",
    }.get(apple_sdk, "macos")


def to_meson_machine(machine_os, machine_arch):
    """Gets the OS system info as the Meson machine context.

    :param machine_os: ``str`` OS name.
    :param machine_arch: ``str`` OS arch.
    :return: ``dict`` Meson machine context.
    """
    system = _meson_system_map.get(machine_os, machine_os.lower())
    default_cpu_tuple = (machine_arch.lower(), machine_arch.lower(), 'little')
    cpu_tuple = _meson_cpu_family_map.get(machine_arch, default_cpu_tuple)
    cpu_family, cpu, endian = cpu_tuple[0], cpu_tuple[1], cpu_tuple[2]
    context = {
        'system': system, 'cpu_family': cpu_family, 'cpu': cpu, 'endian': endian,
    }
    return context


def to_meson_value(value):
    """Puts any value with a valid str-like Meson format.

    :param value: ``str``, ``bool``, or ``list``, otherwise, it will do nothing.
    :return: formatted value as a ``str``.
    """
    # https://mesonbuild.com/Machine-files.html#data-types
    # we don't need to transform the integer values
    if isinstance(value, str):
        return f"'{value}'"
    elif isinstance(value, bool):
        return 'true' if value else 'false'
    elif isinstance(value, list):
        return f'[{", ".join([str(to_meson_value(val)) for val in value])}]'
    elif isinstance(value, _PackageOption):
        Output().warning(
            f"Please, do not use a Recipe option value directly. "
            f"Convert 'options.{value.name}' into a valid Python"
            f"data type, e.g, bool(self.options.shared)", warn_tag="deprecated")
    return value


def to_cppstd_flag(
    recipe: RecipeBase,
    compiler,
    compiler_version,
    cppstd):
    """Gets a valid cppstd flag.
    :param recipe: ``RecipeBase`` instance.
    :param compiler: ``str`` compiler name.
    :param compiler_version: ``str`` compiler version.
    :param cppstd: ``str`` cppstd version.
    :return: ``str`` cppstd flag.
    """
    if cppstd is None:
        return None
    if disable_flag(recipe, "cppstd"):
        return None
    if compiler == "msvc":
        # Meson's logic with 'vc++X' vs 'c++X' is possibly a little outdated.
        # Presumably the intent is 'vc++X' is permissive and 'c++X' is not,
        # but '/permissive-' is the default since 16.8.
        flag = cppstd_msvc_flag(compiler_version, cppstd)
        return 'v%s' % flag if flag else None
    else:
        return f"gnu++{cppstd[3:]}" if cppstd.startswith("gnu") else f"c++{cppstd}"


def to_cstd_flag(recipe: RecipeBase, cstd):
    """Gets a valid cstd flag.
    """
    if cstd is None:
        return None
    if disable_flag(recipe, "cppstd"):
        return None
    return cstd if cstd.startswith("gnu") else f"c{cstd}"
