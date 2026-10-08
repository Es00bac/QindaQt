import hashlib
import os
from pathlib import Path
import tempfile
import time
import unittest
from unittest.mock import patch

from copy_stage import DIRECTORY, copy_objects, parent_fd, read_regular, vdb_check
from stage_inventory import guest_target


class CopyStage(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.source = self.root/"source"; self.source.mkdir()
        self.stage = self.root/"stage"; self.stage.mkdir()
        (self.source/"usr/bin").mkdir(parents=True)
        (self.stage/"usr").mkdir()
        self.source_fd = os.open(self.source, DIRECTORY)
        self.stage_fd = os.open(self.stage, DIRECTORY)
        self.expected = {"usr": {"kind": "directory", "mode": 0o755, "input": "overlay"}}
        self.packages = {"fixture/public-1": {}}
        self.path = self.source/"usr/bin/app"; self.path.write_bytes(b"exact")
        self.path.chmod(0o755)
        md5 = hashlib.md5(b"exact", usedforsecurity=False).hexdigest()
        self.record = {"kind": "file", "sourceMode": 0o755, "stageMode": 0o755,
                       "size": 5, "sha256": hashlib.sha256(b"exact").hexdigest(),
                       "md5": md5, "owners": [{"cpv": "fixture/public-1",
                                               "kind": "file", "md5": md5}]}

    def tearDown(self):
        os.close(self.source_fd); os.close(self.stage_fd); self.temp.cleanup()

    def copy(self, records=None, deadline=None):
        return copy_objects(self.source_fd, self.stage_fd,
                            records or {"/usr/bin/app": self.record},
                            self.packages, self.expected, os.getuid(),
                            time.monotonic()+5 if deadline is None else deadline)

    def test_exact_copy_is_new_regular_inode_and_bound_expected_digest(self):
        os.link(self.path, self.source/"usr/bin/alias")
        self.assertEqual(self.copy(), 5)
        output = self.stage/"usr/bin/app"
        self.assertEqual(output.read_bytes(), b"exact")
        self.assertNotEqual(output.stat().st_ino, self.path.stat().st_ino)
        self.assertEqual(output.stat().st_nlink, 1)
        self.assertEqual(self.expected["usr/bin/app"]["sha256"], self.record["sha256"])

    def test_growth_truncation_same_size_mutation_refuse(self):
        for value in (b"exact-growth", b"tiny", b"other"):
            with self.subTest(value=value):
                self.path.write_bytes(value)
                with self.assertRaises(ValueError): self.copy()
                self.assertFalse((self.stage/"usr/bin/app").exists())

    def test_mutation_after_read_is_detected_before_publication(self):
        original_read = os.read
        changed = False
        def reading(fd, count):
            nonlocal changed
            data = original_read(fd, count)
            if data and not changed:
                changed = True
                self.path.write_bytes(b"other")
            return data
        with patch("copy_stage.os.read", side_effect=reading):
            with self.assertRaises(ValueError): self.copy()
        self.assertTrue(changed)
        self.assertFalse((self.stage/"usr/bin/app").exists())

    def test_failed_output_write_retains_partial_without_deleting_source(self):
        with patch("copy_stage.os.write", side_effect=OSError("synthetic-write")):
            with self.assertRaises(OSError): self.copy()
        self.assertTrue((self.stage/"usr/bin/app").is_file())
        self.assertEqual(self.path.read_bytes(), b"exact")

    def test_source_mode_change_refused(self):
        self.path.chmod(0o700)
        with self.assertRaises(ValueError): self.copy()

    def test_source_leaf_symlink_refused(self):
        self.path.unlink(); self.path.symlink_to("/etc/passwd")
        with self.assertRaises(OSError): self.copy()

    def test_source_fifo_refuses_without_blocking(self):
        self.path.unlink(); os.mkfifo(self.path)
        with self.assertRaises(ValueError): self.copy()

    def test_source_parent_symlink_refused(self):
        (self.source/"usr/bin").rename(self.source/"elsewhere")
        (self.source/"usr/bin").symlink_to("../elsewhere")
        with self.assertRaises(OSError): self.copy()

    def test_destination_parent_symlink_does_not_touch_foreign(self):
        foreign = self.root/"foreign"; foreign.mkdir()
        (self.stage/"usr/bin").symlink_to(foreign)
        with self.assertRaises(OSError): self.copy()
        self.assertEqual(list(foreign.iterdir()), [])

    def test_existing_destination_preserved(self):
        (self.stage/"usr/bin").mkdir()
        output = self.stage/"usr/bin/app"; output.write_bytes(b"foreign")
        with self.assertRaises(FileExistsError): self.copy()
        self.assertEqual(output.read_bytes(), b"foreign")

    def test_expired_deadline_never_creates_output(self):
        with self.assertRaises(TimeoutError): self.copy(deadline=0)
        self.assertFalse((self.stage/"usr/bin").exists())

    def test_unknown_owner_and_bad_vdb_md5_refuse(self):
        self.record["owners"][0]["cpv"] = "other/unknown-1"
        with self.assertRaises(ValueError): self.copy()
        self.record["owners"][0]["cpv"] = "fixture/public-1"
        self.record["owners"][0]["md5"] = "0"*32
        with self.assertRaises(ValueError): self.copy()

    def test_public_path_and_canonical_name_only(self):
        for name in ("/etc/app", "/usr/../etc/app", "/usr//bin/app"):
            with self.subTest(name=name):
                with self.assertRaises(ValueError): self.copy({name: self.record})

    def test_exact_symlink_admitted_guest_rooted(self):
        (self.source/"usr/bin/link").symlink_to("app")
        link = {"kind": "symlink", "target": "app",
                "owners": [{"cpv": "fixture/public-1", "kind": "symlink", "target": "app"}]}
        self.copy({"/usr/bin/app": self.record, "/usr/bin/link": link})
        self.assertEqual(guest_target(self.stage, "usr/bin/link"), self.stage/"usr/bin/app")

    def test_link_target_replacement_refused(self):
        (self.source/"usr/bin/link").symlink_to("foreign")
        link = {"kind": "symlink", "target": "app",
                "owners": [{"cpv": "fixture/public-1", "kind": "symlink", "target": "app"}]}
        with self.assertRaises(ValueError): self.copy({"/usr/bin/link": link})

    def test_only_exact_privilege_exception(self):
        self.path.rename(self.source/"usr/bin/mount")
        (self.source/"usr/bin/mount").chmod(0o4755)
        self.record.update(sourceMode=0o4755, stageMode=0o755)
        self.copy({"/usr/bin/mount": self.record})
        self.assertEqual((self.stage/"usr/bin/mount").stat().st_mode & 0o7777, 0o755)
        os.link(self.source/"usr/bin/mount", self.source/"usr/bin/app")
        with self.assertRaises(ValueError):
            self.copy({"/usr/bin/app": self.record})

    def test_vdb_content_pin_is_checked(self):
        vdb = self.root/"vdb"; (vdb/"fixture/public-1").mkdir(parents=True)
        contents = vdb/"fixture/public-1/CONTENTS"; contents.write_bytes(b"metadata")
        fd = os.open(vdb, DIRECTORY)
        try:
            packages = {"fixture/public-1": {"contentsSha256": hashlib.sha256(b"metadata").hexdigest()}}
            vdb_check(fd, packages, os.getuid())
            contents.write_bytes(b"changed!")
            with self.assertRaises(ValueError): vdb_check(fd, packages, os.getuid())
        finally: os.close(fd)

    def test_generated_collision_refuses(self):
        self.expected["usr/bin/app"] = {"kind": "file"}
        with self.assertRaises(ValueError): self.copy()


if __name__ == "__main__":
    unittest.main()
