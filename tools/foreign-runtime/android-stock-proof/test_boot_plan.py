import tempfile
from pathlib import Path
import unittest
from boot_plan import archive_paths
from prepare_overlay import prepare

class BootInputs(unittest.TestCase):
    def test_overlay_contains_only_generated_config_and_exact_sources(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); source = root/"source"; source.mkdir()
            for name in ("guest-init.sh", "guest.py", "windows.py", "scenario.json"):
                (source/name).write_text("fixed synthetic source")
            output = prepare(root/"overlay", source)
            paths = archive_paths(output)
            self.assertIn("./proof/windows.py", paths)
            self.assertFalse((output/"etc/shadow").exists())
            self.assertFalse((output/"etc/machine-id").exists())
            self.assertEqual((output/"bin").readlink(), Path("usr/bin"))
    def test_existing_output_refused(self):
        with tempfile.TemporaryDirectory() as temp:
            with self.assertRaises(FileExistsError): prepare(Path(temp), Path(temp))
    def test_unexpected_root_entry_refused(self):
        with tempfile.TemporaryDirectory() as temp:
            (Path(temp)/"host-home").mkdir()
            with self.assertRaises(ValueError): archive_paths(Path(temp))
    def test_symlink_not_followed(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)/"root"; root.mkdir()
            (root/"usr").mkdir()
            (root/"usr/outside").symlink_to("/nonexistent")
            self.assertEqual(archive_paths(root), [".", "./usr", "./usr/outside"])

if __name__ == "__main__": unittest.main()
