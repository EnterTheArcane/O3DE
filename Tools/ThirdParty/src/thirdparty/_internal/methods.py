import os

from thirdparty._internal.output import Output
from thirdparty.errors import RecipeException
from thirdparty._internal.errors import recipe_exception_formatter, recipe_remove_attr
from thirdparty._internal.paths import PACKAGE_INFO
from thirdparty._internal.model.manifest import FileTreeManifest
from thirdparty._internal.model.refs import PkgReference
from thirdparty._internal.model.pkg_type import PackageType
from thirdparty._internal.model.requires import BuildRequirements, TestRequirements, ToolRequirements
from thirdparty._internal.util.files import mkdir, chdir, save


def run_source_method(recipe, hook_manager):
    mkdir(recipe.source_folder)
    with chdir(recipe.source_folder):
        hook_manager.execute("pre_source", recipe=recipe)
        if hasattr(recipe, "source"):
            recipe.output.highlight("Calling source() in {}".format(recipe.source_folder))
            with recipe_exception_formatter(recipe, "source"):
                with recipe_remove_attr(recipe, ['info', 'settings', "options"], "source"):
                    recipe.source()
        hook_manager.execute("post_source", recipe=recipe)


def run_build_method(recipe, hook_manager):
    if os.path.isfile(recipe.build_folder):
        raise RecipeException(f"{recipe}: Failed to create build folder, there is already a file "
                             f"named: {recipe.build_folder}")
    mkdir(recipe.build_folder)
    mkdir(recipe.package_metadata_folder)
    with chdir(recipe.build_folder):
        hook_manager.execute("pre_build", recipe=recipe)
        if hasattr(recipe, "build"):
            recipe.output.highlight("Calling build()")
            with recipe_exception_formatter(recipe, "build"):
                try:
                    recipe.build()
                except Exception:
                    hook_manager.execute("post_build_fail", recipe=recipe)
                    raise
        hook_manager.execute("post_build", recipe=recipe)


def run_package_method(recipe, package_id, hook_manager, ref):
    """ calls the recipe "package()" method
    - Assigns folders to recipe.package_folder, source_folder, install_folder, build_folder
    - Calls pre-post package hook
    """

    if recipe.package_folder == recipe.build_folder:
        raise RecipeException("Cannot 'recipe package' to the build folder. "
                             "--build-folder and package folder can't be the same")

    mkdir(recipe.package_folder)
    scoped_output = recipe.output
    # Make the copy of all the patterns
    scoped_output.info("Generating the package")
    scoped_output.info("Packaging in folder %s" % recipe.package_folder)

    hook_manager.execute("pre_package", recipe=recipe)
    if hasattr(recipe, "package"):
        scoped_output.highlight("Calling package()")
        with recipe_exception_formatter(recipe, "package"):
            with chdir(recipe.build_folder):
                with recipe_remove_attr(recipe, ['info'], "package"):
                    recipe.package()
    hook_manager.execute("post_package", recipe=recipe)

    save(os.path.join(recipe.package_folder, PACKAGE_INFO), recipe.info.dumps())
    manifest = FileTreeManifest.create(recipe.package_folder)
    manifest.save(recipe.package_folder)
    package_output = Output(scope="%s: package()" % scoped_output.scope)
    manifest.report_summary(package_output, "Packaged")

    prev = manifest.summary_hash
    scoped_output.info("Created package revision %s" % prev)
    pref = PkgReference(ref, package_id)
    scoped_output.success("Package '%s' created" % package_id)
    scoped_output.success("Full package reference: {}".format(pref.repr_notime()))
    return prev


def run_configure_method(recipe, down_options, profile_options, ref):
    """ Run all the config-related functions for the given recipe object """

    initial_requires_count = len(recipe.requires)

    if hasattr(recipe, "config_options"):
        with recipe_exception_formatter(recipe, "config_options"):
            recipe.config_options()
    elif "auto_shared_fpic" in recipe.implements:
        auto_shared_fpic_config_options(recipe)

    auto_language(recipe)  # default implementation removes `compiler.cstd`

    # Assign only the current package options values, but none of the dependencies
    is_consumer = recipe._is_consumer_recipe  # noqa
    recipe.options.apply_downstream(down_options, profile_options, ref, is_consumer)

    if hasattr(recipe, "configure"):
        with recipe_exception_formatter(recipe, "configure"):
            recipe.configure()
    elif "auto_shared_fpic" in recipe.implements:
        auto_shared_fpic_configure(recipe)

    if initial_requires_count != len(recipe.requires):
        recipe.output.warning("Requirements should only be added in the requirements()/"
                                 "build_requirements() methods, not configure()/config_options(), "
                                 "which might raise errors in the future.", warn_tag="deprecated")

    result = recipe.options.get_upstream_options(down_options, ref, is_consumer)
    self_options, up_options, private_up_options = result
    # self_options are the minimum to reproduce state, as defined from downstream (not profile)
    recipe.self_options = self_options
    # up_options are the minimal options that should be propagated to dependencies
    recipe.up_options = up_options
    recipe.private_up_options = private_up_options

    PackageType.compute_package_type(recipe)

    recipe.build_requires = BuildRequirements(recipe.requires)
    recipe.test_requires = TestRequirements(recipe.requires)
    recipe.tool_requires = ToolRequirements(recipe.requires)

    if hasattr(recipe, "requirements"):
        with recipe_exception_formatter(recipe, "requirements"):
            recipe.requirements()

    if hasattr(recipe, "build_requirements"):
        with recipe_exception_formatter(recipe, "build_requirements"):
            recipe.build_requirements()

    if recipe.build_requires._called:  # noqa
        recipe.output.warning(
            "build_requires is deprecated, prefer to use tool_requires with correct traits",
            warn_tag="deprecated")


def auto_shared_fpic_config_options(recipe):
    if recipe.settings.get_safe("os") == "Windows":
        recipe.options.rm_safe("fPIC")


def auto_shared_fpic_configure(recipe):
    if recipe.options.get_safe("header_only"):
        recipe.options.rm_safe("fPIC")
        recipe.options.rm_safe("shared")
    elif recipe.options.get_safe("shared"):
        recipe.options.rm_safe("fPIC")


def auto_header_only_package_id(recipe):
    if recipe.options.get_safe("header_only") or recipe.package_type is PackageType.HEADER:
        recipe.info.clear()


def auto_language(recipe):
    if not recipe.languages:
        recipe.settings.rm_safe("compiler.cstd")
        return
    if "C" not in recipe.languages:
        recipe.settings.rm_safe("compiler.cstd")
    if "C++" not in recipe.languages:
        recipe.settings.rm_safe("compiler.cppstd")
        recipe.settings.rm_safe("compiler.libcxx")
