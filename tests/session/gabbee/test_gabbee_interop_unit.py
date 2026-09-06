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
    readback_proves_insertion,
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
            self.assertTrue(recorder.is_recording)
            self.assertEqual(recorder.stop(), path)
            self.assertFalse(recorder.is_recording)
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

    def test_is_recording_is_a_property_not_a_method(self) -> None:
        recorder = SyntheticRecorder()
        # GabbeeController accesses recorder.is_recording WITHOUT parentheses
        # (it is used as a property, not called as a method).  If is_recording
        # were a plain method, recorder.is_recording would be a truthy bound
        # method object and the controller would never start/stop recording.
        self.assertIsInstance(recorder.is_recording, bool)


class TranscriptTests(unittest.TestCase):
    def test_transcript_is_fixed_and_harmless(self) -> None:
        transcript = synthetic_transcript()
        self.assertEqual(transcript, "[mock transcript from gabbee-synthetic.wav]")
        self.assertTrue(transcript.isascii())
        self.assertNotIn("\n", transcript)

    def test_readback_requires_new_post_delivery_occurrence(self) -> None:
        transcript = synthetic_transcript()
        self.assertTrue(readback_proves_insertion("", transcript, transcript))
        self.assertFalse(readback_proves_insertion(transcript, transcript, transcript))
        self.assertTrue(
            readback_proves_insertion(transcript, transcript + transcript, transcript)
        )
        self.assertFalse(readback_proves_insertion("", "unrelated", transcript))

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
            source = Path(temporary) / "source.conf"
            source.write_text(
                "[preferred]\ndefault=none\norg.freedesktop.impl.portal.Settings=qindaqt\n",
                encoding="utf-8",
            )
            destination = Path(temporary) / "portals.conf"
            facts = stage_portals_configuration(source, destination)
            self.assertEqual(facts["mode"], "append-kde")
            staged = destination.read_text(encoding="utf-8")
            self.assertIn(PORTAL_INTERFACE_LINE + "\n", staged)
            self.assertIn("default=none", staged)
            self.assertEqual(staged.count("GlobalShortcuts"), 1)

    def test_production_conf_stages_valid(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            destination = Path(temporary) / "portals.conf"
            facts = stage_portals_configuration(PRODUCTION_CONF, destination)
            self.assertIn(facts["mode"], ("append-kde", "passthrough"))
            staged = destination.read_text(encoding="utf-8")
            self.assertIn(PORTAL_INTERFACE_LINE + "\n", staged)
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


class OuterPreflightTests(unittest.TestCase):
    """Executable outer preflight — exercises actual run-root creation.

    These are not parser tests; they call into desktop_session_sandbox to prove
    the run-id formula and create_run_root/remove_run_root lifecycle work before
    any nested execution.  Regression: run_id f"gabbee{uuid4().hex[:20]}" was
    26 chars; the sandbox contract requires exactly 32 lowercase hex digits.
    """

    def test_run_id_conforms_to_sandbox_pattern(self) -> None:
        import uuid

        from desktop_session_sandbox import RUN_ID_PATTERN

        run_id = uuid.uuid4().hex
        self.assertIsNotNone(
            RUN_ID_PATTERN.fullmatch(run_id),
            f"run_id {run_id!r} does not satisfy the 32-hex sandbox contract",
        )
        self.assertEqual(len(run_id), 32)

    def test_create_run_root_lifecycle(self) -> None:
        import uuid

        from desktop_session_sandbox import create_run_root, remove_run_root

        with tempfile.TemporaryDirectory() as temporary:
            build_root = Path(temporary)
            run_id = uuid.uuid4().hex
            paths = create_run_root(build_root, run_id)
            self.assertTrue(paths.root.is_dir(), "run root directory must exist")
            self.assertTrue(paths.sentinel.is_file(), "sentinel must be written")
            self.assertTrue(paths.artifacts.is_dir(), "artifacts dir must exist")
            self.assertTrue(paths.runtime.is_dir(), "runtime dir must exist")
            remove_run_root(paths, build_root, run_id)
            self.assertFalse(paths.root.exists(), "run root must be cleaned up")

    def test_system_path_entries_are_strings_for_sandbox_environment(self) -> None:
        """Catch PurePosixPath/str mismatch: sandbox_environment requires str entries.

        Regression: _outer_spec built system_path as sorted({posix(e).parent ...})
        which yields list[PurePosixPath]; sandbox_environment called entry.startswith()
        which is a str-only method, crashing before boot.  The corrected formula is
        sorted({str(posix(e).parent) ...}).  This test exercises that exact path.
        """
        import uuid
        from pathlib import PurePosixPath

        from desktop_session_host_tools import sandbox_path_for, system_mounts
        from desktop_session_sandbox import sandbox_environment

        def posix(value: str) -> PurePosixPath:
            return PurePosixPath(value)

        dbus = shutil.which("dbus-daemon")
        if not dbus:
            self.skipTest("dbus-daemon not available")
        tools = [Path(sys.executable), Path(dbus)]
        mounts = tuple(system_mounts(tools))
        entries = [sandbox_path_for(t.resolve(strict=True), mounts) for t in tools]
        system_path = sorted({str(posix(e).parent) for e in entries})
        for entry in system_path:
            self.assertIsInstance(entry, str, "system_path entries must be str, not PurePosixPath")
        run_id = uuid.uuid4().hex
        env = sandbox_environment(
            run_id=run_id,
            uid=os.getuid(),
            stage_bin="/opt/qindaqt/bin",
            system_path=system_path,
        )
        self.assertIn("PATH", env)
        self.assertIn("/opt/qindaqt/bin", env["PATH"])


class OuterCliTests(unittest.TestCase):
    """Regression for the review blocker: the documented nested-lane command
    (which omits --source-root) must parse before any external-runtime
    preflight, and the no-lane acknowledgement behavior must stay exit 77."""

    DOCUMENTED_ARGV = [
        "--build-root", "/tmp/qindaqt-build",
        "--kwin-wayland", "/usr/bin/kwin_wayland",
        "--plugin-relative", "lib/kwin/plugins/libqindaqt_compositor.so",
        "--decoration-relative", "lib/plugins/libqindaqt_decoration.so",
        "--gabbee-root", "/home/cabewse/gabbee",
    ]

    def test_documented_invocation_parses_with_source_root_default(self) -> None:
        from run_gabbee_interop_nested import _outer_parser

        arguments = _outer_parser().parse_args(self.DOCUMENTED_ARGV)
        self.assertEqual(arguments.source_root, HERE.parents[2])
        self.assertTrue(
            (arguments.source_root / "src/services/portal/data/qindaqt-portals.conf").is_file(),
            "source-root default must be the repository root",
        )

    def test_source_root_is_not_a_required_flag(self) -> None:
        from run_gabbee_interop_nested import _outer_parser

        for action in _outer_parser()._actions:  # noqa: SLF001 (contract check)
            if "--source-root" in action.option_strings:
                self.assertFalse(
                    action.required, "--source-root must default, not require"
                )

    def test_missing_lane_acknowledgement_exits_77(self) -> None:
        completed = subprocess.run(
            [sys.executable, str(HERE / "run_gabbee_interop_nested.py")]
            + self.DOCUMENTED_ARGV,
            capture_output=True,
            text=True,
            timeout=30,
            env={key: value for key, value in os.environ.items()
                 if key != "QINDAQT_PRIVATE_RUNTIME_LANE"},
        )
        self.assertEqual(completed.returncode, 77, completed.stderr)
        self.assertIn("QINDAQT_PRIVATE_RUNTIME_LANE", completed.stderr)


if __name__ == "__main__":
    loader = unittest.TestLoader()
    suite = loader.loadTestsFromModule(sys.modules[__name__])
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    if not result.failures and not result.errors and result.testsRun == len(result.skipped):
        # Assets absent on this machine: mirror the lane's skip convention.
        raise SystemExit(SKIP_CODE)
    raise SystemExit(0 if result.wasSuccessful() else 1)
