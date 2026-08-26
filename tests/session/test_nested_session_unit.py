# SPDX-License-Identifier: GPL-3.0-or-later
"""Focused tests for virtual-display scenario representability."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from host_input_consent import (
    HOST_UINPUT_CONSENT_ENV,
    HOST_UINPUT_CONSENT_VALUE,
    host_uinput_consent_error,
)
from hybrid_pointer_validation import (
    validate_hybrid_pointer_evidence,
    validate_hybrid_unload_evidence,
)
from test_nested_session import (
    ScenarioCoverageError,
    virtual_spec_from_document,
    write_virtual_output_config,
)


def output(
    name: str,
    *,
    width: int = 1920,
    height: int = 1080,
    scale: float = 1.0,
    enabled: bool = True,
    transform: str = "normal",
) -> dict[str, object]:
    return {
        "name": name,
        "enabled": enabled,
        "mode": {"width": width, "height": height},
        "scale": scale,
        "transform": transform,
    }


class VirtualSpecTests(unittest.TestCase):
    def test_common_fractional_mode_becomes_exact_logical_geometry(self) -> None:
        document = {
            "id": "dual-fractional",
            "outputs": [
                output("DP-1", scale=1.25),
                output("DP-2", scale=1.25),
                output("disabled", width=2560, height=1440, enabled=False),
            ],
        }

        spec = virtual_spec_from_document(document)

        self.assertEqual(spec.output_count, 2)
        self.assertEqual((spec.pixel_width, spec.pixel_height), (1920, 1080))
        self.assertEqual((spec.logical_width, spec.logical_height), (1536, 864))
        self.assertEqual(spec.scale, 1.25)
        self.assertIn("positions", spec.coverage["notApplied"])

    def test_heterogeneous_enabled_modes_are_rejected_explicitly(self) -> None:
        document = {
            "id": "mixed",
            "outputs": [output("DP-1"), output("DP-2", width=2560, height=1440)],
        }

        with self.assertRaisesRegex(
            ScenarioCoverageError,
            "heterogeneous enabled output modes/scales",
        ):
            virtual_spec_from_document(document)

    def test_non_integral_logical_extent_is_rejected(self) -> None:
        document = {
            "id": "non-integral",
            "outputs": [output("DP-1", width=2560, height=1440, scale=1.5)],
        }

        with self.assertRaisesRegex(ScenarioCoverageError, "non-integral logical size"):
            virtual_spec_from_document(document)

    def test_transform_is_not_misreported_as_applied(self) -> None:
        document = {
            "id": "portrait",
            "outputs": [output("DP-1", width=1920, height=1200, transform="rotate-90")],
        }

        with self.assertRaisesRegex(ScenarioCoverageError, "cannot apply output transforms"):
            virtual_spec_from_document(document)

    def test_isolated_kwin_config_pins_common_scale(self) -> None:
        spec = virtual_spec_from_document(
            {
                "id": "fractional",
                "outputs": [output("DP-1", scale=1.25)],
            }
        )
        with tempfile.TemporaryDirectory() as directory:
            write_virtual_output_config(Path(directory), spec)
            document = json.loads(
                (Path(directory) / "kwinoutputconfig.json").read_text(encoding="utf-8")
            )

        output_data = document[0]["data"][0]
        self.assertEqual(output_data["mode"]["width"], 1920)
        self.assertEqual(output_data["mode"]["height"], 1080)
        self.assertEqual(output_data["scale"], 1.25)


class HostInputConsentTests(unittest.TestCase):
    def test_non_host_input_workflow_needs_no_consent(self) -> None:
        self.assertIsNone(host_uinput_consent_error(False, {}))

    def test_missing_or_inexact_consent_blocks_host_input(self) -> None:
        error = host_uinput_consent_error(True, {})
        self.assertIsNotNone(error)
        self.assertIn("move, click, and type", error)
        self.assertIsNotNone(
            host_uinput_consent_error(True, {HOST_UINPUT_CONSENT_ENV: "yes"})
        )

    def test_exact_per_invocation_consent_allows_host_input(self) -> None:
        self.assertIsNone(
            host_uinput_consent_error(
                True,
                {HOST_UINPUT_CONSENT_ENV: HOST_UINPUT_CONSENT_VALUE},
            )
        )


class HybridPointerEvidenceTests(unittest.TestCase):
    @staticmethod
    def common_evidence() -> dict[str, object]:
        return {
            "workflow": "hybrid-pointer",
            "dotoolProcessCount": 1,
            "dotoolProcessStayedRunning": True,
            "exactModifierGesture": "Meta+Shift+Left",
            "topologyRevisionAdvanced": True,
            "sameOwnerAfterDock": True,
            "validTargetSplit": True,
            "plainNativeDecorationDetach": True,
            "nativeDecorationMemberTitleDrag": True,
            "ownersClearedAfterDetach": True,
            "draggedMemberFollowedPointer": True,
            "draggedMemberSizeRestored": True,
            "siblingExactFrameRestored": True,
            "chromeSceneAttached": True,
            "chromeSceneRemovedAfterDetach": True,
            "groupStackContiguous": True,
            "unrelatedWindowCoveredGroupChrome": True,
            "coveredWindowBlockedSharedChromeInput": True,
            "popupGrabDismissedBeforeSharedChromeInput": True,
            "normalTransientExcludedFromTopology": True,
            "transientFocusPreservedOutsideTopology": True,
            "unrelatedWindowRemainedAboveTransientGroup": True,
            "sharedChromeRaisedGroupUnit": True,
            "stableGroupRepresentativeActivated": True,
            "outerTitleContextMenuOpened": True,
            "productionQMenuKeepAboveTriggered": True,
            "groupContextQueuedAdoption": True,
            "keepAboveAppliedToEveryMember": True,
            "keepAboveToggleRecoveredEveryMember": True,
            "unrelatedWindowInventoried": True,
            "hybridSnapshotReadable": True,
            "publicContainerRevision": "4",
            "publicSnapshot": {
                "status": "ok",
                "authority": "hybrid-process",
                "revision": "4",
                "snapshot": {"schemaVersion": 1, "pages": [{"id": "page"}]},
            },
        }

    def test_accepts_real_uinput_evidence(self) -> None:
        evidence = self.common_evidence()
        evidence.update(
            {
                "inputInjector": "dotool-uinput",
                "uinputAdmitted": True,
                "uinputDevices": [{"name": "dotool keyboard"}, {"name": "dotool pointer"}],
            }
        )

        validate_hybrid_pointer_evidence({"compositorEvidence": evidence})

    def test_accepts_disclosed_virtual_backend_fallback(self) -> None:
        evidence = self.common_evidence()
        evidence.update(
            {
                "inputInjector": "qindaqt-development-input",
                "uinputAdmitted": False,
                "uinputAdmissionFailure": "virtual backend devices=[]",
                "developmentInputDeviceId": "qindaqt-development-input",
                "developmentInputRequestCount": 17,
            }
        )

        validate_hybrid_pointer_evidence({"compositorEvidence": evidence})

    def test_rejects_fallback_that_hides_uinput_result(self) -> None:
        evidence = self.common_evidence()
        evidence.update(
            {
                "inputInjector": "qindaqt-development-input",
                "uinputAdmitted": False,
                "uinputAdmissionFailure": "",
                "developmentInputDeviceId": "qindaqt-development-input",
                "developmentInputRequestCount": 17,
            }
        )

        with self.assertRaisesRegex(RuntimeError, "omitted the concrete uinput failure"):
            validate_hybrid_pointer_evidence({"compositorEvidence": evidence})

    def test_rejects_revision_zero_even_with_readable_snapshot(self) -> None:
        evidence = self.common_evidence()
        evidence["publicContainerRevision"] = "0"
        evidence["publicSnapshot"]["revision"] = "0"

        with self.assertRaisesRegex(RuntimeError, "nonzero decimal string"):
            validate_hybrid_pointer_evidence({"compositorEvidence": evidence})

    def test_accepts_grouped_unload_lifecycle_evidence(self) -> None:
        evidence = self.common_evidence()
        evidence.update(
            {
                "workflow": "hybrid-pointer-plugin-unload",
                "ownershipAuthority": "hybrid-process",
                "legacyBridgeContainersUsed": False,
                "observedFramesAndVisibilityRestoredAfterUnload": True,
                "observedIndependentStateRestoredAfterUnload": True,
                "onePrimaryTaskIdentity": True,
                "onePrimarySwitcherIdentity": True,
                "samePrimaryTaskAndSwitcherIdentity": True,
                "inactivePageExcluded": True,
                "inactiveActivationActivatedPage": True,
                "pageSwitchBackRetainsSingleIdentity": True,
                "nativeMemberMinimizedWholeContainer": True,
                "activePageOnlyRestore": True,
                "taskFlagsRestoredAfterUnload": True,
                "ownershipAuthorityRemoved": True,
                "runtimeDecorationClasses": ["QindaDecoration"] * 3,
                "inputInjector": "qindaqt-development-input",
                "uinputAdmitted": False,
                "uinputAdmissionFailure": "virtual backend devices=[]",
                "developmentInputDeviceId": "qindaqt-development-input",
                "developmentInputRequestCount": 17,
            }
        )

        validate_hybrid_unload_evidence(evidence)

    def test_rejects_unload_without_active_page_only_restore(self) -> None:
        evidence = self.common_evidence()
        evidence.update(
            {
                "workflow": "hybrid-pointer-plugin-unload",
                "ownershipAuthority": "hybrid-process",
                "legacyBridgeContainersUsed": False,
                "observedFramesAndVisibilityRestoredAfterUnload": True,
                "observedIndependentStateRestoredAfterUnload": True,
                "onePrimaryTaskIdentity": True,
                "onePrimarySwitcherIdentity": True,
                "samePrimaryTaskAndSwitcherIdentity": True,
                "inactivePageExcluded": True,
                "inactiveActivationActivatedPage": True,
                "pageSwitchBackRetainsSingleIdentity": True,
                "nativeMemberMinimizedWholeContainer": True,
                "activePageOnlyRestore": False,
                "taskFlagsRestoredAfterUnload": True,
                "ownershipAuthorityRemoved": True,
                "runtimeDecorationClasses": ["QindaDecoration"] * 3,
            }
        )

        with self.assertRaisesRegex(RuntimeError, "activePageOnlyRestore"):
            validate_hybrid_unload_evidence(evidence)


if __name__ == "__main__":
    unittest.main()
