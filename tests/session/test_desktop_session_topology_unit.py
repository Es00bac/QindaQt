# SPDX-License-Identifier: GPL-3.0-or-later

from __future__ import annotations

import copy
import unittest

from desktop_session_topology import (
    TopologyContractError,
    desktop_1080p_topology,
    validate_boot_evidence,
)


def valid_evidence() -> dict[str, object]:
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
                "name": "Virtual-1",
                "geometry": {"x": 0, "y": 0, "width": 1920, "height": 1080},
                "scale": 1.0,
            }
        ],
        "generations": {"outputs": "7", "shellVisibility": "7"},
        "inputDevices": [
            {
                "name": "QindaQt Development Input",
                "keyboard": True,
                "pointer": True,
            }
        ],
        "dockSurfaces": [
            {
                "scope": "dock",
                "outputName": "Virtual-1",
                "desiredOutputName": "Virtual-1",
                "mapped": True,
                "committed": True,
            }
        ],
        "applications": [
            {
                "appId": "org.qindaqt.Settings",
                "windowTitle": "QindaQt Settings",
                "mapped": True,
            },
            {
                "appId": "org.qindaqt.TextEditor",
                "windowTitle": "QindaQt Text Editor",
                "mapped": True,
            },
        ],
        "cleanup": {"bounded": True, "survivorPids": []},
    }


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

    def test_dock_cannot_be_inferred_from_an_ordinary_window(self) -> None:
        evidence = valid_evidence()
        evidence["dockSurfaces"] = []
        with self.assertRaisesRegex(TopologyContractError, "dock surface"):
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


if __name__ == "__main__":
    unittest.main()
