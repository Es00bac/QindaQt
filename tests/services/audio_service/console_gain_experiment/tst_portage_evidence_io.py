#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Actual bounded file observations in owned scratch; no Portage or audio."""
import os
from pathlib import Path
import tempfile
import unittest
from evidence_io import exact_record, read_evidence, record, snapshot


class EvidenceIoTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="audio-evidence-")
        self.root = Path(self.temporary.name)
        self.file = self.root / "evidence"
        self.file.write_bytes(b"raw evidence\n")
        self.file.chmod(0o400)

    def tearDown(self):
        self.temporary.cleanup()

    def test_actual_snapshot_retains_bytes_without_shared_inode(self):
        value = snapshot(self.file, self.root / "snapshot")
        self.assertEqual(exact_record(value), b"raw evidence\n")
        self.assertNotEqual(self.file.stat().st_ino, Path(value["path"]).stat().st_ino)

    def test_fifo_rejects_without_waiting_for_writer(self):
        path = self.root / "fifo"
        os.mkfifo(path)
        with self.assertRaises(ValueError): read_evidence(path)

    def test_symlink_and_hardlink_refuse(self):
        path = self.root / "alias"
        path.symlink_to(self.file)
        with self.assertRaises(OSError): read_evidence(path)
        path.unlink()
        os.link(self.file, path)
        with self.assertRaises(ValueError): record(self.file)

    def test_writable_pinned_file_refuses(self):
        value = record(self.file)
        self.file.chmod(0o660)
        with self.assertRaises(ValueError): exact_record(value)

    def test_snapshot_never_overwrites_existing_file(self):
        target = self.root / "snapshot"
        target.write_bytes(b"preserved")
        with self.assertRaises(FileExistsError): snapshot(self.file, target)
        self.assertEqual(target.read_bytes(), b"preserved")

    def test_unknown_record_and_changed_content_refuse(self):
        value = record(self.file)
        with self.assertRaises(ValueError): exact_record({**value, "approved": True})
        self.file.chmod(0o600)
        self.file.write_bytes(b"different\n")
        with self.assertRaises(ValueError): exact_record(value)


if __name__ == "__main__":
    unittest.main()
