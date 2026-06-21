"""Compatibility exports for the small remaining public API surface."""

from thirdparty._internal.api.output import (
    Color,
    Output,
    TimedOutput,
    LEVEL_DEBUG,
    LEVEL_QUIET,
)
from thirdparty._internal.api.model import (
    RecipeReference,
    PkgReference,
)

__all__ = [
    "Color",
    "Output",
    "TimedOutput",
    "LEVEL_DEBUG",
    "LEVEL_QUIET",
    "RecipeReference",
    "PkgReference",
]
