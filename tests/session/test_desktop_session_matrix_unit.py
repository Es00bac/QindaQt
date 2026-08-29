# SPDX-License-Identifier: GPL-3.0-or-later
"""Hostile unit contract for the S3 contained desktop matrix."""

from __future__ import annotations

import copy
import unittest
from pathlib import Path

from desktop_session_interactive import validate_interactive_evidence
from desktop_session_matrix import (
    DesktopMatrixError,
    EXECUTABLE_MATRIX_ROWS,
    load_matrix_scenario,
    physical_content_region,
)
from desktop_session_topology import (
    TopologyContractError,
    interactive_matrix_topology,
)
from test_desktop_session_topology_unit import valid_evidence


SOURCE_ROOT = Path(__file__).resolve().parents[2]


def valid_matrix_evidence(scenario_id: str) -> tuple[dict[str, object], object]:
    scenario = load_matrix_scenario(SOURCE_ROOT, scenario_id)
    topology = interactive_matrix_topology(scenario)
    evidence = valid_evidence("WL-0")
    parent_pid = 999
    parent_executable = "kwin_wayland" if scenario.virtual.scale != 1.0 else "weston"
    evidence["topology"] = topology.document()
    evidence["processes"] = {
        "parent-compositor": {
            "pid": parent_pid, "executable": parent_executable,
            "parentRole": None,
        },
        **evidence["processes"],  # type: ignore[arg-type]
    }
    if scenario.virtual.scale != 1.0:
        evidence["processes"]["parent-private-bus"] = {  # type: ignore[index]
            "pid": 998, "executable": "dbus-daemon", "parentRole": None,
        }
        evidence["cleanup"]["terminalPhases"].insert(  # type: ignore[index]
            0,
            {
                "role": "parent-private-bus", "pid": 998,
                "processGroup": 998, "executablePath": "/usr/bin/dbus-daemon",
                "startTicks": 998, "terminalPhase": "term",
            },
        )
    evidence["cleanup"]["terminalPhases"].insert(  # type: ignore[index]
        0,
        {
            "role": "parent-compositor", "pid": parent_pid,
            "processGroup": parent_pid,
            "executablePath": f"/usr/bin/{parent_executable}",
            "startTicks": 999, "terminalPhase": "term",
        },
    )
    outputs = []
    visibility = []
    docks = []
    shell_pid = evidence["processes"]["shell"]["pid"]  # type: ignore[index]
    for output in topology.outputs:
        name = f"WL-{output.ordinal}"
        geometry = {
            "x": output.logical_x, "y": output.logical_y,
            "width": output.logical_width, "height": output.logical_height,
        }
        outputs.append({"name": name, "geometry": geometry, "scale": output.scale})
        visibility.append({"id": name, "geometry": geometry, "scale": output.scale})
        docks.append({
            "scope": "dock", "processId": str(shell_pid),
            "outputName": name, "desiredOutputName": name,
            "mapped": True, "committed": True,
        })
    evidence["outputs"] = outputs
    evidence["visibilityOutputs"] = visibility
    evidence["dockSurfaces"] = docks
    evidence["inputDevices"] = [
        {
            "id": "input-00000001", "name": "", "enabled": True,
            "busType": 0, "vendorId": 0, "productId": 0,
            "capabilities": ["keyboard"],
        },
        {
            "id": "input-00000002", "name": "QindaQt Development Input",
            "enabled": True, "busType": 0, "vendorId": 0, "productId": 0,
            "capabilities": ["keyboard", "pointer"],
        },
    ]
    if scenario.virtual.scale == 1.0:
        evidence["inputDevices"].insert(0, {  # type: ignore[union-attr]
            "id": "input-00000003", "name": "", "enabled": True,
            "busType": 0, "vendorId": 0, "productId": 0,
            "capabilities": ["pointer"],
        })
    active = topology.outputs[1] if scenario.virtual.output_count == 2 else topology.outputs[0]
    logical_geometry = {
        "x": active.logical_x + active.logical_width - 456,
        "y": active.logical_y + 16,
        "width": 440,
        "height": 640,
    }
    evidence["containment"].update({  # type: ignore[union-attr]
        "parentBackend": (
            "kwin-virtual-qpaint" if scenario.virtual.scale != 1.0
            else "weston-headless-pixman"
        ),
        "qindaqtBackend": "kwin-windowed-qpaint",
        "parentWaylandSocket": "qindaqt-parent-wayland",
        "childWaylandSocket": "qindaqt-0123456789ab",
    })
    evidence["interaction"] = {
        "action": "open-notification-center",
        "deviceId": "qindaqt-development-input",
        "eventCount": 5 if scenario.virtual.output_count == 2 else 4,
        "preInjectionActiveSurfaceCount": 0,
        "surface": {
            "scope": "notification-center", "processId": str(shell_pid),
            "outputName": f"WL-{active.ordinal}",
            "desiredOutputName": f"WL-{active.ordinal}",
            "mapped": True, "committed": True, "active": True,
            "geometry": logical_geometry,
        },
    }
    evidence["matrixPresentation"] = {
        "scenarioId": scenario.scenario_id,
        "profileId": scenario.profile_id,
        "themeId": scenario.theme_id,
        "requestedScale": scenario.virtual.scale,
        "parentArguments": (
            [
                "--virtual", "--width", str(scenario.virtual.logical_width),
                "--height", str(scenario.virtual.logical_height),
                "--scale", str(scenario.virtual.scale), "--output-count", "1",
                "--socket", "qindaqt-parent-wayland", "--no-lockscreen",
                "--no-global-shortcuts",
            ]
            if scenario.virtual.scale != 1.0 else None
        ),
        "sessionArguments": [
            "--profile", scenario.profile_id, "--theme", scenario.theme_id
        ],
        "editorArguments": ["--theme", scenario.theme_id],
    }
    captures = []
    for output in (active,):
        capture = {
            "tool": (
                "kwin-virtual-shm" if output.render_scale != 1.0
                else "weston-screenshooter"
            ),
            "path": (
                f"desktop-matrix-{scenario.scenario_id}-output-{output.ordinal}.png"
            ),
            "sha256": f"{output.ordinal + 1:x}" * 64,
            "byteCount": 4096,
            "width": output.pixel_width,
            "height": output.pixel_height,
            "sampledDistinctColors": 16,
            "outputName": f"WL-{output.ordinal}",
        }
        capture["contentRegion"] = {
            **physical_content_region(
                logical_geometry, output, physical_scale=output.render_scale
            ),
            "sampledDistinctColors": 16,
            "sha256": "a" * 64,
        }
        captures.append(capture)
    evidence["matrixCaptures"] = captures
    return evidence, topology


