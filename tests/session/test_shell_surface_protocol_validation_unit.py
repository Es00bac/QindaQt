# SPDX-License-Identifier: GPL-3.0-or-later
"""Focused contract tests for production shell layer-role validation."""

from __future__ import annotations

import copy
import unittest
from dataclasses import dataclass
from typing import Any

from shell_surface_protocol_validation import validate_active_protocol


@dataclass(frozen=True)
class OutputSpec:
    logical_width: int = 1920
    logical_height: int = 1080


def state(
    *, layer: int, anchors: int, edge: int, zone: int, width: int, height: int
) -> dict[str, Any]:
    return {
        "layer": layer,
        "anchors": anchors,
        "exclusiveEdge": edge,
        "exclusiveZone": zone,
        "desiredSize": {"width": width, "height": height},
    }


def surface(
    *,
    role_id: int,
    surface_id: int,
    buffer_id: int,
    scope: str,
    role_state: dict[str, Any],
    configured_width: int,
    configured_height: int,
) -> dict[str, Any]:
    return {
        "roleId": str(role_id),
        "waylandSurfaceId": str(surface_id),
        "outputId": "100",
        "requestCount": 1,
        "scope": scope,
        "initialLayer": role_state["layer"],
        "roleDestroyed": False,
        "surfaceDestroyed": False,
        "mapped": True,
        "pendingState": copy.deepcopy(role_state),
        "committedEpoch": 2,
        "committedState": copy.deepcopy(role_state),
        "configurations": [
            {
                "serial": str(role_id + 1000),
                "width": configured_width,
                "height": configured_height,
                "committedEpoch": 1,
                "committedState": copy.deepcopy(role_state),
                "configureOrder": 1,
                "acknowledgeOrder": 2,
            }
        ],
        "activeBufferMapping": {
            "commitEpoch": 2,
            "bufferId": str(buffer_id),
            "attachOrder": 3,
            "commitOrder": 4,
            "configureSerial": str(role_id + 1000),
            "configureCommittedEpoch": 1,
            "committedState": copy.deepcopy(role_state),
        },
    }


def valid_protocol() -> dict[str, Any]:
    return {
        "inputTruncated": False,
        "identityAmbiguous": False,
        "protocolAmbiguous": False,
        "surfaces": [
            surface(
                role_id=10,
                surface_id=20,
                buffer_id=30,
                scope="desktop",
                role_state=state(
                    layer=0, anchors=15, edge=0, zone=-1, width=0, height=0
                ),
                configured_width=1920,
                configured_height=1080,
            ),
            surface(
                role_id=11,
                surface_id=21,
                buffer_id=31,
                scope="dock",
                role_state=state(
                    layer=2, anchors=13, edge=1, zone=30, width=0, height=30
                ),
                configured_width=1920,
                configured_height=30,
            ),
            surface(
                role_id=12,
                surface_id=22,
                buffer_id=32,
                scope="dock",
                role_state=state(
                    layer=2, anchors=6, edge=2, zone=54, width=998, height=54
                ),
                configured_width=998,
                configured_height=54,
            ),
        ],
    }


class ShellSurfaceProtocolValidationTest(unittest.TestCase):
    def test_accepts_one_desktop_top_bar_and_shelf(self) -> None:
        identities = validate_active_protocol(valid_protocol(), OutputSpec(), "active")
        self.assertEqual(len(identities), 3)

    def test_rejects_missing_role(self) -> None:
        for missing_index, role_name in enumerate(("desktop", "top bar", "shelf")):
            with self.subTest(role=role_name):
                protocol = valid_protocol()
                protocol["surfaces"].pop(missing_index)
                with self.assertRaisesRegex(
                    RuntimeError, "expected exactly one desktop, top bar, and shelf"
                ):
                    validate_active_protocol(protocol, OutputSpec(), "active")

    def test_rejects_duplicate_semantic_role(self) -> None:
        for source_index, role_name in enumerate(("desktop", "top bar", "shelf")):
            with self.subTest(role=role_name):
                protocol = valid_protocol()
                duplicate = copy.deepcopy(protocol["surfaces"][source_index])
                duplicate.update({"roleId": "13", "waylandSurfaceId": "23"})
                duplicate["activeBufferMapping"]["bufferId"] = "33"
                duplicate["configurations"][0]["serial"] = "1013"
                duplicate["activeBufferMapping"]["configureSerial"] = "1013"
                protocol["surfaces"][(source_index + 1) % 3] = duplicate
                with self.assertRaisesRegex(
                    RuntimeError, f"claimed the {role_name} role"
                ):
                    validate_active_protocol(protocol, OutputSpec(), "active")


if __name__ == "__main__":
    unittest.main()
