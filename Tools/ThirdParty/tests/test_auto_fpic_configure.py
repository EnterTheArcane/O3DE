import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
if str(SRC) not in sys.path:
    sys.path.insert(0, str(SRC))

from thirdparty._internal.methods import _auto_fpic_configure as auto_fpic_configure
from thirdparty._internal.methods import auto_shared_fpic_configure


class _FakeSettings:
    def __init__(self, **values):
        self._values = values

    def get_safe(self, name, default=None):
        return self._values.get(name, default)


class _FakeOptions:
    def __init__(self, **values):
        self._values = values
        self.removed = []

    def get_safe(self, name, default=None):
        return self._values.get(name, default)

    def rm_safe(self, name):
        self.removed.append(name)
        self._values.pop(name, None)


class _FakeRecipe:
    def __init__(self, os_name, **options):
        self.settings = _FakeSettings(os=os_name)
        self.options = _FakeOptions(**options)


class AutoFpicConfigureTests(unittest.TestCase):
    def test_windows_static_removes_fpic(self):
        recipe = _FakeRecipe("Windows", fPIC=True, shared=False)

        auto_fpic_configure(recipe)

        self.assertEqual(recipe.options.removed, ["fPIC"])
        self.assertNotIn("fPIC", recipe.options._values)
        self.assertEqual(recipe.options._values["shared"], False)

    def test_non_windows_shared_removes_fpic(self):
        recipe = _FakeRecipe("Linux", fPIC=True, shared=True)

        auto_fpic_configure(recipe)

        self.assertEqual(recipe.options.removed, ["fPIC"])
        self.assertNotIn("fPIC", recipe.options._values)
        self.assertEqual(recipe.options._values["shared"], True)

    def test_non_windows_static_keeps_fpic(self):
        recipe = _FakeRecipe("Linux", fPIC=True, shared=False)

        auto_fpic_configure(recipe)

        self.assertEqual(recipe.options.removed, [])
        self.assertEqual(recipe.options._values["fPIC"], True)
        self.assertEqual(recipe.options._values["shared"], False)

    def test_header_only_removes_fpic_and_shared(self):
        recipe = _FakeRecipe("Linux", fPIC=True, shared=True, header_only=True)

        auto_fpic_configure(recipe)

        self.assertEqual(recipe.options.removed, ["fPIC", "shared"])
        self.assertNotIn("fPIC", recipe.options._values)
        self.assertNotIn("shared", recipe.options._values)
        self.assertEqual(recipe.options._values["header_only"], True)

    def test_internal_wrapper_matches_public_helper(self):
        public_recipe = _FakeRecipe("Windows", fPIC=True, shared=True, header_only=True)
        wrapper_recipe = _FakeRecipe("Windows", fPIC=True, shared=True, header_only=True)

        auto_fpic_configure(public_recipe)
        auto_shared_fpic_configure(wrapper_recipe)

        self.assertEqual(wrapper_recipe.options.removed, public_recipe.options.removed)
        self.assertEqual(wrapper_recipe.options._values, public_recipe.options._values)


if __name__ == "__main__":
    unittest.main()