class DesktopMatrixTests(unittest.TestCase):
    def test_rows_cover_requested_breadth(self) -> None:
        scenarios = [load_matrix_scenario(SOURCE_ROOT, row) for row in EXECUTABLE_MATRIX_ROWS]
        self.assertEqual({item.theme_id for item in scenarios}, {
            "qinda-light", "qinda-dusk", "qinda-dark"
        })
        self.assertTrue(any(item.virtual.pixel_height == 1200 for item in scenarios))
        self.assertTrue(any(item.virtual.pixel_width == 2560 for item in scenarios))
        self.assertEqual({item.virtual.scale for item in scenarios}, {1.0, 1.25, 1.5})
        self.assertTrue(any(item.virtual.output_count == 2 for item in scenarios))

    def test_unapproved_or_nonhorizontal_rows_fail_closed(self) -> None:
        with self.assertRaisesRegex(DesktopMatrixError, "not approved"):
            load_matrix_scenario(SOURCE_ROOT, "single-1080p")
        with self.assertRaisesRegex(DesktopMatrixError, "not approved"):
            load_matrix_scenario(SOURCE_ROOT, "dual-1080p-vertical")

    def test_fractional_content_region_rounds_outward(self) -> None:
        scenario = load_matrix_scenario(SOURCE_ROOT, "single-1440p-125")
        self.assertEqual(
            physical_content_region(
                {"x": 1592, "y": 46, "width": 440, "height": 640},
                scenario.outputs[0],
            ),
            {"x": 1990, "y": 57, "width": 550, "height": 801},
        )

    def test_each_exact_matrix_contract_passes(self) -> None:
        for row in EXECUTABLE_MATRIX_ROWS:
            with self.subTest(row=row):
                evidence, topology = valid_matrix_evidence(row)
                validate_interactive_evidence(evidence, topology)

    def test_missing_second_output_or_dock_fails(self) -> None:
        evidence, topology = valid_matrix_evidence("dual-1080p-horizontal")
        evidence["outputs"].pop()  # type: ignore[union-attr]
        with self.assertRaisesRegex(TopologyContractError, "cardinality"):
            validate_interactive_evidence(evidence, topology)
        evidence, topology = valid_matrix_evidence("dual-1080p-horizontal")
        evidence["dockSurfaces"].pop()  # type: ignore[union-attr]
        with self.assertRaisesRegex(TopologyContractError, "every matrix output"):
            validate_interactive_evidence(evidence, topology)

    def test_capture_and_presentation_tampering_fail(self) -> None:
        evidence, topology = valid_matrix_evidence("single-1440p-125")
        tampered = copy.deepcopy(evidence)
        tampered["matrixCaptures"][0]["contentRegion"]["x"] += 1  # type: ignore[index]
        with self.assertRaisesRegex(TopologyContractError, "physical region"):
            validate_interactive_evidence(tampered, topology)
        tampered = copy.deepcopy(evidence)
        tampered["matrixPresentation"]["themeId"] = "qinda-dark"  # type: ignore[index]
        with self.assertRaisesRegex(TopologyContractError, "presentation"):
            validate_interactive_evidence(tampered, topology)


if __name__ == "__main__":
    unittest.main()
