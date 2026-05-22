from __future__ import annotations

from packaging.version import Version as _PkgVersion  # type: ignore[import-untyped]


class Version:
    """Thin wrapper around :class:`packaging.version.Version` that also accepts
    plain strings on the right-hand side of comparison operators.

    Usage (mirrors Conan's Version class)::

        if Version(self.version) >= "2.0":
            ...
    """

    def __init__(self, s: str | Version) -> None:
        self._raw = str(s)
        self._v = _PkgVersion(self._raw)

    # ------------------------------------------------------------------
    # Comparison operators
    # ------------------------------------------------------------------

    def _coerce(self, other: object) -> _PkgVersion:
        if isinstance(other, Version):
            return other._v
        return _PkgVersion(str(other))

    def __eq__(self, other: object) -> bool:
        try:
            return self._v == self._coerce(other)
        except Exception:
            return NotImplemented  # type: ignore[return-value]

    def __lt__(self, other: object) -> bool:
        return self._v < self._coerce(other)

    def __le__(self, other: object) -> bool:
        return self._v <= self._coerce(other)

    def __gt__(self, other: object) -> bool:
        return self._v > self._coerce(other)

    def __ge__(self, other: object) -> bool:
        return self._v >= self._coerce(other)

    def __hash__(self) -> int:
        return hash(self._v)

    def __str__(self) -> str:
        return self._raw

    def __repr__(self) -> str:
        return f"Version({self._raw!r})"

    # ------------------------------------------------------------------
    # Attribute helpers (Conan compatibility: Version("1.2.3").major etc.)
    # ------------------------------------------------------------------

    @property
    def major(self) -> int:
        return self._v.major

    @property
    def minor(self) -> int:
        return self._v.minor

    @property
    def micro(self) -> int:
        return self._v.micro
