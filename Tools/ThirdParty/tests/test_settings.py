import sys
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from thirdparty._internal.model.settings import Settings
from thirdparty.errors import RecipeException


class SettingsTests(unittest.TestCase):
    def test_arch_rejects_universal_arch_values(self):
        settings = Settings({"arch": ["ARM", "X64"]})

        with self.assertRaisesRegex(RecipeException, "Invalid setting 'ARM\\|X64'"):
            settings.arch = "ARM|X64"

    def test_arch_accepts_single_arch_values(self):
        settings = Settings({"arch": ["ARM", "X64"]})

        settings.arch = "ARM"

        self.assertEqual(settings.arch.value, "ARM")


if __name__ == "__main__":
    unittest.main()
