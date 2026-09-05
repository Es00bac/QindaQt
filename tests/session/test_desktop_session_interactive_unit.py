# SPDX-License-Identifier: GPL-3.0-or-later
"""Hostile S2 evidence tests kept separate from the S1 topology matrix."""

from __future__ import annotations

import unittest

from desktop_session_interactive import validate_interactive_evidence
from desktop_session_topology import TopologyContractError, interactive_1080p_topology
from test_desktop_session_topology_unit import valid_evidence


def valid_interactive_evidence() -> dict[str, object]:
    evidence = valid_evidence("WL-0")
    topology = interactive_1080p_topology()
    parent_pid = 99
    evidence["topology"] = topology.document()
    evidence["processes"] = {
        "parent-compositor": {
            "pid": parent_pid, "executable": "weston", "parentRole": None,
        },
        **evidence["processes"],  # type: ignore[arg-type]
    }
    evidence["cleanup"]["terminalPhases"].insert(  # type: ignore[index]
        0,
        {
            "role": "parent-compositor", "pid": parent_pid,
            "processGroup": parent_pid, "executablePath": "/usr/bin/weston",
            "startTicks": 999, "terminalPhase": "term",
        },
    )
    evidence["containment"].update({  # type: ignore[union-attr]
        "parentBackend": "weston-headless-pixman",
        "qindaqtBackend": "kwin-windowed-qpaint",
        "parentWaylandSocket": "qindaqt-parent-wayland",
        "childWaylandSocket": "qindaqt-0123456789ab",
    })
    evidence["inputDevices"] = [
        {
            "id": "input-00000001", "name": "", "enabled": True,
            "busType": 0, "vendorId": 0, "productId": 0,
            "capabilities": ["pointer"],
        },
        {
            "id": "input-00000002", "name": "", "enabled": True,
            "busType": 0, "vendorId": 0, "productId": 0,
            "capabilities": ["keyboard"],
        },
        {
            "id": "input-00000003", "name": "QindaQt Development Input",
            "enabled": True, "busType": 0, "vendorId": 0, "productId": 0,
            "capabilities": ["keyboard", "pointer"],
        },
    ]
    shell_pid = evidence["processes"]["shell"]["pid"]  # type: ignore[index]
    geometry = {"x": 1464, "y": 46, "width": 440, "height": 640}
    evidence["interaction"] = {
        "action": "open-notification-center",
        "deviceId": "qindaqt-development-input",
        "eventCount": 4,
        "activation": {
            "action": "qindaqt_toggle_notification_center",
            "component": "qindaqt-shell",
            "pressed": True,
            "released": True,
        },
        "preInjectionActiveSurfaceCount": 0,
        "shellPresentation": {
            "before": {
                "owner": ":1.20", "servicePid": str(shell_pid),
                "shellPid": str(shell_pid),
                "tokens": {
                    "ready": True, "qstRevision": 1, "generation": "1",
                    "sourceThemeId": "qinda-dark", "backgroundBase": "#171a18",
                },
                "taskList": {"phase": "ready", "generation": "1", "windowCount": 2},
                "presentation": {
                    "privatePresentationAllowed": True, "centerOpen": False,
                },
                "centerOpenedCount": "0",
                "centerWindow": {
                    "exists": True, "visible": False, "outputName": "WL-0",
                },
            },
            "after": {
                "owner": ":1.20", "servicePid": str(shell_pid),
                "shellPid": str(shell_pid),
                "tokens": {
                    "ready": True, "qstRevision": 1, "generation": "1",
                    "sourceThemeId": "qinda-dark", "backgroundBase": "#171a18",
                },
                "taskList": {"phase": "ready", "generation": "1", "windowCount": 2},
                "presentation": {
                    "privatePresentationAllowed": True, "centerOpen": True,
                },
                "centerOpenedCount": "1",
                "centerWindow": {
                    "exists": True, "visible": True, "outputName": "WL-0",
                },
            },
        },
        "surface": {
            "scope": "notification-center", "processId": str(shell_pid),
            "outputName": "WL-0", "desiredOutputName": "WL-0",
            "mapped": True, "committed": True, "active": True,
            "geometry": geometry,
        },
    }
    evidence["capture"] = {
        "tool": "weston-screenshooter", "path": "desktop-1080p.png",
        "sha256": "a" * 64, "byteCount": 4096, "width": 1920,
        "height": 1080, "sampledDistinctColors": 16,
        "contentRegion": {
            **geometry, "sampledDistinctColors": 16, "sha256": "b" * 64,
        },
    }
    return evidence


