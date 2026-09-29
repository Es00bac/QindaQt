# SPDX-License-Identifier: GPL-3.0-or-later
"""ADR-0286: importing owner wallpapers into the bundle, and checking it.

Images are minimal PNG headers: the import and check rules read only the
PNG header, so no image library is needed; the one conversion row runs only
when Pillow is installed.
"""

from __future__ import annotations

import io
import json
import struct
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

import wallpaper_import as wi

try:
    import PIL  # noqa: F401
    HAVE_PILLOW = True
except ImportError:
    HAVE_PILLOW = False


def fake_png(path: Path, width: int, height: int, payload: bytes = b"", color_type: int = 2) -> Path:
    header = struct.pack(">IIBBBBB", width, height, 8, color_type, 0, 0, 0)
    path.write_bytes(wi.PNG_SIGNATURE + struct.pack(">I", 13) + b"IHDR" + header
                     + b"\x00\x00\x00\x00" + payload)
    return path


class WallpaperImportTests(unittest.TestCase):
    def setUp(self) -> None:
        self._temporary = tempfile.TemporaryDirectory()
        self.root = Path(self._temporary.name)
        self.bundle = self.root / "wallpapers"
        self.bundle.mkdir()
        fake_png(self.bundle / "jade-fold.png", 1672, 941, b"jade")
        (self.bundle / wi.ARTWORK).write_text("# Artwork\n\n### jade-fold\n\nExisting.\n",
                                              encoding="utf-8")
        self.sources = self.root / "Pictures"
        self.sources.mkdir()

    def tearDown(self) -> None:
        self._temporary.cleanup()

    def write_list(self, entries: list[dict]) -> Path:
        path = self.root / "list.json"
        path.write_text(json.dumps(entries), encoding="utf-8")
        return path

    def run_tool(self, *arguments: str) -> tuple[int, str, str]:
        out, err = io.StringIO(), io.StringIO()
        with redirect_stdout(out), redirect_stderr(err):
            status = wi.main(["--wallpapers", str(self.bundle), *arguments])
        return status, out.getvalue(), err.getvalue()

    def test_imports_credits_and_keeps_existing_wallpapers(self) -> None:
        first = fake_png(self.sources / "image-gen-4(6).png", 1672, 941, b"harbor")
        twin = fake_png(self.sources / "ChatGPT Image (4).png", 1672, 941, b"harbor")
        bundled_copy = fake_png(self.sources / "copy.png", 1672, 941, b"jade")
        listing = self.write_list([
            {"source": str(first), "title": "Neon Harbor", "description": "Night harbor."},
            {"source": str(twin), "title": "Neon Harbor Again"},
            {"source": str(bundled_copy), "title": "Jade Again"},
        ])
        existing = (self.bundle / "jade-fold.png").read_bytes()
        status, out, err = self.run_tool(str(listing))
        self.assertEqual(status, 0, err)
        # Duplicates by content are skipped, whatever their file names.
        self.assertIn("skip", out)
        self.assertEqual(sorted(path.name for path in self.bundle.glob("*.png")),
                         ["jade-fold.png", "neon-harbor.png"])
        # Already in the bundled format: copied byte for byte.
        self.assertEqual((self.bundle / "neon-harbor.png").read_bytes(), first.read_bytes())
        self.assertEqual((self.bundle / "jade-fold.png").read_bytes(), existing)
        artwork = (self.bundle / wi.ARTWORK).read_text(encoding="utf-8")
        self.assertTrue(artwork.startswith("# Artwork\n\n### jade-fold\n"))
        self.assertIn(wi.COLLECTION_HEADING, artwork)
        self.assertIn("### neon-harbor\n", artwork)
        self.assertIn("Title: Neon Harbor. Credit: Jarrod C, AI-assisted. Night harbor.", artwork)
        self.assertIn(f"source SHA-256 {wi.sha256(first)}", artwork)
        # The recorded hash makes a later re-import of the same file a no-op.
        status, out, _ = self.run_tool(str(self.write_list([
            {"source": str(twin), "title": "Neon Harbor Later"}])))
        self.assertEqual(status, 0)
        self.assertIn("skip", out)
        self.assertEqual(self.run_tool("--check")[0], 0)

    def test_refuses_portrait_square_narrow_and_replacement(self) -> None:
        cases = {
            "portrait": fake_png(self.sources / "portrait.png", 941, 1672, b"p"),
            "square": fake_png(self.sources / "square.png", 1600, 1600, b"s"),
            "narrow": fake_png(self.sources / "narrow.png", 1280, 720, b"n"),
            "Jade Fold": fake_png(self.sources / "replace.png", 1672, 941, b"new jade"),
        }
        for title, source in cases.items():
            with self.subTest(title=title):
                status, _, err = self.run_tool(str(self.write_list(
                    [{"source": str(source), "title": title}])))
                self.assertEqual(status, 1)
                self.assertIn("ERROR", err)
        self.assertEqual(sorted(path.name for path in self.bundle.glob("*.png")), ["jade-fold.png"])

    def test_list_shape_is_strict(self) -> None:
        source = fake_png(self.sources / "a.png", 1672, 941, b"a")
        for entries in ([], [{"source": str(source)}], [{"source": str(source), "title": "  "}],
                        [{"source": str(source), "title": "A", "extra": 1}],
                        [{"source": str(source), "title": "Same"},
                         {"source": str(fake_png(self.sources / "b.png", 1672, 941, b"b")),
                          "title": "same"}]):
            with self.subTest(entries=entries):
                self.assertEqual(self.run_tool(str(self.write_list(entries)))[0], 1)

    def test_dry_run_writes_nothing(self) -> None:
        source = fake_png(self.sources / "a.png", 1672, 941, b"a")
        status, out, _ = self.run_tool("--dry-run", str(self.write_list(
            [{"source": str(source), "title": "Aurora Plain"}])))
        self.assertEqual(status, 0)
        self.assertIn("aurora-plain.png", out)
        self.assertFalse((self.bundle / "aurora-plain.png").exists())

    def test_check_flags_uncredited_badly_named_and_portrait_images(self) -> None:
        fake_png(self.bundle / "Not Slug.png", 1672, 941, b"x")
        fake_png(self.bundle / "tall.png", 941, 1672, b"y")
        status, _, err = self.run_tool("--check")
        self.assertEqual(status, 1)
        self.assertIn("Not Slug.png: the name", err)
        self.assertIn("no '### tall' credit section", err)
        self.assertIn("941x1672", err)

    @unittest.skipUnless(HAVE_PILLOW, "Pillow is not installed")
    def test_other_sizes_are_converted_to_the_bundled_png(self) -> None:
        from PIL import Image
        source = self.sources / "wide.jpg"
        Image.new("RGB", (2048, 1024), (40, 80, 120)).save(source, format="JPEG")
        status, _, err = self.run_tool(str(self.write_list(
            [{"source": str(source), "title": "Wide Sky"}])))
        self.assertEqual(status, 0, err)
        with Image.open(self.bundle / "wide-sky.png") as image:
            self.assertEqual((image.format, image.mode, image.size), ("PNG", "RGB", wi.TARGET_SIZE))


class ShippedBundleTests(unittest.TestCase):
    def test_the_repository_bundle_passes_the_check(self) -> None:
        images, problems = wi.check(wi.DEFAULT_DIRECTORY)
        self.assertEqual(problems, [])
        names = {image.stem for image in images}
        # ADR-0286: the owner's five, added beside the original six.
        self.assertTrue({"horizon-arc", "light-waves", "valley-sunrise", "neon-harbor",
                         "aurora-plain"} <= names)
        self.assertTrue({"compile-club", "ink-tide", "jade-fold", "porcelain-dawn",
                         "qinda-bliss", "qinda-punk"} <= names)


if __name__ == "__main__":
    unittest.main()
