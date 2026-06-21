from collections import OrderedDict

from thirdparty._internal.errors import recipe_remove_attr, recipe_exception_formatter
from thirdparty.errors import RecipeException, RecipeInvalidConfiguration
from thirdparty._internal.methods import auto_header_only_package_id
from thirdparty._internal.model.info import (PackageIdInfo, RequirementsInfo, RequirementInfo,
                                       PythonRequiresInfo)
from thirdparty._internal.model.pkg_type import PackageType


def compute_package_id(node, modes, config_version, hook_manager):
    """
    Compute the binary package ID of this node
    """
    recipe = node.recipe
    unknown_mode, non_embed_mode, embed_mode, python_mode, build_mode = modes
    python_requires = getattr(recipe, "python_requires", None)
    if python_requires:
        python_requires = python_requires.info_requires()

    data = OrderedDict()
    build_data = OrderedDict()

    fix_transitive_static = True

    for require, transitive in node.transitive_deps.items():
        dep_node = transitive.node
        require.deduce_package_id_mode(recipe, dep_node,
                                       non_embed_mode, embed_mode, build_mode, unknown_mode,
                                       fix_transitive_static)
        if require.package_id_mode is not None:
            req_info = RequirementInfo(dep_node.pref.ref, dep_node.pref.package_id,
                                       require.package_id_mode)
            if require.build:
                build_data[require] = req_info
            else:
                data[require] = req_info

    if recipe.vendor:  # Make the package_id fully independent of dependencies versions
        data, build_data = OrderedDict(), OrderedDict()  # TODO, cleaner, now minimal diff

    reqs_info = RequirementsInfo(data)
    build_requires_info = RequirementsInfo(build_data)
    python_requires = PythonRequiresInfo(python_requires, python_mode)
    try:
        copied_options = recipe.options.copy_package_id_info_options()
    except RecipeException as e:
        raise RecipeException(f"{recipe}: {e}")

    recipe.info = PackageIdInfo(settings=recipe.settings.copy_package_id_info_settings(),
                               options=copied_options,
                               reqs_info=reqs_info,
                               build_requires_info=build_requires_info,
                               python_requires=python_requires,
                               conf=recipe.conf.copy_package_id_info_conf(),
                               config_version=config_version.copy() if config_version else None)
    recipe.original_info = recipe.info.clone()

    # To account for effect of headers into consumers, like shared/static variability
    # It affects to both embed and not embed, that would imply some "repetition" of the information
    # in embed cases embedding the full package_id, but it is useful to have that info explicit too
    if recipe.package_type and recipe.package_type in [PackageType.SHARED, PackageType.STATIC,
                                                             PackageType.APP]:
        for require, transitive in node.transitive_deps.items():
            if require.headers:
                header_opts = getattr(transitive.node.recipe, "package_id_abi_options", ())
                for pkg_id_option in header_opts:
                    v = getattr(transitive.node.recipe.options, pkg_id_option)
                    setattr(recipe.info.options[f"{transitive.node.name}/*"], pkg_id_option, v)

    run_validate_package_id(recipe, hook_manager)

    if recipe.info.settings_target:
        # settings_target has beed added to recipe package via package_id api
        recipe.original_info.settings_target = recipe.info.settings_target

    info = recipe.info
    node.package_id = info.package_id()


def run_validate_package_id(recipe, hook_manager):
    # IMPORTANT: This validation code must run before calling info.package_id(), to mark "invalid"
    if hasattr(recipe, "validate_build"):
        with recipe_exception_formatter(recipe, "validate_build"):
            with recipe_remove_attr(recipe, ['cpp_info'], "validate_build"):
                try:
                    recipe.validate_build()
                except RecipeInvalidConfiguration as e:
                    # This 'cant_build' will be ignored if we don't have to build the node.
                    recipe.info.cant_build = str(e)

    if hasattr(recipe, "validate") or hook_manager.validate_hook:
        with recipe_exception_formatter(recipe, "validate"):
            with recipe_remove_attr(recipe, ['cpp_info'], "validate"):
                try:
                    if hook_manager.validate_hook:
                        hook_manager.execute("pre_validate", recipe=recipe)
                    if hasattr(recipe, "validate"):
                        recipe.validate()
                    if hook_manager.validate_hook:
                        hook_manager.execute("post_validate", recipe=recipe)
                except RecipeInvalidConfiguration as e:
                    recipe.info.invalid = str(e)

    # Once we are done, call package_id() to narrow and change possible values
    if hasattr(recipe, "package_id"):
        with recipe_exception_formatter(recipe, "package_id"):
            with recipe_remove_attr(recipe, ['cpp_info', 'settings', 'options'], "package_id"):
                recipe.package_id()
    elif "auto_header_only" in recipe.implements:
        auto_header_only_package_id(recipe)
    if hook_manager.post_package_id_hook:
        with recipe_exception_formatter(recipe, "package_id"):
            with recipe_remove_attr(recipe, ['cpp_info', 'settings', 'options'], "package_id"):
                hook_manager.execute("post_package_id", recipe=recipe)

    recipe.info.validate()



