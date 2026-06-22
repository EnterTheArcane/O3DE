from thirdparty._internal.errors import recipe_exception_formatter
from thirdparty._internal.model.requires import BuildRequirements, TestRequirements, ToolRequirements


def run_configure_method(recipe):
    """Drive a recipe's configuration and requirement declaration in canonical order.

    config_options() -> default language handling -> configure() -> default auto-fPIC/shared
    handling -> package-type computation -> requirements()/build_requirements().

    This is the single entry point for a recipe's config phase.  It injects the default
    auto-fPIC behavior (``_auto_fpic_configure``) so recipes don't need manual
    ``del self.options.fPIC`` boilerplate.  ``validate()`` is intentionally NOT run here —
    callers handle it (and its ``RecipeInvalidConfiguration``) separately.
    """
    initial_requires_count = len(recipe.requires)

    if hasattr(recipe, "config_options"):
        with recipe_exception_formatter(recipe, "config_options"):
            recipe.config_options()

    auto_language(recipe)  # default implementation removes `compiler.cstd`

    if hasattr(recipe, "configure"):
        with recipe_exception_formatter(recipe, "configure"):
            recipe.configure()

    _auto_fpic_configure(recipe)

    if initial_requires_count != len(recipe.requires):
        recipe.output.warning("Requirements should only be added in the requirements()/"
                              "build_requirements() methods, not configure()/config_options().",
                              warn_tag="deprecated")

    recipe.build_requires = BuildRequirements(recipe.requires)
    recipe.test_requires = TestRequirements(recipe.requires)
    recipe.tool_requires = ToolRequirements(recipe.requires)

    if hasattr(recipe, "requirements"):
        with recipe_exception_formatter(recipe, "requirements"):
            recipe.requirements()

    if hasattr(recipe, "build_requirements"):
        with recipe_exception_formatter(recipe, "build_requirements"):
            recipe.build_requirements()


def _auto_fpic_configure(recipe):
    """Default option handling injected by ``run_configure``: drop ``fPIC`` where it does not
    apply (Windows, or shared/header-only builds) and drop ``shared`` for header-only packages.
    This lets recipes omit manual ``del self.options.fPIC`` boilerplate."""
    if recipe.settings.get_safe("os") == "Windows":
        recipe.options.rm_safe("fPIC")
    if recipe.options.get_safe("header_only"):
        recipe.options.rm_safe("fPIC")
        recipe.options.rm_safe("shared")
    elif recipe.options.get_safe("shared"):
        recipe.options.rm_safe("fPIC")


def auto_shared_fpic_configure(recipe):
    _auto_fpic_configure(recipe)


def auto_language(recipe):
    # This system does not use the ``languages`` attribute; default to removing the C-only
    # ``compiler.cstd`` setting (the historical empty-languages behavior).
    recipe.settings.rm_safe("compiler.cstd")
