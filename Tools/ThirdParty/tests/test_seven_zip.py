import os
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import py7zr

from thirdparty.files.files import un7zip


def _make_7z(archive_path: str, files: dict[str, str]):
    """Create a .7z at archive_path from a {arcname: content} mapping."""
    with tempfile.TemporaryDirectory() as staging:
        for arcname, content in files.items():
            full = os.path.join(staging, arcname)
            os.makedirs(os.path.dirname(full), exist_ok=True)
            with open(full, "w", encoding="utf-8") as fh:
                fh.write(content)
        with py7zr.SevenZipFile(archive_path, "w") as archive:
            for arcname in files:
                archive.write(os.path.join(staging, arcname), arcname)


class TestUn7zip(unittest.TestCase):
    def test_extract_flat(self):
        with tempfile.TemporaryDirectory() as tmp:
            archive = os.path.join(tmp, "pkg.7z")
            _make_7z(archive, {"a.txt": "alpha", "sub/b.txt": "beta"})
            dest = os.path.join(tmp, "out")
            un7zip(archive, Path(dest))
            self.assertEqual(Path(dest, "a.txt").read_text(), "alpha")
            self.assertEqual(Path(dest, "sub", "b.txt").read_text(), "beta")

    def test_strip_root(self):
        with tempfile.TemporaryDirectory() as tmp:
            archive = os.path.join(tmp, "pkg.7z")
            _make_7z(archive, {"root/a.txt": "alpha", "root/sub/b.txt": "beta"})
            dest = os.path.join(tmp, "out")
            un7zip(archive, Path(dest), strip_root=True)
            # The single top-level "root/" folder is stripped away.
            self.assertEqual(Path(dest, "a.txt").read_text(), "alpha")
            self.assertEqual(Path(dest, "sub", "b.txt").read_text(), "beta")
            self.assertFalse(Path(dest, "root").exists())

    def test_pattern_filter(self):
        with tempfile.TemporaryDirectory() as tmp:
            archive = os.path.join(tmp, "pkg.7z")
            _make_7z(archive, {"keep.h": "h", "skip.cpp": "cpp"})
            dest = os.path.join(tmp, "out")
            un7zip(archive, Path(dest), pattern="*.h")
            self.assertTrue(Path(dest, "keep.h").exists())
            self.assertFalse(Path(dest, "skip.cpp").exists())

    def test_uses_7zip_program_when_available(self):
        def fake_run(args, stdout, stderr, text):
            self.assertEqual(args[:3], ["/usr/bin/7zz", "x", "-y"])
            output_arg = next(arg for arg in args if arg.startswith("-o"))
            Path(output_arg[2:], "from-tool.txt").write_text("ok", encoding="utf-8")
            return mock.Mock(returncode=0, stdout="", stderr="")

        with tempfile.TemporaryDirectory() as tmp:
            archive = os.path.join(tmp, "pkg.7z")
            Path(archive).write_bytes(b"not read by mocked extractors")
            dest = os.path.join(tmp, "out")

            with mock.patch("py7zr.SevenZipFile") as py7zr_mock:
                with mock.patch("thirdparty.files.files.which", return_value="/usr/bin/7zz"):
                    with mock.patch("thirdparty.files.files.subprocess.run", side_effect=fake_run):
                        un7zip(archive, Path(dest))

            py7zr_mock.assert_not_called()
            self.assertEqual(Path(dest, "from-tool.txt").read_text(encoding="utf-8"), "ok")


if __name__ == "__main__":
    unittest.main()
