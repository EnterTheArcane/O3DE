from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from thirdparty._internal.model.recipe import RecipeBase
else:
    # Runtime placeholders. Annotations are deferred (PEP 649) and never evaluate these.
    RecipeBase = object

__all__ = ["RecipeBase"]
