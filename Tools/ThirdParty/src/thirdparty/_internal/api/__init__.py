"""Compatibility exports for the small remaining public API surface."""

from thirdparty._internal.api.output import (
    Color,
    ConanOutput,
    TimedOutput,
    LEVEL_DEBUG,
    LEVEL_QUIET,
)
from thirdparty._internal.api.model import (
    RecipeReference,
    PkgReference,
    Remote,
    LOCAL_RECIPES_INDEX,
    PackagesList,
    MultiPackagesList,
    ListPattern,
)

__all__ = [
    "Color",
    "ConanOutput",
    "TimedOutput",
    "LEVEL_DEBUG",
    "LEVEL_QUIET",
    "RecipeReference",
    "PkgReference",
    "Remote",
    "LOCAL_RECIPES_INDEX",
    "PackagesList",
    "MultiPackagesList",
    "ListPattern",
]
