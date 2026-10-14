#!/usr/bin/env python3
"""Exercise source-manifest parsing without a Maya SDK or IDE project."""

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

from list_active_cpp_sources import ManifestError, load_manifest  # noqa: E402


class SourceManifestTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="dommy-source-manifest-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name) / "repository"
        self.root.mkdir()
        self.manifest = self.root / "sources.txt"
        for source in ("src/a.cpp", "src/b.cc", "third_party/lib/c.c"):
            path = self.root / source
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("// fixture\n", encoding="utf-8")

    def load(self, content):
        self.manifest.write_text(content, encoding="utf-8")
        return load_manifest(str(self.manifest), str(self.root))

    def test_comments_blank_lines_and_sorting(self):
        self.assertEqual(self.load(
            "# sources\n\nthird_party/lib/c.c\n  # comment\nsrc/b.cc\nsrc/a.cpp\n"
        ), ["src/a.cpp", "src/b.cc", "third_party/lib/c.c"])

    def test_empty_and_duplicate_entries_are_rejected(self):
        for content in ("", "# empty\n", "src/a.cpp\nsrc/a.cpp\n"):
            with self.subTest(content=content), self.assertRaises(ManifestError):
                self.load(content)

    def test_noncanonical_paths_are_rejected(self):
        for path in (
            "/src/a.cpp", "../src/a.cpp", "src/../src/a.cpp",
            "src/./a.cpp", "src//a.cpp", "other/a.cpp", "src/a.h",
            "src/a.cpp ", " src/a.cpp", "src/a.cpp;src/b.cc",
            "$(FASTNOISE_LOCATION)/FastNoise.cpp", "src/${FILE}.cpp",
            "src/a\\.cpp", "src/a\x00.cpp", "src/a.cpp # comment",
        ):
            with self.subTest(path=path), self.assertRaises(ManifestError):
                self.load(path + "\n")

    def test_missing_sources_and_directories_are_rejected(self):
        (self.root / "src/directory.cpp").mkdir()
        for path in ("src/missing.cpp", "src/directory.cpp"):
            with self.subTest(path=path), self.assertRaises(ManifestError):
                self.load(path + "\n")

    def test_symlinks_cannot_escape_repository(self):
        outside = self.root.parent / "outside.cpp"
        outside.write_text("// external\n", encoding="utf-8")
        (self.root / "src/link.cpp").symlink_to(outside)
        with self.assertRaises(ManifestError):
            self.load("src/link.cpp\n")

    def test_symlink_aliases_cannot_duplicate_sources(self):
        (self.root / "src/alias.cpp").symlink_to(self.root / "src/a.cpp")
        with self.assertRaises(ManifestError):
            self.load("src/a.cpp\nsrc/alias.cpp\n")

    def test_cli_emits_only_paths_on_stdout(self):
        self.load("src/a.cpp\n")
        result = subprocess.run(
            [sys.executable, str(ROOT / "tools/list_active_cpp_sources.py"),
             "--manifest", str(self.manifest), "--repository-root", str(self.root),
             "--summary"], capture_output=True, text=True, check=False
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, "src/a.cpp\n")
        self.assertIn("1 unique checked-in repository sources", result.stderr)

    def test_cli_reports_invalid_manifest_without_partial_output(self):
        self.manifest.write_text("src/a.cpp\nsrc/missing.cpp\n", encoding="utf-8")
        result = subprocess.run(
            [sys.executable, str(ROOT / "tools/list_active_cpp_sources.py"),
             "--manifest", str(self.manifest), "--repository-root", str(self.root)],
            capture_output=True, text=True, check=False
        )
        self.assertEqual(result.returncode, 1)
        self.assertEqual(result.stdout, "")
        self.assertIn("active source manifest error:", result.stderr)


if __name__ == "__main__":
    unittest.main()
