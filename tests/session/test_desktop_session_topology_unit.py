# SPDX-License-Identifier: GPL-3.0-or-later

from __future__ import annotations

import copy
import unittest

from desktop_session_runtime import await_complete_snapshot
from desktop_session_topology import (
    TopologyContractError,
    desktop_1080p_topology,
    observed_applications,
    validate_boot_evidence,
)


def valid_evidence(output_name: str = "Virtual-0") -> dict[str, object]:
    topology = desktop_1080p_topology()
    pids = {item.role: 100 + index for index, item in enumerate(topology.processes)}
    processes = {
        item.role: {
            "pid": pids[item.role],
            "executable": item.executable,
            "parentRole": item.parent_role,
        }
        for item in topology.processes
    }
    services = [
        {
            "name": item.name,
            "owner": f":1.{index + 1}",
            "pid": pids[item.process_role],
            "executable": next(
                process.executable
                for process in topology.processes
                if process.role == item.process_role
            ),
        }
        for index, item in enumerate(topology.services)
    ]
    return {
        "schemaVersion": 1,
        "topology": topology.document(),
        "containment": {
            "mode": "bwrap-pid-network-ipc",
            "hostDisplayReachable": False,
            "hostSessionBusReachable": False,
            "hostInputReachable": False,
        },
        "processes": processes,
        "services": services,
        "outputs": [
            {
                "name": output_name,
                "geometry": {"x": 0, "y": 0, "width": 1920, "height": 1080},
                "scale": 1.0,
            }
        ],
        "visibilityOutputs": [
            {
                "id": output_name,
                "geometry": {"x": 0, "y": 0, "width": 1920, "height": 1080},
                "scale": 1.0,
            }
        ],
        "generations": {"outputs": "7", "shellVisibility": "7"},
        "inputDevices": [
            {
                "name": "QindaQt Development Input",
                "enabled": True,
                "capabilities": ["keyboard", "pointer"],
            }
        ],
        "dockSurfaces": [
            {
                "scope": "dock",
                "processId": str(pids["shell"]),
                "outputName": output_name,
                "desiredOutputName": output_name,
                "mapped": True,
                "committed": True,
            }
        ],
        "applications": [
            {
                "appId": "org.qindaqt.Settings",
                "processRole": "settings-app",
                "windowId": "settings-window",
                "windowTitle": "QindaQt Settings",
                "mapped": True,
            },
            {
                "appId": "org.qindaqt.TextEditor",
                "processRole": "editor-app",
                "windowId": "editor-window",
                "windowTitle": "QindaQt Text Editor",
                "mapped": True,
            },
        ],
        "measurements": {"residentPssKiB": 524288, "ceilingKiB": 1048576},
        "cleanup": {
            "bounded": True,
            "survivorPids": [],
            "terminalPhases": [
                {
                    "role": item.role,
                    "pid": pids[item.role],
                    "processGroup": pids[item.role],
                    "executablePath": f"/opt/qindaqt/bin/{item.executable}",
                    "startTicks": 1000 + index,
                    "terminalPhase": "already-exited" if index == 0 else "term",
                }
                for index, item in enumerate(topology.processes)
            ],
        },
    }


def ready_probe() -> dict[str, object]:
    evidence = valid_evidence()
    return {
        "schemaVersion": 1,
        "services": [
            {
                "name": record["name"],
                "status": "owned",
                "owner": record["owner"],
                "pid": str(record["pid"]),
            }
            for record in evidence["services"]  # type: ignore[union-attr]
        ],
        "outputs": {
            "status": "ok", "outputs": evidence["outputs"],
            "outputGeneration": evidence["generations"]["outputs"],  # type: ignore[index]
        },
        "shellVisibility": {
            "status": "ok",
            "outputGeneration": evidence["generations"]["shellVisibility"],  # type: ignore[index]
            "outputs": evidence["visibilityOutputs"],
        },
        "inputCapabilities": {
            "status": "ok", "devices": evidence["inputDevices"],
        },
        "developmentShellSurfaces": {
            "status": "ok", "surfaces": evidence["dockSurfaces"],
        },
        "windows": {
            "status": "ok",
            "windows": [
                {
                    "id": record["windowId"],
                    "applicationId": record["appId"],
                    "title": record["windowTitle"],
                }
                for record in evidence["applications"]  # type: ignore[union-attr]
            ],
        },
    }


class FakeClock:
    def __init__(self) -> None:
        self.now = 0.0

    def monotonic(self) -> float:
        return self.now

    def sleep(self, seconds: float) -> None:
        self.now += max(seconds, 0.001)


