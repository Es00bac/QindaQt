import tempfile
from pathlib import Path
import unittest
from boot_plan import archive_paths
from prepare_overlay import prepare
from stage_inventory import staged_mode

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
            self.assertEqual((output/"usr/sbin").readlink(), Path("bin"))
            self.assertEqual((output/"usr/bin/python3").readlink(), Path("python3.14"))
            self.assertEqual((output/"usr/bin/sh").readlink(), Path("bash"))
            self.assertEqual((output/"usr/bin/awk").readlink(), Path("gawk"))
    def test_guest_only_nobody_without_host_account_copy(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); source = root/"source"; source.mkdir()
            for name in ("guest-init.sh", "guest.py", "windows.py", "scenario.json"):
                (source/name).write_text("fixed synthetic source")
            output = prepare(root/"overlay", source)
            passwd = (output/"etc/passwd").read_text().splitlines()
            self.assertEqual([row.split(":")[0] for row in passwd],
                             ["root", "proof", "nobody"])
            self.assertEqual(passwd[-1].split(":")[2:4], ["65534", "65534"])
            self.assertIn("nobody:x:65534:", (output/"etc/group").read_text())
    def test_only_exact_mount_helpers_normalize_privilege(self):
        for name in ("usr/bin/mount", "usr/bin/umount"):
            self.assertEqual(staged_mode(name, 0o4755), 0o755)
        self.assertEqual(staged_mode("usr/bin/ordinary", 0o755), 0o755)
        for name, mode in (("usr/bin/other", 0o4755), ("usr/bin/mount", 0o6755)):
            with self.assertRaises(ValueError): staged_mode(name, mode)
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); (root/"usr").mkdir()
            binary = root/"usr/unreviewed"; binary.write_text("not executable")
            binary.chmod(0o4755)
            with self.assertRaises(ValueError): archive_paths(root)

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
