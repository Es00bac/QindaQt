#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real private filesystem admission failures; never invokes Portage/native code."""
import hashlib
import multiprocessing
import os
from pathlib import Path
import stat
import tempfile
import unittest
from unittest.mock import patch

from artifact_receipt import artifact, MAX_FILE
import portage_inputs
from portage_inputs import _stage_exact_inputs


def fifo_replacement_child(manifest, source, output, digest, expected, connection):
    """Use the actual helper; parent bounds and reaps only this owned child."""
    observe = portage_inputs._observe
    injected = False
    def replace_after_observation(directory, spec, destination=None):
        nonlocal injected
        result = observe(directory, spec, destination)
        if not injected and spec.name == "Manifest":
            injected = True
            manifest.unlink()
            os.mkfifo(manifest, mode=0o644)
        return result
    try:
        with patch("portage_inputs._observe", side_effect=replace_after_observation):
            _stage_exact_inputs(manifest, source, output, os.getgid(), digest, expected)
    except ValueError as error:
        connection.send({"injected": injected, "rejected": str(error)})
    except BaseException:
        connection.send({"injected": injected, "rejected": "unexpected-error"})
    else:
        connection.send({"injected": injected, "rejected": "accepted"})
    finally:
        connection.close()


class PortageInputs(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="audio-input-contract-")
        self.root = Path(self.temporary.name)
        self.source = self.root / "caller-cache"
        self.source.mkdir()
        self.output = self.root / "cohort"
        self.output.mkdir()
        self.data = {"pipewire-1.6.8.tar.bz2": b"tiny-source-fixture",
                     "pipewire-1.6.0-docs.tar.xz": b"tiny-docs-fixture"}
        self.expected = {name: hashlib.sha256(data).hexdigest()
                         for name, data in self.data.items()}
        rows = []
        for name, data in self.data.items():
            (self.source / name).write_bytes(data)
            (self.source / name).chmod(0o644)
            rows.append(f"DIST {name} {len(data)} BLAKE2B "
                        f"{hashlib.blake2b(data).hexdigest()} SHA512 "
                        f"{hashlib.sha512(data).hexdigest()}")
        self.manifest = self.root / "Manifest"
        self.manifest.write_text("\n".join(rows) + "\n")
        self.manifest.chmod(0o644)
        self.manifest_hash = hashlib.sha256(self.manifest.read_bytes()).hexdigest()

    def tearDown(self):
        self.temporary.cleanup()

    def stage(self):
        # Substitute tiny exact byte pins only in this private helper. The
        # actual production stage_inputs wrapper always supplies frozen pins.
        return _stage_exact_inputs(self.manifest, self.source, self.output,
                                   os.getgid(), self.manifest_hash, self.expected)

    def test_writable_caller_cache_is_copied_without_mutation_or_hardlinks(self):
        originals = {}
        for name in self.data:
            path = self.source / name
            path.chmod(0o664)
            info = path.stat()
            originals[name] = (info.st_ino, info.st_mtime_ns, info.st_ctime_ns, info.st_mode)
        staged = self.stage()
        proof = staged.verify()
        self.assertTrue(proof["verified"])
        self.assertEqual(stat.S_IMODE(staged.snapshot.stat().st_mode), 0o700)
        self.assertEqual(stat.S_IMODE(staged.distdir.stat().st_mode), 0o770)
        for name, data in self.data.items():
            source = self.source / name
            protected = staged.snapshot / name
            writable = staged.distdir / name
            info = source.stat()
            self.assertEqual((info.st_ino, info.st_mtime_ns, info.st_ctime_ns, info.st_mode),
                             originals[name])
            self.assertEqual(protected.read_bytes(), data)
            self.assertEqual(writable.read_bytes(), data)
            self.assertEqual(stat.S_IMODE(protected.stat().st_mode), 0o400)
            self.assertEqual(stat.S_IMODE(writable.stat().st_mode), 0o664)
            self.assertEqual(writable.stat().st_gid, os.getgid())
            self.assertEqual(len({source.stat().st_ino, protected.stat().st_ino,
                                  writable.stat().st_ino}), 3)
        self.assertEqual(staged.snapshot.joinpath("Manifest").read_bytes(),
                         self.manifest.read_bytes())
        self.assertFalse(staged.distdir.joinpath("Manifest").exists())

    def test_cache_permission_normalization_does_not_weaken_artifact_admission(self):
        staged = self.stage()
        name = next(iter(self.data))
        protected = staged.snapshot / name
        writable = staged.distdir / name
        self.assertEqual(artifact(protected)["sha256"], self.expected[name])
        with self.assertRaises(ValueError):
            artifact(writable)
        writable.chmod(0o644)
        self.assertTrue(staged.verify()["verified"])
        writable.chmod(0o664)
        self.assertTrue(staged.verify()["verified"])

    def test_changed_cache_bytes_refuse_even_when_snapshot_is_intact(self):
        staged = self.stage()
        name = next(iter(self.data))
        path = staged.distdir / name
        path.write_bytes(b"x" * len(self.data[name]))
        with self.assertRaises(ValueError):
            staged.verify()
        self.assertEqual((staged.snapshot / name).read_bytes(), self.data[name])

    def test_same_byte_snapshot_rewrite_rejects_version_change(self):
        staged = self.stage()
        name = next(iter(self.data))
        path = staged.snapshot / name
        previous = path.stat()
        path.chmod(0o600)
        path.write_bytes(self.data[name])
        path.chmod(0o400)
        os.utime(path, ns=(previous.st_atime_ns, previous.st_mtime_ns + 1))
        self.assertEqual(artifact(path)["sha256"], self.expected[name])
        with self.assertRaises(ValueError):
            staged.verify()

    def test_manifest_copy_replacement_refuses_identical_content(self):
        staged = self.stage()
        path = staged.snapshot / "Manifest"
        body = path.read_bytes()
        path.unlink()
        path.write_bytes(body)
        path.chmod(0o400)
        with self.assertRaises(ValueError):
            staged.verify()

    def test_cache_symlink_cannot_borrow_admitted_snapshot(self):
        staged = self.stage()
        name = next(iter(self.data))
        path = staged.distdir / name
        path.unlink()
        path.symlink_to(staged.snapshot / name)
        with self.assertRaises(OSError):
            staged.verify()

    def test_cache_hardlink_cannot_share_snapshot_inode(self):
        staged = self.stage()
        name = next(iter(self.data))
        path = staged.distdir / name
        path.unlink()
        os.link(staged.snapshot / name, path)
        with self.assertRaises(ValueError):
            staged.verify()

    def test_snapshot_directory_symlink_refuses(self):
        staged = self.stage()
        relocated = self.output / "relocated-inputs"
        staged.snapshot.rename(relocated)
        staged.snapshot.symlink_to(relocated, target_is_directory=True)
        with self.assertRaises(OSError):
            staged.verify()

    def test_changed_manifest_digest_refuses_before_staging(self):
        self.manifest.write_text(self.manifest.read_text() + "\n")
        with self.assertRaises(ValueError):
            self.stage()
        self.assertFalse((self.output / "source-inputs").exists())

    def test_manifest_symlink_is_not_followed(self):
        other = self.root / "other-manifest"
        self.manifest.rename(other)
        self.manifest.symlink_to(other)
        with self.assertRaises(ValueError):
            self.stage()

    def test_bad_source_hash_refuses_without_repairing_caller(self):
        name = next(iter(self.data))
        path = self.source / name
        body = b"x" * len(self.data[name])
        path.write_bytes(body)
        with self.assertRaises(ValueError):
            self.stage()
        self.assertEqual(path.read_bytes(), body)
        self.assertFalse((self.output / "distfiles" / name).exists())

    def test_source_symlink_refuses_without_touching_target(self):
        name = next(iter(self.data))
        path = self.source / name
        outside = self.root / "outside"
        outside.write_bytes(self.data[name])
        path.unlink()
        path.symlink_to(outside)
        with self.assertRaises(OSError):
            self.stage()
        self.assertEqual(outside.read_bytes(), self.data[name])

    def test_source_fifo_refuses_without_blocking_open(self):
        name = next(iter(self.data))
        path = self.source / name
        path.unlink()
        os.mkfifo(path)
        with self.assertRaises(ValueError):
            self.stage()

    def test_same_byte_named_replacement_during_read_is_rejected(self):
        name = next(iter(self.data))
        path = self.source / name
        original_read = os.read
        changed = False
        def replacing_read(descriptor, count):
            nonlocal changed
            chunk = original_read(descriptor, count)
            if not changed and chunk == self.data[name]:
                changed = True
                replacement = self.source / "replacement"
                replacement.write_bytes(self.data[name])
                os.replace(replacement, path)
            return chunk
        with patch("portage_inputs.os.read", side_effect=replacing_read):
            with self.assertRaises(ValueError):
                self.stage()
        self.assertTrue(changed)

    def test_same_inode_rewrite_during_read_is_rejected(self):
        name = next(iter(self.data))
        path = self.source / name
        original_read = os.read
        changed = False
        def rewriting_read(descriptor, count):
            nonlocal changed
            chunk = original_read(descriptor, count)
            if not changed and chunk == self.data[name]:
                changed = True
                before = path.stat()
                path.write_bytes(self.data[name])
                os.utime(path, ns=(before.st_atime_ns, before.st_mtime_ns + 1))
            return chunk
        with patch("portage_inputs.os.read", side_effect=rewriting_read):
            with self.assertRaises(ValueError):
                self.stage()
        self.assertTrue(changed)

    def test_duplicate_manifest_rows_refuse(self):
        row = self.manifest.read_text().splitlines()[0]
        self.manifest.write_text(row + "\n" + row + "\n")
        self.manifest_hash = hashlib.sha256(self.manifest.read_bytes()).hexdigest()
        with self.assertRaises(ValueError):
            self.stage()

    def test_oversize_manifest_row_refuses_without_reading_archive(self):
        rows = self.manifest.read_text().splitlines()
        fields = rows[0].split()
        fields[2] = str(MAX_FILE + 1)
        rows[0] = " ".join(fields)
        self.manifest.write_text("\n".join(rows) + "\n")
        self.manifest_hash = hashlib.sha256(self.manifest.read_bytes()).hexdigest()
        with self.assertRaises(ValueError):
            self.stage()

    def test_manifest_fifo_replacement_between_observation_and_parse_is_bounded(self):
        context = multiprocessing.get_context("fork")
        parent, child = context.Pipe(duplex=False)
        process = context.Process(target=fifo_replacement_child,
            args=(self.manifest, self.source, self.output, self.manifest_hash,
                  self.expected, child))
        process.start()
        child.close()
        try:
            process.join(timeout=3)
            self.assertFalse(process.is_alive(), "Manifest FIFO open blocked")
            self.assertEqual(process.exitcode, 0)
            self.assertTrue(parent.poll(1), "owned child returned no observed result")
            self.assertEqual(parent.recv(), {"injected": True,
                             "rejected": "Manifest changed before parsing"})
        finally:
            # This child starts no descendants. Keep it unreaped while
            # signalling, then join before releasing its private fixture.
            if process.is_alive():
                process.terminate()
                process.join(timeout=3)
            if process.is_alive():
                process.kill()
                process.join(timeout=3)
            parent.close()
            process.close()

    def test_partial_staging_is_not_reused_after_failure(self):
        name = next(iter(self.data))
        path = self.source / name
        path.write_bytes(b"x" * len(self.data[name]))
        with self.assertRaises(ValueError):
            self.stage()
        path.write_bytes(self.data[name])
        with self.assertRaises(FileExistsError):
            self.stage()


if __name__ == "__main__":
    unittest.main()
