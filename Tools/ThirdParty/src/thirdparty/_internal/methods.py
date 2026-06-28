
from thirdparty._internal.errors import recipe_exception_formatter
from thirdparty.recipe import RecipeBase


def run_configure_method(recipe: RecipeBase):
    initial_requires_count = len(recipe._requires)

    with recipe_exception_formatter(recipe, "config_options"):
        recipe.config_options()

    # default implementation removes compiler.cstd
    recipe.settings.rm_safe("compiler.cstd")

    with recipe_exception_formatter(recipe, "configure"):
        recipe.configure()

    # Default option handling injected by run_configure: drop fPIC where it does not apply (Windows, or shared/header-only builds) and drop shared for header-only packages.
    # This lets recipes omit manual del self.options.fPIC boilerplate.
    if recipe.settings.get_safe("os") == "Windows" and "fPIC" in recipe.options:
        del recipe.options.fPIC
    if recipe.options.get_safe("header_only"):
        if "fPIC" in recipe.options:
            del recipe.options.fPIC
        if "shared" in recipe.options:
            del recipe.options.shared
    elif recipe.options.get_safe("shared"):
        if "fPIC" in recipe.options:
            del recipe.options.fPIC

    if initial_requires_count != len(recipe._requires):
        recipe.output.warning(
            "Requirements should only be added in the requirements() method, "
            "not configure()/config_options().", warn_tag="deprecated")

    with recipe_exception_formatter(recipe, "requirements"):
        recipe.requirements()

    # Register tools implied by the build-system helpers the recipe imports (e.g. CMake ->
    # "cmake"), after requirements() so explicit declarations win. Skip the recipe's own name
    # and anything already declared as a tool.
    own = recipe.name
    existing = {r.name for r in recipe._requires if r.build}
    for tool in getattr(type(recipe), "_implicit_requires_tool", ()):
        if tool != own and tool not in existing:
            recipe.requires_tool(tool)
