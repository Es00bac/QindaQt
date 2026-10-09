#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Actual phase orchestrator with injected runner; never imports Portage."""
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from cohort_qualification import COMPILE_PHASES, private_bus_values
from portage_cohort import compile_and_observe, assemble_image
from qualification_fixture import encoded, suite_payloads


class PhaseFixture:
    def __init__(self, root: Path, failed=True):
        self.root = root
        self.recipe = root / "pipewire.ebuild"
        self.recipe.write_text("fixture recipe\n")
        self.build = root / "tmp/portage/media-video/pipewire-1.6.8"
        self.work = self.build / "work/pipewire-1.6.8-abi_x86_64.amd64"
        (self.work / "meson-info").mkdir(parents=True)
        (self.work / "meson-logs").mkdir()
        self.payloads = suite_payloads(root, failed)
        (self.work / "meson-info/intro-tests.json").write_bytes(self.payloads["registry"])
        self.failed = failed
        self.steps = []
        self.calls = []
        self.mode = "unpatched-asan-control"
        self.variant = True
        self.corrupt = None
        self.environment = {**private_bus_values(root), "ENV_UNSET": "DISPLAY",
                            "FEATURES": "-* test nostrip network-sandbox"}

    def record(self, step):
        self.steps.append(step)
        (self.root / "results.json").write_bytes(encoded({
            "mode": self.mode, "upstreamTestVariant": self.variant, "steps": self.steps}))

    def attempt(self, recipe, phases, environment, output, inputs, record, *, name):
        self.calls.append((phases.copy(), environment.copy()))
        log = output / (name + ".log")
        log.write_text(name + " observed\n")
        if phases == COMPILE_PHASES:
            (self.build / ".compiled").touch()
            saved=self.build / "temp/environment"
            saved.parent.mkdir()
            saved.write_text("".join('declare -x '+key+'="'+environment[key]+'"\n'
                for key in ("ENV_UNSET","DBUS_SESSION_BUS_ADDRESS","DBUS_SYSTEM_BUS_ADDRESS")))
        elif phases == ["test"]:
            for key, filename in (("json", "testlog.json"), ("text", "testlog.txt"),
                                  ("junit", "testlog.junit.xml")):
                (self.work / "meson-logs" / filename).write_bytes(self.payloads[key])
            if not self.failed: (self.build / ".tested").touch()
        elif phases == ["install", "package"]:
            if self.corrupt == "image-created-tested": (self.build / ".tested").touch()
        else:
            raise AssertionError("unexpected delegated phase")
        step = {"name": name, "argv": ["/usr/bin/ebuild", str(recipe), *phases],
                "exit": 1 if phases == ["test"] and self.failed else 0,
                "timedOut": self.corrupt == "outer-timeout" and phases == ["test"],
                "leaderReservedThroughCleanup": not (self.corrupt == "cleanup" and phases == COMPILE_PHASES),
                "ownedProcessGroup": 200 + len(self.calls), "log": str(log),
                "sha256": hashlib.sha256(log.read_bytes()).hexdigest()}
        if self.corrupt == "compile-exit" and phases == COMPILE_PHASES: step["exit"] = 1
        record(step)
        (output / "input-final-verification.json").write_bytes(
            encoded({"verified": self.corrupt != "final-inputs"}))
        return step

    def observe(self, allow=True):
        return compile_and_observe(self.recipe, self.environment, self.root, self.build,
            None, self.record, self.mode, self.variant, allow, self.attempt)

    def image(self, value, allow=True):
        return assemble_image(self.recipe, self.environment, self.root, self.build,
            None, self.record, self.mode, value, allow, self.attempt)


class PhaseAdmissionTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="audio-phase-control-")
        self.fixture = PhaseFixture(Path(self.temporary.name))

    def tearDown(self):
        self.temporary.cleanup()

    def test_failed_original_observes_once_then_only_noauto_install_package(self):
        value = self.fixture.observe()
        self.fixture.image(value)
        self.assertEqual([call[0] for call in self.fixture.calls],
                         [COMPILE_PHASES, ["test"], ["install", "package"]])
        self.assertNotIn("noauto", self.fixture.calls[0][1]["FEATURES"])
        self.assertNotIn("noauto", self.fixture.calls[1][1]["FEATURES"])
        self.assertIn("noauto", self.fixture.calls[2][1]["FEATURES"])
        self.assertIn("test", self.fixture.calls[2][1]["FEATURES"].split())
        self.assertFalse((self.fixture.build / ".tested").exists())
        self.assertEqual(value["upstreamTests"], "failed")
        self.assertTrue(value["diagnosticOnly"])

    def test_default_failed_suite_never_dispatches_image(self):
        with self.assertRaises(ValueError): self.fixture.observe(allow=False)
        self.assertEqual(len(self.fixture.calls), 2)
        self.assertFalse((self.fixture.build / ".tested").exists())
        self.assertFalse((self.fixture.root / "upstream-qualification.json").exists())

    def test_compile_failure_never_dispatches_test(self):
        self.fixture.corrupt = "compile-exit"
        with self.assertRaises(ValueError): self.fixture.observe()
        self.assertEqual(len(self.fixture.calls), 1)

    def test_cleanup_uncertainty_never_dispatches_test(self):
        self.fixture.corrupt = "cleanup"
        with self.assertRaises(ValueError): self.fixture.observe()
        self.assertEqual(len(self.fixture.calls), 1)

    def test_outer_timeout_refuses_image_even_with_complete_logs(self):
        self.fixture.corrupt = "outer-timeout"
        with self.assertRaises(ValueError): self.fixture.observe()
        self.assertEqual(len(self.fixture.calls), 2)

    def test_failed_final_input_proof_refuses_image(self):
        self.fixture.corrupt = "final-inputs"
        with self.assertRaises(ValueError): self.fixture.observe()
        self.assertEqual(len(self.fixture.calls), 2)

    def test_changed_pinned_evidence_refuses_before_image_dispatch(self):
        value = self.fixture.observe()
        raw = Path(value["upstreamEvidence"]["raw"]["json"]["path"])
        raw.chmod(0o600)
        raw.write_bytes(b"replacement")
        with self.assertRaises(ValueError): self.fixture.image(value)
        self.assertEqual(len(self.fixture.calls), 2)

    def test_tested_marker_invention_is_detected_after_image_return(self):
        value = self.fixture.observe()
        self.fixture.corrupt = "image-created-tested"
        with self.assertRaises(ValueError): self.fixture.image(value)
        self.assertEqual(len(self.fixture.calls), 3)

    def test_wrong_mode_opt_in_refuses_before_any_dispatch(self):
        self.fixture.mode = "patched-asan"
        with self.assertRaises(ValueError): self.fixture.observe()
        self.assertEqual(self.fixture.calls, [])

    def test_normal_release_keeps_dependency_policy_and_no_test_claim(self):
        self.fixture.mode = "patched-normal"
        self.fixture.variant = False
        self.fixture.environment["FEATURES"] = "-* sandbox usersandbox binpkg-signing"
        value = self.fixture.observe(allow=False)
        self.fixture.image(value, allow=False)
        self.assertEqual([call[0] for call in self.fixture.calls],
                         [COMPILE_PHASES, ["install", "package"]])
        self.assertNotIn("noauto", self.fixture.calls[-1][1]["FEATURES"])
        self.assertEqual(value["upstreamTests"], "not-requested")
        self.assertFalse(value["diagnosticOnly"])

    def test_admitted_green_fixed_suite_keeps_tested_marker_and_noauto_off(self):
        self.fixture.failed = False
        self.fixture.payloads = suite_payloads(self.fixture.root, failed=False)
        self.fixture.mode = "patched-asan"
        value = self.fixture.observe(allow=False)
        self.fixture.image(value, allow=False)
        self.assertTrue((self.fixture.build / ".tested").exists())
        self.assertNotIn("noauto", self.fixture.calls[-1][1]["FEATURES"])
        self.assertEqual(value["upstreamTests"], "passed")


if __name__ == "__main__":
    unittest.main()
