import sys
import unittest
from pathlib import Path
from unittest.mock import patch


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from thirdparty._internal.model.settings import Settings
from thirdparty._internal.util import detect as detect_util
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

    def test_detect_settings_defaults_ios_to_latest_device_sdk(self):
        with (
            patch.object(detect_util, "_machine_os", return_value="Mac"),
            patch.object(detect_util, "_machine_arch", return_value="X64"),
            patch.object(detect_util, "_detect_apple_clang_version", return_value="17"),
        ):
            settings = detect_util.detect_settings(target_os="iOS")

        self.assertEqual(settings.get_safe("os"), "iOS")
        self.assertEqual(settings.get_safe("arch"), "ARM")
        self.assertEqual(settings.get_safe("os.sdk"), "iphoneos")
        self.assertIsNone(settings.get_safe("os.sdk_version"))

    def test_detect_settings_defaults_ios_x64_to_simulator_sdk(self):
        with (
            patch.object(detect_util, "_machine_os", return_value="Mac"),
            patch.object(detect_util, "_machine_arch", return_value="ARM"),
            patch.object(detect_util, "_detect_apple_clang_version", return_value="17"),
        ):
            settings = detect_util.detect_settings(target_os="iOS", target_arch="X64")

        self.assertEqual(settings.get_safe("arch"), "X64")
        self.assertEqual(settings.get_safe("os.sdk"), "iphonesimulator")

    def test_detect_platform_tag_uses_ios_device_default(self):
        with patch.object(detect_util, "_machine_arch", return_value="X64"):
            self.assertEqual(detect_util.detect_platform_tag(target_os="iOS"), "ios-arm")


if __name__ == "__main__":
    unittest.main()
