# SPDX-License-Identifier: GPL-3.0-or-later
"""Hostile identity and cross-inventory tests for the private virtual output."""

from __future__ import annotations

import copy
import unittest

from desktop_session_topology import TopologyContractError, validate_boot_evidence
from test_desktop_session_topology_unit import valid_evidence


class OutputInventoryTests(unittest.TestCase):
    def test_runtime_assigned_canonical_virtual_output_ordinal_passes(self) -> None:
        validate_boot_evidence(valid_evidence("Virtual-17"))

    def test_output_name_must_use_canonical_kwin_virtual_form(self) -> None:
        for output_name in (
            "", "Virtual-", "Virtual--1", "Virtual-01", "Virtual-+1",
            "Virtual-1 ", "HDMI-A-1", "Virtual-" + "1" * 505, None, 0, True,
        ):
            with self.subTest(output_name=output_name):
                evidence = valid_evidence()
                evidence["outputs"][0]["name"] = output_name  # type: ignore[index]
                with self.assertRaisesRegex(
                    TopologyContractError, "canonical KWin virtual output"
                ):
                    validate_boot_evidence(evidence)

    def test_output_and_visibility_inventory_identity_must_match(self) -> None:
        evidence = valid_evidence()
        evidence["visibilityOutputs"][0]["id"] = "Virtual-1"  # type: ignore[index]
        with self.assertRaisesRegex(TopologyContractError, "different outputs"):
            validate_boot_evidence(evidence)

    def test_both_public_inventories_must_contain_exactly_one_output(self) -> None:
        for field in ("outputs", "visibilityOutputs"):
            for count in (0, 2):
                with self.subTest(field=field, count=count):
                    evidence = valid_evidence()
                    record = evidence[field][0]  # type: ignore[index]
                    evidence[field] = [record] * count
                    with self.assertRaisesRegex(
                        TopologyContractError, "requires exactly one output"
                    ):
                        validate_boot_evidence(evidence)

    def test_visibility_inventory_retains_exact_geometry(self) -> None:
        evidence = valid_evidence()
        evidence["visibilityOutputs"][0]["geometry"]["width"] = 1919  # type: ignore[index]
        with self.assertRaisesRegex(TopologyContractError, "1920x1080"):
            validate_boot_evidence(evidence)

    def test_geometry_and_scale_reject_boolean_substitutions(self) -> None:
        for inventory in ("outputs", "visibilityOutputs"):
            for field, value in (("x", False), ("y", False), ("scale", True)):
                with self.subTest(inventory=inventory, field=field):
                    evidence = valid_evidence()
                    record = evidence[inventory][0]  # type: ignore[index]
                    if field == "scale":
                        record[field] = value
                    else:
                        record["geometry"][field] = value
                    with self.assertRaisesRegex(TopologyContractError, "1920x1080"):
                        validate_boot_evidence(evidence)

    def test_dock_output_references_bind_to_observed_inventory(self) -> None:
        for field in ("outputName", "desiredOutputName"):
            with self.subTest(field=field):
                evidence = valid_evidence("Virtual-17")
                evidence["dockSurfaces"][0][field] = "Virtual-0"  # type: ignore[index]
                with self.assertRaisesRegex(TopologyContractError, "dock surface"):
                    validate_boot_evidence(evidence)

    def test_one_valid_dock_cannot_hide_a_phantom_output_record(self) -> None:
        for field in ("outputName", "desiredOutputName"):
            with self.subTest(field=field):
                evidence = valid_evidence("Virtual-17")
                phantom = copy.deepcopy(evidence["dockSurfaces"][0])  # type: ignore[index]
                phantom[field] = "Virtual-999999"
                evidence["dockSurfaces"].append(phantom)  # type: ignore[union-attr]
                with self.assertRaisesRegex(TopologyContractError, "dock surface"):
                    validate_boot_evidence(evidence)


if __name__ == "__main__":
    unittest.main()
