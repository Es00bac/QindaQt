# SPDX-License-Identifier: GPL-3.0-or-later
"""Deterministic focused tests for the Gabbee interoperability probe assets.

Covers the probe's own contracts (synthetic recorder, portal-configuration
staging, evidence schema, host-endpoint hygiene, injection-free sources) and,
when the Gabbee checkout plus xdg-desktop-portal exist on this machine, the
full private-bus GlobalShortcuts registration/routing chain with Gabbee's
real portal client.  Everything here is host-safe: no display, microphone,
uinput device, or synthetic input of any kind.
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent))

from gabbee_probe_support import (  # noqa: E402
    PORTAL_INTERFACE_LINE,
    PROBE_FORBIDDEN_ENVIRONMENT,
    ProbeContractError,
    ResultDocument,
    SyntheticRecorder,
    PhaseResult,
    gabbee_source_root,
    stage_portals_configuration,
    synthetic_transcript,
    validate_synthetic_wav,
    wav_header_facts,
)

PRODUCTION_CONF = HERE.parents[2] / "src/services/portal/data/qindaqt-portals.conf"
PROBE_SOURCES = ("gabbee_probe_support.py", "gabbee_portal_fake.py", "gabbee_portal_driver.py")

SKIP_CODE = 77


def _chain_assets_available() -> str | None:
    try:
        gabbee_source_root()
    except ProbeContractError as error:
        return str(error)
    if not shutil.which("dbus-daemon"):
        return "dbus-daemon is not installed"
    frontend = shutil.which("xdg-desktop-portal") or next(
        (
            candidate
            for candidate in (
                "/usr/libexec/xdg-desktop-portal",
                "/usr/lib64/libexec/xdg-desktop-portal",
            )
            if Path(candidate).is_file()
        ),
        None,
    )
    if not frontend:
        return "xdg-desktop-portal frontend is not installed"
    return None


class SyntheticRecorderTests(unittest.TestCase):
    def test_writes_fixed_silent_wav(self) -> None:
        recorder = SyntheticRecorder()
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "utterance-01" / "gabbee-synthetic.wav"
            recorder.start(path, None)
            self.assertTrue(recorder.is_recording())
            self.assertEqual(recorder.stop(), path)
            self.assertFalse(recorder.is_recording())
            facts = validate_synthetic_wav(path)
            self.assertEqual(facts["frameRate"], 16_000)
            self.assertEqual(facts["frames"], recorder.fixed_pcm_frames)
            self.assertEqual(path.stat().st_size, 44 + facts["frames"] * 2)

    def test_header_facts_reject_non_riff(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "noise.wav"
            path.write_bytes(b"NOTRIFF")
            with self.assertRaises(ProbeContractError):
                wav_header_facts(path)

    def test_stop_before_start_is_a_contract_error(self) -> None:
        with self.assertRaises(ProbeContractError):
            SyntheticRecorder().stop()

    def test_double_start_is_a_contract_error(self) -> None:
        recorder = SyntheticRecorder()
        with tempfile.TemporaryDirectory() as temporary:
            recorder.start(Path(temporary) / "a.wav", None)
            with self.assertRaises(ProbeContractError):
                recorder.start(Path(temporary) / "b.wav", None)


class TranscriptTests(unittest.TestCase):
    def test_transcript_is_fixed_and_harmless(self) -> None:
        transcript = synthetic_transcript()
        self.assertEqual(transcript, "[mock transcript from gabbee-synthetic.wav]")
        self.assertTrue(transcript.isascii())
        self.assertNotIn("\n", transcript)

    def test_matches_real_gabbee_mock_provider(self) -> None:
        try:
            root = gabbee_source_root()
        except ProbeContractError:
            self.skipTest("Gabbee checkout unavailable")
        environment = dict(os.environ)
        environment["PYTHONPATH"] = os.pathsep.join(
            [str(root / "src"), environment.get("PYTHONPATH", "")]
        )
        interpreter = root / ".venv/bin/python"
        if not interpreter.is_file():
            self.skipTest("Gabbee venv interpreter unavailable")
        completed = subprocess.run(
            [
                str(interpreter),
                "-c",
                "from pathlib import Path; from gabbee.stt.mock import MockSpeechToText;"
                " print(MockSpeechToText().transcribe(Path('gabbee-synthetic.wav')).text)",
            ],
            capture_output=True,
            text=True,
            env=environment,
            timeout=30,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(completed.stdout.strip(), synthetic_transcript())


class PortalConfigurationStagingTests(unittest.TestCase):
    def test_appends_reviewed_kde_routing(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            destination = Path(temporary) / "portals.conf"
            facts = stage_portals_configuration(PRODUCTION_CONF, destination)
            self.assertEqual(facts["mode"], "append-kde")
            staged = destination.read_text(encoding="utf-8")
            self.assertIn(PORTAL_INTERFACE_LINE + "\n", staged)
            self.assertIn("default=none", staged)
            self.assertEqual(staged.count("GlobalShortcuts"), 1)

    def test_passthrough_when_already_integrated(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "portals.conf"
            source.write_text(
                "[preferred]\ndefault=none\n" + PORTAL_INTERFACE_LINE + "\n",
                encoding="utf-8",
            )
            destination = Path(temporary) / "staged.conf"
            facts = stage_portals_configuration(source, destination)
            self.assertEqual(facts["mode"], "passthrough")
            self.assertEqual(
                destination.read_text(encoding="utf-8"), source.read_text(encoding="utf-8")
            )

    def test_rejects_divergent_integrated_routing(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "portals.conf"
            source.write_text(
                "[preferred]\ndefault=none\n"
                "org.freedesktop.impl.portal.GlobalShortcuts=kde;gtk\n",
                encoding="utf-8",
            )
            with self.assertRaises(ProbeContractError):
                stage_portals_configuration(source, Path(temporary) / "staged.conf")

    def test_rejects_unexpected_conf_shape(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "portals.conf"
            source.write_text("not-a-portal-conf\n", encoding="utf-8")
            with self.assertRaises(ProbeContractError):
                stage_portals_configuration(source, Path(temporary) / "staged.conf")


class ResultDocumentTests(unittest.TestCase):
    def test_round_trip_and_outcome(self) -> None:
        document = ResultDocument("run1", "unit")
        document.add(PhaseResult("a", True, "ok", {"x": 1}))
        document.add(PhaseResult("b", False, "no", {}))
        self.assertFalse(document.outcome())
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "result.json"
            document.write(path)
            loaded = ResultDocument.load(path)
            self.assertEqual(len(loaded["phases"]), 2)
            self.assertFalse(loaded["ok"])
            self.assertEqual(loaded["safety"]["stt"], "gabbee-mock (fixed harmless transcript)")

    def test_load_rejects_schema_drift(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "result.json"
            path.write_text('{"schemaVersion": 99, "phases": [], "ok": true}', encoding="utf-8")
            with self.assertRaises(ProbeContractError):
                ResultDocument.load(path)


class EnvironmentHygieneTests(unittest.TestCase):
    def test_forbidden_keys_are_rejected(self) -> None:
        from gabbee_probe_support import probe_child_environment

        with self.assertRaises(ProbeContractError):
            probe_child_environment({"DISPLAY": ":0"}, {})
        with self.assertRaises(ProbeContractError):
            probe_child_environment({}, {"WAYLAND_DISPLAY": "wayland-0"})
        clean = probe_child_environment({"HOME": "/tmp"}, {"GABBEE_STT_PROVIDER": "mock"})
        self.assertEqual(clean["GABBEE_STT_PROVIDER"], "mock")
        self.assertIn("QINDAQT_ALLOW_HOST_UINPUT", PROBE_FORBIDDEN_ENVIRONMENT)

    def test_probe_sources_are_injection_free(self) -> None:
        import ast

        forbidden_identifiers = {"dotool", "InjectTestInput", "uinput", "ydotool", "evdev"}
        for name in PROBE_SOURCES:
            tree = ast.parse((HERE / name).read_text(encoding="utf-8"))
            for node in ast.walk(tree):
                if isinstance(node, ast.Name) and node.id in forbidden_identifiers:
                    self.fail(f"{name} uses forbidden identifier {node.id}")
                if isinstance(node, ast.Attribute) and node.attr in forbidden_identifiers:
                    self.fail(f"{name} uses forbidden attribute {node.attr}")
                if isinstance(node, ast.Constant) and isinstance(node.value, str):
                    if "dev/uinput" in node.value:
                        self.fail(f"{name} references the uinput device node")


class PortalChainTests(unittest.TestCase):
    def test_registration_and_routing_chain(self) -> None:
        reason = _chain_assets_available()
        if reason:
            self.skipTest(reason)
        with tempfile.TemporaryDirectory(prefix="gabbeechain-", dir="/tmp") as temporary:
            run_root = Path(temporary) / "run"
            result_path = Path(temporary) / "result.json"
            completed = subprocess.run(
                [
                    sys.executable,
                    str(HERE / "run_gabbee_portal_chain.py"),
                    "--run-root",
                    str(run_root),
                    "--result-path",
                    str(result_path),
                ],
                capture_output=True,
                text=True,
                timeout=180,
            )
            self.assertEqual(
                completed.returncode, 0, completed.stderr[-4000:] + completed.stdout[-4000:]
            )
            document = ResultDocument.load(result_path)
            phases = {phase["phase"]: phase for phase in document["phases"]}
            self.assertTrue(phases["portal-chain-preflight"]["ok"])
            routing = phases["shortcut-registration-and-routing"]
            self.assertTrue(routing["ok"], routing["evidence"])
            driver = routing["evidence"]["driver"]
            self.assertEqual(driver["registeredIds"], ["command", "push_to_talk"])
            self.assertTrue(driver["activatedRoutedPressed"])
            self.assertTrue(driver["deactivatedRoutedReleased"])
            self.assertTrue(phases["backend-journal"]["ok"])


if __name__ == "__main__":
    loader = unittest.TestLoader()
    suite = loader.loadTestsFromModule(sys.modules[__name__])
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    if not result.failures and not result.errors and result.testsRun == len(result.skipped):
        # Assets absent on this machine: mirror the lane's skip convention.
        raise SystemExit(SKIP_CODE)
    raise SystemExit(0 if result.wasSuccessful() else 1)
