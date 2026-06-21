import importlib
import inspect
import os
import traceback

from thirdparty.errors import RecipeException
from thirdparty._internal.cache.home_paths import HomePaths
from thirdparty._internal.errors import recipe_exception_formatter
from thirdparty._internal.util.files import mkdir, chdir


_generators = {"CMakeToolchain": "thirdparty.tools.cmake",
               "CMakeDeps": "thirdparty.tools.cmake",
               "CMakeConfigDeps": "thirdparty.tools.cmake",
               "MesonToolchain": "thirdparty.tools.meson",
               "MSBuildDeps": "thirdparty.tools.microsoft",
               "MSBuildToolchain": "thirdparty.tools.microsoft",
               "NMakeToolchain": "thirdparty.tools.microsoft",
               "NMakeDeps": "thirdparty.tools.microsoft",
               "VCVars": "thirdparty.tools.microsoft",
               "VirtualRunEnv": "thirdparty.tools.env.virtualrunenv",
               "VirtualBuildEnv": "thirdparty.tools.env.virtualbuildenv",
               "AutotoolsDeps": "thirdparty.tools.gnu",
               "AutotoolsToolchain": "thirdparty.tools.gnu",
               "GnuToolchain": "thirdparty.tools.gnu",
               "PkgConfigDeps": "thirdparty.tools.gnu",
               "BazelDeps": "thirdparty.tools.google",
               "BazelToolchain": "thirdparty.tools.google",
               "XcodeDeps": "thirdparty.tools.apple",
               "XcodeToolchain": "thirdparty.tools.apple",
               "PremakeDeps": "thirdparty.tools.premake",
               "PremakeToolchain": "thirdparty.tools.premake",
               "MakeDeps": "thirdparty.tools.gnu",
               "SConsDeps": "thirdparty.tools.scons",
               "QbsDeps": "thirdparty.tools.qbs",
               "QbsProfile": "thirdparty.tools.qbs",
               "ROSEnv": "thirdparty.tools.ros"
               }


def _get_generator_class(generator_name):
    try:
        generator_class = _generators[generator_name]
        # This is identical to import ... form ... in terms of cacheing
    except KeyError as e:
        raise RecipeException(f"Invalid generator '{generator_name}'. "
                             f"Available types: {', '.join(_generators)}") from e
    try:
        return getattr(importlib.import_module(generator_class), generator_name)
    except ImportError as e:
        raise RecipeException("Internal Recipe error: "
                             f"Could not find module {generator_class}") from e
    except AttributeError as e:
        raise RecipeException("Internal Recipe error: "
                             f"Could not find name {generator_name} "
                             f"inside module {generator_class}") from e


def load_cache_generators(path):
    from thirdparty._internal.loader import load_python_file
    result = {}  # Name of the generator: Class
    if not os.path.isdir(path):
        return result
    for f in os.listdir(path):
        if not f.endswith(".py") or f.startswith("_"):
            continue
        full_path = os.path.join(path, f)
        mod, _ = load_python_file(full_path)
        for name, value in inspect.getmembers(mod):
            if inspect.isclass(value) and not name.startswith("_"):
                result[name] = value
    return result


