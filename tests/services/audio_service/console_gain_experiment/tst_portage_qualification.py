#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Strict mode/flags/full-suite evidence controls; no package or runtime."""
import json
from pathlib import Path
import tempfile
import unittest

from cohort_qualification import admit_qualification, image_features, preserved_env_unset
from evidence_io import json_value
from qualification_fixture import EvidenceFixture, encoded


class QualificationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="audio-qualification-")
        self.fixture = EvidenceFixture(Path(self.temporary.name))

    def tearDown(self):
        self.temporary.cleanup()

    def reject(self, value, mode="unpatched-asan-control", allow=True):
        with self.assertRaises((ValueError, OSError)):
            admit_qualification(value, mode, allow_failed_old=allow)

    def test_complete_failed_suite_requires_explicit_old_opt_in(self):
        value = self.fixture.qualification()
        self.reject(value, allow=False)
        self.assertIs(admit_qualification(value, self.fixture.mode, allow_failed_old=True), value)

    def test_fixed_asan_failed_suite_remains_refused(self):
        value = self.fixture.qualification(mode="patched-asan")
        self.reject(value, mode="patched-asan", allow=False)
        self.reject(value, mode="patched-asan", allow=True)

    def test_normal_failed_suite_remains_refused(self):
        value = self.fixture.qualification(mode="patched-normal")
        self.reject(value, mode="patched-normal", allow=False)
        self.reject(value, mode="patched-normal", allow=True)

    def test_green_fixed_suite_does_not_need_exception(self):
        value = self.fixture.qualification(status="passed", mode="patched-asan")
        self.assertIs(admit_qualification(value, "patched-asan"), value)

    def test_normal_release_truth_is_not_requested(self):
        value = self.fixture.qualification(status="not-requested", mode="patched-normal")
        self.assertIs(admit_qualification(value, "patched-normal"), value)
        value["upstreamEvidence"]["suiteComplete"] = True
        self.reject(value, mode="patched-normal", allow=False)

    def test_unknown_and_missing_flags_refuse(self):
        for changed in ("add", "remove"):
            value = self.fixture.qualification()
            if changed == "add": value["releaseQualified"] = True
            else: del value["diagnosticOnly"]
            self.reject(value)

    def test_non_boolean_and_contradictory_diagnostic_flag_refuse(self):
        for flag in (False, 1, "true", None):
            value = self.fixture.qualification()
            value["diagnosticOnly"] = flag
            self.reject(value)

    def test_incomplete_json_refuses(self):
        value = self.fixture.qualification()
        raw = Path(value["upstreamEvidence"]["raw"]["json"]["path"]).read_bytes()
        self.fixture.replace(value, "json", b"\n".join(raw.splitlines()[:-1]) + b"\n")
        self.reject(value)

    def test_duplicate_registry_identity_refuses(self):
        value = self.fixture.qualification()
        registry = self.fixture.json_of(value, "registry")
        registry[1] = registry[0]
        self.fixture.replace(value, "registry", encoded(registry))
        self.reject(value)

    def test_duplicate_json_key_refuses(self):
        with self.assertRaises(ValueError):
            json_value(b'{"diagnosticOnly":false,"diagnosticOnly":true}')

    def test_unknown_result_refuses(self):
        value = self.fixture.qualification()
        path = Path(value["upstreamEvidence"]["raw"]["json"]["path"])
        rows = [json.loads(line) for line in path.read_bytes().splitlines()]
        rows[1]["result"] = "EXPECTEDFAIL"
        self.fixture.replace(value, "json", b"".join(encoded(row) for row in rows))
        self.reject(value)

    def test_changed_raw_file_without_repin_refuses(self):
        value = self.fixture.qualification()
        path = Path(value["upstreamEvidence"]["raw"]["testLog"]["path"])
        path.chmod(0o600)
        path.write_bytes(b"replacement")
        self.reject(value)

    def test_unknown_raw_evidence_refuses(self):
        value = self.fixture.qualification()
        value["upstreamEvidence"]["raw"]["fabricatedPass"] = self.fixture.put("extra", b"x")
        self.reject(value)

    def test_contradictory_count_and_exit_refuse(self):
        for changed in ("count", "exit"):
            value = self.fixture.qualification()
            if changed == "count": value["upstreamEvidence"]["counts"]["fail"] = 0
            else: value["upstreamEvidence"]["testExit"] = 0
            self.reject(value)

    def test_full_suite_internal_timeout_is_not_outer_timeout(self):
        value = self.fixture.qualification()
        self.assertEqual(value["upstreamEvidence"]["counts"]["timeout"], 1)
        admit_qualification(value, self.fixture.mode, allow_failed_old=True)
        phases = self.fixture.json_of(value, "phases")
        phases["steps"][1]["timedOut"] = True
        self.fixture.replace(value, "phases", encoded(phases))
        self.reject(value)

    def test_wrong_phase_order_refuses(self):
        value = self.fixture.qualification()
        phases = self.fixture.json_of(value, "phases")
        phases["steps"][0]["argv"][-1] = "install"
        self.fixture.replace(value, "phases", encoded(phases))
        self.reject(value)

    def test_cleanup_uncertainty_refuses(self):
        value = self.fixture.qualification()
        phases = self.fixture.json_of(value, "phases")
        phases["steps"][1]["leaderReservedThroughCleanup"] = False
        self.fixture.replace(value, "phases", encoded(phases))
        self.reject(value)

    def test_failed_final_inputs_refuse(self):
        value = self.fixture.qualification()
        self.fixture.replace(value, "inputsFinal", encoded({"verified": False}))
        self.reject(value)

    def test_each_actual_row_requires_both_private_bus_addresses(self):
        for key in ("DBUS_SESSION_BUS_ADDRESS", "DBUS_SYSTEM_BUS_ADDRESS"):
            value = self.fixture.qualification()
            path = Path(value["upstreamEvidence"]["raw"]["json"]["path"])
            rows = [json.loads(line) for line in path.read_bytes().splitlines()]
            del rows[-1]["env"][key]
            self.fixture.replace(value, "json", b"".join(encoded(row) for row in rows))
            self.reject(value)

    def test_test_env_unknown_bus_or_env_unset_refuses(self):
        for change in ("host-value", "filtered", "malformed"):
            value = self.fixture.qualification()
            path = Path(value["upstreamEvidence"]["raw"]["json"]["path"])
            rows = [json.loads(line) for line in path.read_bytes().splitlines()]
            env = rows[0]["env"]
            if change == "host-value": env["DBUS_SESSION_BUS_ADDRESS"] = "unix:path=/host/bus"
            elif change == "filtered": env["ENV_UNSET"] += " DBUS_SESSION_BUS_ADDRESS"
            else: env["ENV_UNSET"] = 5
            self.fixture.replace(value, "json", b"".join(encoded(row) for row in rows))
            self.reject(value)

    def test_env_unset_preserves_only_local_addresses(self):
        self.assertEqual(preserved_env_unset("DISPLAY DBUS_SESSION_BUS_ADDRESS XAUTHORITY DBUS_SYSTEM_BUS_ADDRESS"),
                         "DISPLAY XAUTHORITY -DBUS_SESSION_BUS_ADDRESS -DBUS_SYSTEM_BUS_ADDRESS")

    def test_junit_or_text_contradiction_refuses(self):
        for key, raw in (("junit", b'<testsuites tests="3" failures="0" errors="0"/>'),
                         ("text", b"test: a\ntest: b\ntest: c\nOk: 3\n")):
            value = self.fixture.qualification()
            self.fixture.replace(value, key, raw)
            self.reject(value)

    def test_noauto_is_old_failed_assembly_only(self):
        failed = self.fixture.qualification()
        self.assertEqual(image_features("-* test nostrip", self.fixture.mode, failed),
                         "-* test nostrip noauto")
        passed = self.fixture.qualification(status="passed", mode="patched-asan")
        self.assertEqual(image_features("-* test sandbox", "patched-asan", passed), "-* test sandbox")
        for features in ("test", "-* test noauto", "-* nostrip", "-* test test-fail-continue"):
            with self.assertRaises(ValueError):
                image_features(features, self.fixture.mode, failed)

    def test_asan_not_requested_and_nonboolean_opt_in_refuse(self):
        value = self.fixture.qualification(status="not-requested", mode="patched-asan")
        self.reject(value, mode="patched-asan", allow=False)
        self.reject(self.fixture.qualification(), allow=1)


if __name__ == "__main__":
    unittest.main()