class TopologyTests(unittest.TestCase):
    def test_exact_topology_passes(self) -> None:
        validate_boot_evidence(valid_evidence())

    def test_missing_resident_process_fails(self) -> None:
        evidence = valid_evidence()
        del evidence["processes"]["audio-service"]  # type: ignore[index]
        with self.assertRaisesRegex(TopologyContractError, "process roles"):
            validate_boot_evidence(evidence)

    def test_service_pid_must_match_process(self) -> None:
        evidence = valid_evidence()
        evidence["services"][0]["pid"] = 999  # type: ignore[index]
        with self.assertRaisesRegex(TopologyContractError, "owner PID"):
            validate_boot_evidence(evidence)

    def test_generations_must_be_equal_and_nonzero(self) -> None:
        for generations in (
            {"outputs": "0", "shellVisibility": "0"},
            {"outputs": "4", "shellVisibility": "5"},
            {"outputs": 4, "shellVisibility": "4"},
        ):
            with self.subTest(generations=generations):
                evidence = valid_evidence()
                evidence["generations"] = generations
                with self.assertRaises(TopologyContractError):
                    validate_boot_evidence(evidence)

    def test_input_device_uses_exact_production_capability_shape(self) -> None:
        for mutation in ("legacy-booleans", "disabled", "partial", "extra-device"):
            with self.subTest(mutation=mutation):
                evidence = valid_evidence()
                device = evidence["inputDevices"][0]  # type: ignore[index]
                if mutation == "legacy-booleans":
                    del device["capabilities"]
                    device.update({"keyboard": True, "pointer": True})
                elif mutation == "disabled":
                    device["enabled"] = False
                elif mutation == "partial":
                    device["capabilities"] = ["keyboard"]
                else:
                    evidence["inputDevices"].append(copy.deepcopy(device))  # type: ignore[union-attr]
                with self.assertRaises(TopologyContractError):
                    validate_boot_evidence(evidence)

    def test_dock_cannot_be_inferred_from_an_ordinary_window(self) -> None:
        evidence = valid_evidence()
        evidence["dockSurfaces"] = []
        with self.assertRaisesRegex(TopologyContractError, "dock surface"):
            validate_boot_evidence(evidence)

    def test_shell_owned_dock_process_id_passes(self) -> None:
        evidence = valid_evidence()
        shell_pid = evidence["processes"]["shell"]["pid"]  # type: ignore[index]
        self.assertEqual(
            evidence["dockSurfaces"][0]["processId"], str(shell_pid)  # type: ignore[index]
        )
        validate_boot_evidence(evidence)

    def test_dock_process_id_is_required(self) -> None:
        evidence = valid_evidence()
        del evidence["dockSurfaces"][0]["processId"]  # type: ignore[index]
        with self.assertRaisesRegex(TopologyContractError, "canonical positive"):
            validate_boot_evidence(evidence)

    def test_dock_process_id_rejects_noncanonical_values(self) -> None:
        for process_id in ("0", "-1", 104, True, "0104", "+104", " 104"):
            with self.subTest(process_id=process_id):
                evidence = valid_evidence()
                evidence["dockSurfaces"][0]["processId"] = process_id  # type: ignore[index]
                with self.assertRaisesRegex(TopologyContractError, "canonical positive"):
                    validate_boot_evidence(evidence)

    def test_forged_999999_dock_process_id_is_rejected(self) -> None:
        evidence = valid_evidence()
        evidence["dockSurfaces"][0]["processId"] = "999999"  # type: ignore[index]
        with self.assertRaisesRegex(TopologyContractError, "authenticated shell"):
            validate_boot_evidence(evidence)

    def test_foreign_live_process_cannot_own_the_dock(self) -> None:
        evidence = valid_evidence()
        foreign_pid = evidence["processes"]["editor-app"]["pid"]  # type: ignore[index]
        evidence["dockSurfaces"][0]["processId"] = str(foreign_pid)  # type: ignore[index]
        with self.assertRaisesRegex(TopologyContractError, "authenticated shell"):
            validate_boot_evidence(evidence)

    def test_stale_dock_pid_rejects_an_authenticated_shell_replacement(self) -> None:
        evidence = valid_evidence()
        stale_pid = evidence["dockSurfaces"][0]["processId"]  # type: ignore[index]
        replacement_pid = 999999
        evidence["processes"]["shell"]["pid"] = replacement_pid  # type: ignore[index]
        for record in evidence["cleanup"]["terminalPhases"]:  # type: ignore[index]
            if record["role"] == "shell":
                record["pid"] = replacement_pid
        self.assertNotEqual(stale_pid, str(replacement_pid))
        with self.assertRaisesRegex(TopologyContractError, "authenticated shell"):
            validate_boot_evidence(evidence)

    def test_host_reachability_and_survivors_fail(self) -> None:
        for mutation in ("display", "bus", "input", "survivor"):
            with self.subTest(mutation=mutation):
                evidence = copy.deepcopy(valid_evidence())
                if mutation == "display":
                    evidence["containment"]["hostDisplayReachable"] = True  # type: ignore[index]
                elif mutation == "bus":
                    evidence["containment"]["hostSessionBusReachable"] = True  # type: ignore[index]
                elif mutation == "input":
                    evidence["containment"]["hostInputReachable"] = True  # type: ignore[index]
                else:
                    evidence["cleanup"]["survivorPids"] = [321]  # type: ignore[index]
                with self.assertRaises(TopologyContractError):
                    validate_boot_evidence(evidence)

    def test_matching_titles_with_wrong_application_ids_are_rejected(self) -> None:
        windows = [
            {"id": "fake-settings", "applicationId": "org.attacker.Fake",
             "title": "QindaQt Settings"},
            {"id": "fake-editor", "applicationId": "org.attacker.Other",
             "title": "QindaQt Text Editor"},
        ]
        with self.assertRaisesRegex(TopologyContractError, "org.qindaqt.Settings"):
            observed_applications(windows)

    def test_executable_fallback_is_not_the_installed_settings_identity(self) -> None:
        windows = ready_probe()["windows"]["windows"]  # type: ignore[index]
        windows[0]["applicationId"] = "qindaqt-settings"
        with self.assertRaisesRegex(TopologyContractError, "org.qindaqt.Settings"):
            observed_applications(windows)

    def test_observed_application_identity_and_declared_role_are_preserved(self) -> None:
        windows = ready_probe()["windows"]["windows"]  # type: ignore[index]
        applications = observed_applications(windows)
        self.assertEqual(applications[0]["appId"], "org.qindaqt.Settings")
        self.assertEqual(applications[0]["windowId"], "settings-window")
        self.assertEqual(applications[0]["processRole"], "settings-app")
        evidence = valid_evidence()
        evidence["applications"][0]["processRole"] = "editor-app"  # type: ignore[index]
        with self.assertRaisesRegex(TopologyContractError, "not mapped"):
            validate_boot_evidence(evidence)

    def test_readiness_polls_past_services_then_apps_then_dock(self) -> None:
        service_pending = ready_probe()
        service_pending["services"][0]["status"] = "unavailable"  # type: ignore[index]
        apps_pending = ready_probe()
        apps_pending["windows"]["windows"] = []  # type: ignore[index]
        dock_pending = ready_probe()
        dock_pending["developmentShellSurfaces"]["surfaces"] = []  # type: ignore[index]
        snapshots = iter((service_pending, apps_pending, dock_pending, ready_probe()))
        observed = await_complete_snapshot(lambda _: next(snapshots), seconds=1)
        self.assertEqual(observed, ready_probe())

    def test_readiness_timeout_is_bounded(self) -> None:
        pending = ready_probe()
        pending["windows"]["windows"] = []  # type: ignore[index]
        clock = FakeClock()
        with self.assertRaisesRegex(RuntimeError, "readiness timed out"):
            await_complete_snapshot(
                lambda _: pending, seconds=0.1,
                monotonic=clock.monotonic, sleep=clock.sleep,
            )

    def test_snapshot_that_completes_after_deadline_is_rejected(self) -> None:
        clock = FakeClock()

        def late(_: float) -> dict[str, object]:
            clock.now = 0.2
            return ready_probe()

        with self.assertRaisesRegex(RuntimeError, "readiness timed out"):
            await_complete_snapshot(
                late, seconds=0.1,
                monotonic=clock.monotonic, sleep=clock.sleep,
            )

    def test_public_method_error_fails_without_retry(self) -> None:
        failed = ready_probe()
        failed["outputs"] = {"status": "unavailable"}
        calls = 0

        def sample(_: float) -> dict[str, object]:
            nonlocal calls
            calls += 1
            return failed

        with self.assertRaisesRegex(RuntimeError, "public D-Bus method outputs"):
            await_complete_snapshot(sample, seconds=1)
        self.assertEqual(calls, 1)

    def test_invalid_service_evidence_fails_without_retry(self) -> None:
        failed = ready_probe()
        failed["services"][0]["status"] = "invalid-pid"  # type: ignore[index]
        calls = 0

        def sample(_: float) -> dict[str, object]:
            nonlocal calls
            calls += 1
            return failed

        with self.assertRaisesRegex(RuntimeError, "invalid ownership"):
            await_complete_snapshot(sample, seconds=1)
        self.assertEqual(calls, 1)

    def test_measurement_schema_and_ceiling_are_exact(self) -> None:
        mutations = (
            {},
            {"residentPssKiB": -1, "ceilingKiB": 1048576},
            {"residentPssKiB": "1", "ceilingKiB": 1048576},
            {"residentPssKiB": 1, "ceilingKiB": 999},
            {"residentPssKiB": 1048577, "ceilingKiB": 1048576},
        )
        for measurements in mutations:
            with self.subTest(measurements=measurements):
                evidence = valid_evidence()
                evidence["measurements"] = measurements
                with self.assertRaisesRegex(TopologyContractError, "PSS"):
                    validate_boot_evidence(evidence)

    def test_cleanup_ledger_must_bind_role_identity_and_terminal_phase(self) -> None:
        for mutation in ("missing", "pid", "phase"):
            with self.subTest(mutation=mutation):
                evidence = valid_evidence()
                records = evidence["cleanup"]["terminalPhases"]  # type: ignore[index]
                if mutation == "missing":
                    records.pop()
                elif mutation == "pid":
                    records[0]["pid"] = 999
                else:
                    records[0]["terminalPhase"] = "graceful"
                with self.assertRaises(TopologyContractError):
                    validate_boot_evidence(evidence)


if __name__ == "__main__":
    unittest.main()
