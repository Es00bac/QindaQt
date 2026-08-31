# SPDX-License-Identifier: GPL-3.0-or-later
"""Hostile deadline and archive tests for desktop readiness probing."""

from __future__ import annotations

import json
import unittest
from io import StringIO
from pathlib import Path

from desktop_session_readiness import (
    MARKER,
    PROBE_LIFETIME_SECONDS,
    ReadinessDeadlineExpired,
    _notification_shell_pending,
    _snapshot_pending,
    await_complete_snapshot,
    parse_and_archive_probe,
    require_probe_lifetime,
)
from test_desktop_session_topology_unit import ready_probe


class ReadinessProbeTests(unittest.TestCase):
    def test_archived_ready_snapshot_matches_producer_schema(self) -> None:
        fixture = Path(__file__).parent / "fixtures/desktop_session/probe-ready-1080p.json"
        snapshot = json.loads(fixture.read_text(encoding="utf-8"))
        self.assertEqual(
            await_complete_snapshot(lambda _: snapshot, seconds=1), snapshot
        )

    def test_archived_fallback_snapshot_names_installed_identity_gap(self) -> None:
        fixture = (
            Path(__file__).parent
            / "fixtures/desktop_session/probe-observed-fallback-1080p.json"
        )
        snapshot = json.loads(fixture.read_text(encoding="utf-8"))
        self.assertEqual(
            _snapshot_pending(snapshot),
            "mapped test application was missing: org.qindaqt.Settings",
        )

    def test_ready_fixture_changes_only_the_fixed_settings_identity(self) -> None:
        fixture_root = Path(__file__).parent / "fixtures/desktop_session"
        observed = json.loads(
            (fixture_root / "probe-observed-fallback-1080p.json").read_text(
                encoding="utf-8"
            )
        )
        ready = json.loads(
            (fixture_root / "probe-ready-1080p.json").read_text(encoding="utf-8")
        )
        settings = next(
            window
            for window in observed["windows"]["windows"]
            if window["applicationId"] == "qindaqt-settings"
        )
        settings["applicationId"] = "org.qindaqt.Settings"
        self.assertEqual(observed, ready)

    def test_near_outer_deadline_never_shrinks_probe_lifetime(self) -> None:
        self.assertEqual(
            require_probe_lifetime(PROBE_LIFETIME_SECONDS),
            PROBE_LIFETIME_SECONDS,
        )
        with self.assertRaisesRegex(
            ReadinessDeadlineExpired, "no complete probe lifetime"
        ):
            require_probe_lifetime(PROBE_LIFETIME_SECONDS - 0.001)

    def test_budget_expiry_reports_last_failed_observation(self) -> None:
        pending = ready_probe()
        pending["windows"]["windows"] = []  # type: ignore[index]
        calls = 0

        def sample(_: float) -> dict[str, object]:
            nonlocal calls
            calls += 1
            if calls == 1:
                return pending
            raise ReadinessDeadlineExpired("no complete probe lifetime remains")

        with self.assertRaisesRegex(
            RuntimeError,
            "mapped test application was missing: org.qindaqt.Settings; "
            "no complete probe lifetime remains",
        ):
            await_complete_snapshot(sample, seconds=2, sleep=lambda _: None)
        self.assertEqual(calls, 2)

    def test_cold_boot_service_gap_is_retryable_before_method_validation(self) -> None:
        snapshot = ready_probe()
        snapshot["services"][0] = {  # type: ignore[index]
            "name": "org.qindaqt.Compositor",
            "status": "unavailable",
        }
        for method in (
            "outputs",
            "shellVisibility",
            "inputCapabilities",
            "developmentShellSurfaces",
            "windows",
        ):
            snapshot[method] = {
                "status": "unavailable",
                "failure": {"code": "service-not-ready", "message": method},
            }

        self.assertEqual(
            _snapshot_pending(snapshot),
            "service org.qindaqt.Compositor is not owned yet",
        )

    def test_shell_identity_and_state_mutations_fail_closed(self) -> None:
        mutations = (
            "owner", "service-pid", "shell-pid", "counter", "privacy",
            "pre-open", "center-absent", "visible", "output",
        )
        for mutation in mutations:
            with self.subTest(mutation=mutation):
                snapshot = ready_probe()
                shell = snapshot["notificationShell"]["evidence"]  # type: ignore[index]
                if mutation == "owner":
                    shell["owner"] = "org.attacker.Shell"
                elif mutation == "service-pid":
                    shell["servicePid"] = "999"
                elif mutation == "shell-pid":
                    shell["shellPid"] = "999"
                elif mutation == "counter":
                    shell["centerOpenedCount"] = "01"
                elif mutation == "privacy":
                    shell["presentation"]["privatePresentationAllowed"] = False
                elif mutation == "pre-open":
                    shell["presentation"]["centerOpen"] = True
                elif mutation == "center-absent":
                    shell["centerWindow"] = {"exists": False}
                elif mutation == "visible":
                    shell["centerWindow"]["visible"] = True
                else:
                    shell["centerWindow"]["outputName"] = "WL-9"
                with self.assertRaises(RuntimeError):
                    _snapshot_pending(snapshot)

    def test_shell_pending_codes_and_normalized_evidence_are_exact(self) -> None:
        snapshot = ready_probe()
        shell = snapshot["notificationShell"]["evidence"]  # type: ignore[index]
        shell["presentation"]["privatePresentationAllowed"] = False
        snapshot["notificationShell"] = {
            "status": "pending",
            "failure": {"code": "privacy-denied", "message": "privacy denied"},
            "evidence": shell,
        }
        self.assertEqual(_snapshot_pending(snapshot), "privacy denied")

        for mutation in ("missing-evidence", "wrong-code", "stale-service-evidence"):
            with self.subTest(mutation=mutation):
                changed = ready_probe()
                evidence = changed["notificationShell"]["evidence"]  # type: ignore[index]
                evidence["presentation"]["privatePresentationAllowed"] = False
                code = "privacy-denied"
                if mutation == "wrong-code":
                    code = "center-open-pending"
                elif mutation == "stale-service-evidence":
                    code = "snapshot-object-pending"
                changed["notificationShell"] = {
                    "status": "pending",
                    "failure": {"code": code, "message": mutation},
                    **({} if mutation == "missing-evidence" else {"evidence": evidence}),
                }
                with self.assertRaises(RuntimeError):
                    _snapshot_pending(changed)

        cold = ready_probe()
        cold["notificationShell"] = {
            "status": "pending",
            "failure": {
                "code": "snapshot-object-pending",
                "message": "object is not registered",
            },
        }
        self.assertEqual(_snapshot_pending(cold), "object is not registered")

        for code in (
            "public-topology-pending", "output-pending",
            "selected-output-pending", "dock-owner-pending",
        ):
            with self.subTest(contradictory_code=code):
                contradictory = ready_probe()
                contradictory["notificationShell"] = {
                    "status": "pending",
                    "failure": {"code": code, "message": code},
                }
                with self.assertRaisesRegex(RuntimeError, "not retryable"):
                    _snapshot_pending(contradictory)

    def test_readiness_polls_past_pending_shell_privacy(self) -> None:
        pending = ready_probe()
        shell_evidence = pending["notificationShell"]["evidence"]  # type: ignore[index]
        shell_evidence["presentation"]["privatePresentationAllowed"] = False
        pending["notificationShell"] = {
            "status": "pending",
            "failure": {"code": "privacy-denied", "message": "privacy denied"},
            "evidence": shell_evidence,
        }
        snapshots = iter((pending, ready_probe()))
        observed = await_complete_snapshot(lambda _: next(snapshots), seconds=1)
        self.assertEqual(observed, ready_probe())

    def test_shell_top_level_shapes_are_exact(self) -> None:
        for mutation in ("ready-extra", "pending-extra", "pending-null-evidence"):
            with self.subTest(mutation=mutation):
                snapshot = ready_probe()
                if mutation == "ready-extra":
                    snapshot["notificationShell"]["extra"] = True  # type: ignore[index]
                else:
                    snapshot["notificationShell"] = {  # type: ignore[assignment]
                        "status": "pending",
                        "failure": {
                            "code": "snapshot-object-pending",
                            "message": "object not ready",
                        },
                    }
                    snapshot["notificationShell"][  # type: ignore[index]
                        "extra" if mutation == "pending-extra" else "evidence"
                    ] = True if mutation == "pending-extra" else None
                with self.assertRaises(RuntimeError):
                    _snapshot_pending(snapshot)

    def test_valid_pending_shell_samples_preserve_normalized_evidence(self) -> None:
        ready = ready_probe()
        raw = ready["notificationShell"]  # type: ignore[assignment]
        docks = ready["developmentShellSurfaces"]["surfaces"]  # type: ignore[index]
        outputs = ready["outputs"]["outputs"]  # type: ignore[index]
        cases = ("center-window-missing", "center-output-pending")
        for code in cases:
            with self.subTest(code=code):
                evidence = dict(raw["evidence"])  # type: ignore[index]
                evidence["presentation"] = dict(evidence["presentation"])
                if code == "center-window-missing":
                    evidence["centerWindow"] = {"exists": False}
                    case_outputs = outputs
                else:
                    evidence["centerWindow"] = {
                        "exists": True, "visible": False, "outputName": "WL-1",
                    }
                    case_outputs = [*outputs, {"name": "WL-1"}]
                pending = {
                    "status": "pending",
                    "failure": {"code": code, "message": code},
                    "evidence": evidence,
                }
                self.assertEqual(
                    _notification_shell_pending(
                        pending, dock_surfaces=docks, outputs=case_outputs
                    ),
                    code,
                )

    def test_shell_pending_evidence_must_agree_with_its_code(self) -> None:
        for code, mutation in (
            ("center-window-missing", "center-still-exists"),
            ("center-output-pending", "center-on-selected-output"),
            ("privacy-denied", "privacy-allowed"),
        ):
            with self.subTest(code=code):
                snapshot = ready_probe()
                evidence = snapshot["notificationShell"]["evidence"]  # type: ignore[index]
                if code == "center-window-missing":
                    pass
                elif code == "center-output-pending":
                    pass
                else:
                    evidence["presentation"]["privatePresentationAllowed"] = True
                snapshot["notificationShell"] = {
                    "status": "pending",
                    "failure": {"code": code, "message": mutation},
                    "evidence": evidence,
                }
                with self.assertRaises(RuntimeError):
                    _snapshot_pending(snapshot)

    def test_shell_authority_rejects_unsettled_foreign_or_multiple_docks(self) -> None:
        ready = ready_probe()
        raw = ready["notificationShell"]  # type: ignore[assignment]
        outputs = ready["outputs"]["outputs"]  # type: ignore[index]
        base_docks = ready["developmentShellSurfaces"]["surfaces"]  # type: ignore[index]
        for mutation in ("unmapped", "unconverged", "zero-size"):
            with self.subTest(mutation=mutation):
                docks = [dict(base_docks[0])]
                if mutation == "unmapped":
                    docks[0]["mapped"] = False
                elif mutation == "unconverged":
                    docks[0]["desiredOutputName"] = "WL-1"
                    outputs = [*outputs, {"name": "WL-1"}]
                else:
                    docks[0]["geometry"] = {
                        "x": 0, "y": 0, "width": 0, "height": 0,
                    }
                self.assertIsNotNone(
                    _notification_shell_pending(
                        raw, dock_surfaces=docks, outputs=outputs
                    )
                )
                outputs = ready["outputs"]["outputs"]  # type: ignore[index]

        for mutation in ("foreign", "malformed-owner", "multiple-owners"):
            with self.subTest(mutation=mutation):
                docks = [dict(base_docks[0])]
                if mutation == "foreign":
                    docks[0]["outputName"] = "WL-9"
                elif mutation == "malformed-owner":
                    docks[0]["processId"] = "076"
                else:
                    docks.append({**docks[0], "processId": "77"})
                with self.assertRaises(RuntimeError):
                    _notification_shell_pending(
                        raw, dock_surfaces=docks, outputs=outputs
                    )

        split_owners = [dict(base_docks[0])]
        split_owners.append({**split_owners[0], "processId": "77", "mapped": False})
        with self.assertRaises(RuntimeError):
            _notification_shell_pending(
                raw, dock_surfaces=split_owners, outputs=outputs
            )

    def test_shell_zero_geometry_remains_retryable(self) -> None:
        snapshot = ready_probe()
        snapshot["developmentShellSurfaces"]["surfaces"][0]["geometry"] = {  # type: ignore[index]
            "x": 0, "y": 0, "width": 0, "height": 0,
        }
        self.assertEqual(
            _snapshot_pending(snapshot),
            "notification-shell dock geometry is not settled yet",
        )

    def test_observation_is_archived_before_schema_rejection(self) -> None:
        line = MARKER + json.dumps({"schemaVersion": 2}) + "\n"
        log = StringIO()
        with self.assertRaisesRegex(RuntimeError, "invalid document"):
            parse_and_archive_probe(line, log)
        self.assertEqual(log.getvalue(), line)


if __name__ == "__main__":
    unittest.main()
