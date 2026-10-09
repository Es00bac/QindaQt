#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real consumer flag admission with tiny files; no ELF/package qualification."""
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from artifact_receipt import ORIGINAL_RECIPE, PATCHED_RECIPE, bootstrap_environment, make_receipt
from evidence_io import record
from qualification_fixture import EvidenceFixture, encoded
from run_experiment import validate_receipt


class ReceiptFlagsTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="audio-receipt-flags-")
        self.root = Path(self.temporary.name)
        self.fixture = EvidenceFixture(self.root)
        self.path = self.root / "cohort.json"

    def tearDown(self):
        self.temporary.cleanup()

    def receipt(self, mode="unpatched-asan-control", status="failed"):
        qualification = self.fixture.qualification(status=status, mode=mode)
        final = self.fixture.put("publication", encoded({"verified": True}))
        artifact = self.fixture.put("artifact", b"tiny fixture, not ELF\n")
        return {"schema": 2, "mode": mode, **qualification, "finalInputVerification": final,
            "imageRoot": str(self.root), "packageVerification": {}, "package": "fixture",
            "overlayCandidate": "", "archiveSha256": "", "moduleSourceSha256": "",
            "recipeSha256": ORIGINAL_RECIPE if mode == "unpatched-asan-control" else PATCHED_RECIPE,
            "patchSha256": "", "moduleDirectory": str(self.root), "spaDirectory": str(self.root),
            "libraryDirectory": str(self.root), "artifacts": {"core": artifact}, "scope": "fixture"}

    def write(self, value):
        self.path.write_bytes(encoded(value))
        self.path.chmod(0o600)

    def test_consumer_default_refuses_failed_upstream_before_bootstrap(self):
        self.write(self.receipt())
        with self.assertRaises(ValueError): validate_receipt(self.path)

    def test_consumer_explicit_old_opt_in_retains_failed_status(self):
        self.write(self.receipt())
        value, _ = validate_receipt(self.path, allow_failed_old=True)
        self.assertTrue(value["diagnosticOnly"])
        self.assertEqual(value["upstreamTests"], "failed")

    def test_unknown_and_legacy_receipt_schema_refuse(self):
        for change in ("unknown", "legacy"):
            value = self.receipt()
            if change == "unknown": value["releaseAllowed"] = True
            else: value["schema"] = 1
            self.write(value)
            with self.assertRaises(ValueError): validate_receipt(self.path, allow_failed_old=True)

    def test_bad_publication_inputs_refuse_even_with_complete_failed_suite(self):
        value = self.receipt()
        value["finalInputVerification"] = self.fixture.put("publication", encoded({"verified": False}))
        self.write(value)
        with self.assertRaises(ValueError): validate_receipt(self.path, allow_failed_old=True)

    def test_bootstrap_requires_flag_and_does_not_create_root_on_denial(self):
        value = self.receipt()
        private = self.root / "native-private"
        with self.assertRaises(ValueError): bootstrap_environment(value, private)
        self.assertFalse(private.exists())
        environment = bootstrap_environment(value, private, allow_failed_old=True)
        self.assertEqual(environment["DBUS_SESSION_BUS_ADDRESS"],
                         f"unix:path={private}/absent-session")
        self.assertEqual(value["upstreamTests"], "failed")

    def test_bootstrap_rechecks_publication_inputs_after_receipt_admission(self):
        value = self.receipt()
        self.write(value)
        validate_receipt(self.path, allow_failed_old=True)
        final = Path(value["finalInputVerification"]["path"])
        final.chmod(0o600)
        final.write_bytes(encoded({"verified": False}))
        private = self.root / "revoked-native"
        with self.assertRaises(ValueError):
            bootstrap_environment(value, private, allow_failed_old=True)
        self.assertFalse(private.exists())

    def test_publication_record_outside_snapshot_refuses(self):
        value = self.receipt()
        outside = self.root / "outside.json"
        outside.write_bytes(encoded({"verified": True}))
        outside.chmod(0o400)
        value["finalInputVerification"] = record(outside)
        self.write(value)
        with self.assertRaises(ValueError): validate_receipt(self.path, allow_failed_old=True)

    def test_fixed_opt_in_refuses_before_environment_composition(self):
        value = self.receipt(mode="patched-asan", status="passed")
        with self.assertRaises(ValueError):
            bootstrap_environment(value, self.root / "fixed", allow_failed_old=True)

    def test_producer_default_denial_precedes_image_reads_or_readelf(self):
        value = self.receipt()
        with patch("artifact_receipt.artifact") as artifact:
            with patch("artifact_receipt.asan_needed") as elf:
                with self.assertRaises(ValueError):
                    make_receipt(self.root, self.root, self.fixture.recipe, self.root / "archive",
                        "unpatched-asan-control", "usr/lib64", None, self.root / "proof",
                        {key: value[key] for key in ("diagnosticOnly", "upstreamTests", "upstreamEvidence")},
                        value["finalInputVerification"])
                artifact.assert_not_called()
                elf.assert_not_called()


if __name__ == "__main__":
    unittest.main()