class InteractiveEvidenceTests(unittest.TestCase):
    def test_exact_contract_passes(self) -> None:
        validate_interactive_evidence(valid_interactive_evidence())

    def test_s1_output_or_foreign_surface_fails(self) -> None:
        evidence = valid_interactive_evidence()
        evidence["outputs"][0]["name"] = "Virtual-0"  # type: ignore[index]
        with self.assertRaisesRegex(TopologyContractError, "canonical KWin wayland"):
            validate_interactive_evidence(evidence)
        evidence = valid_interactive_evidence()
        evidence["interaction"]["surface"]["processId"] = "999999"  # type: ignore[index]
        with self.assertRaisesRegex(TopologyContractError, "interaction"):
            validate_interactive_evidence(evidence)

    def test_nonprivate_forwarded_input_shape_fails(self) -> None:
        evidence = valid_interactive_evidence()
        evidence["inputDevices"][0]["vendorId"] = 1  # type: ignore[index]
        with self.assertRaisesRegex(TopologyContractError, "fake-seat"):
            validate_interactive_evidence(evidence)

    def test_same_socket_or_preopened_center_fails(self) -> None:
        evidence = valid_interactive_evidence()
        evidence["containment"]["childWaylandSocket"] = "qindaqt-parent-wayland"  # type: ignore[index]
        with self.assertRaisesRegex(TopologyContractError, "containment"):
            validate_interactive_evidence(evidence)
        evidence = valid_interactive_evidence()
        evidence["interaction"]["preInjectionActiveSurfaceCount"] = 1  # type: ignore[index]
        with self.assertRaisesRegex(TopologyContractError, "interaction"):
            validate_interactive_evidence(evidence)

    def test_activation_and_shell_presentation_mutations_fail(self) -> None:
        mutations = (
            (("activation", "action"), "wrong-action"),
            (("activation", "component"), "attacker-shell"),
            (("activation", "pressed"), False),
            (("activation", "released"), False),
            (("shellPresentation", "before", "owner"), ":1.19"),
            (("shellPresentation", "after", "owner"), ":1.21"),
            (("shellPresentation", "before", "servicePid"), "77"),
            (("shellPresentation", "before", "shellPid"), "77"),
            (("shellPresentation", "after", "servicePid"), "77"),
            (("shellPresentation", "after", "shellPid"), "77"),
            (("shellPresentation", "before", "presentation",
              "privatePresentationAllowed"), False),
            (("shellPresentation", "after", "presentation",
              "privatePresentationAllowed"), False),
            (("shellPresentation", "before", "presentation", "centerOpen"), True),
            (("shellPresentation", "after", "presentation", "centerOpen"), False),
            (("shellPresentation", "before", "centerWindow", "exists"), False),
            (("shellPresentation", "after", "centerWindow", "exists"), False),
            (("shellPresentation", "before", "centerWindow", "visible"), True),
            (("shellPresentation", "after", "centerWindow", "visible"), False),
            (("shellPresentation", "before", "centerWindow", "outputName"), "WL-9"),
            (("shellPresentation", "after", "centerWindow", "outputName"), "WL-9"),
            (("shellPresentation", "before", "centerOpenedCount"), "01"),
            (("shellPresentation", "after", "centerOpenedCount"), "0"),
        )
        for path, value in mutations:
            with self.subTest(path=path):
                evidence = valid_interactive_evidence()
                cursor = evidence["interaction"]  # type: ignore[assignment]
                for field in path[:-1]:
                    cursor = cursor[field]  # type: ignore[index,assignment]
                cursor[path[-1]] = value  # type: ignore[index]
                with self.assertRaises(TopologyContractError):
                    validate_interactive_evidence(evidence)

    def test_capture_must_bind_to_surface_region(self) -> None:
        evidence = valid_interactive_evidence()
        evidence["capture"]["contentRegion"]["x"] += 1  # type: ignore[index]
        with self.assertRaisesRegex(TopologyContractError, "captured desktop"):
            validate_interactive_evidence(evidence)


if __name__ == "__main__":
    unittest.main()