def write_generators(recipe, hook_manager, home_folder, envs_generation=None):
    new_gen_folder = recipe.generators_folder
    _receive_conf(recipe)
    _receive_generators(recipe)

    # TODO: Optimize this, so the global generators are not loaded every call to write_generators
    global_generators = load_cache_generators(HomePaths(home_folder).custom_generators_path)
    hook_manager.execute("pre_generate", recipe=recipe)

    if recipe.generators:
        recipe.output.highlight(f"Writing generators to {new_gen_folder}")
    # generators check that they are not present in the generators field,
    # to avoid duplicates between the generators attribute and the generate() method
    # They would raise an exception here if we don't invalidate the field while we call them
    old_generators = []
    for gen in recipe.generators:
        if gen not in old_generators:
            old_generators.append(gen)
    recipe.generators = []

    for generator_name in old_generators:
        if isinstance(generator_name, str):
            global_generator = global_generators.get(generator_name)
            generator_class = global_generator or _get_generator_class(generator_name)
        else:
            generator_class = generator_name
            generator_name = generator_class.__name__
        assert generator_class
        try:
            generator = generator_class(recipe)
            mkdir(new_gen_folder)
            recipe.output.info(f"Generator '{generator_name}' calling 'generate()'")
            with chdir(new_gen_folder):
                generator.generate()
        except Exception as e:
            # When a generator fails, it is very useful to have the whole stacktrace
            if not isinstance(e, RecipeException):
                recipe.output.error(traceback.format_exc(), error_type="exception")
            raise RecipeException(f"Error in generator '{generator_name}': {str(e)}") from e

    # restore the generators attribute, so it can raise
    # if the user tries to instantiate a generator already present in generators
    recipe.generators = old_generators

    if hasattr(recipe, "generate"):
        recipe.output.highlight("Calling generate()")
        recipe.output.info(f"Generators folder: {new_gen_folder}")
        mkdir(new_gen_folder)
        with chdir(new_gen_folder):
            with recipe_exception_formatter(recipe, "generate"):
                recipe.generate()

    if envs_generation is None:
        if recipe.virtualbuildenv:
            mkdir(new_gen_folder)
            with chdir(new_gen_folder):
                from thirdparty.env.virtualbuildenv import VirtualBuildEnv
                env = VirtualBuildEnv(recipe)
                # TODO: Check length of env.vars().keys() when adding NotEmpty
                env.generate()
        if recipe.virtualrunenv:
            mkdir(new_gen_folder)
            with chdir(new_gen_folder):
                from thirdparty.env import VirtualRunEnv
                env = VirtualRunEnv(recipe)
                env.generate()

    from thirdparty.env.environment import generate_aggregated_env
    generate_aggregated_env(recipe)
    hook_manager.execute("post_generate", recipe=recipe)


def _receive_conf(recipe):
    """  collect conf_info from the immediate build_requires, aggregate it and injects/update
    current conf
    """
    # TODO: Open question 1: Only build_requires can define config?
    # TODO: Only direct build_requires?
    # TODO: Is really the best mechanism to define this info? Better than env-vars?
    # Conf only for first level build_requires
    for build_require in recipe.dependencies.direct_build.values():
        if build_require.conf_info:
            recipe.conf.compose_conf(build_require.conf_info)


def _receive_generators(recipe):
    """  Collect generators_info from the immediate build_requires"""
    for build_req in recipe.dependencies.direct_build.values():
        if build_req.generator_info:
            if not isinstance(build_req.generator_info, list):
                raise RecipeException(f"{build_req} 'generator_info' must be a list")
            names = [c.__name__ if not isinstance(c, str) else c for c in build_req.generator_info]
            recipe.output.warning(f"Tool-require {build_req} adding generators: {names}",
                                     warn_tag="experimental")
            # Generators can be defined as a tuple in recipes, ensure we don't break if so
            recipe.generators = build_req.generator_info + list(recipe.generators)


def relativize_path(path, recipe, placeholder, normalize=True):
    """
    relative path from the "generators_folder" to "path", asuming the root file, like
    recipe_toolchain.cmake will be directly in the "generators_folder"
    """
    base_common_folder = recipe.folders._base_generators # noqa
    if not base_common_folder or not os.path.isabs(base_common_folder):
        return path
    try:
        common_path = os.path.commonpath([path, recipe.generators_folder, base_common_folder])
        if common_path.replace("\\", "/") == base_common_folder.replace("\\", "/"):
            rel_path = os.path.relpath(path, recipe.generators_folder)
            new_path = os.path.join(placeholder, rel_path)
            return new_path.replace("\\", "/") if normalize else new_path
    except ValueError:  # In case the unit in Windows is different, path cannot be made relative
        pass
    return path



