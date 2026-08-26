# SPDX-License-Identifier: GPL-3.0-or-later
"""Validation for the nested Hybrid pointer evidence document."""

from __future__ import annotations

from typing import Any


def validate_hybrid_public_snapshot(evidence: dict[str, Any]) -> None:
    """Require the process-local Hybrid authority at a live nonzero revision."""
    revision = evidence.get("publicContainerRevision")
    try:
        numeric_revision = int(revision)
    except (TypeError, ValueError) as error:
        raise RuntimeError("Hybrid public revision was not a decimal string") from error
    if not isinstance(revision, str) or numeric_revision <= 0:
        raise RuntimeError("Hybrid public revision was not a nonzero decimal string")
    snapshot = evidence.get("publicSnapshot")
    model = snapshot.get("snapshot") if isinstance(snapshot, dict) else None
    if (
        not isinstance(snapshot, dict)
        or snapshot.get("status") != "ok"
        or snapshot.get("authority") != "hybrid-process"
        or snapshot.get("revision") != revision
        or not isinstance(model, dict)
        or model.get("schemaVersion") != 1
        or not isinstance(model.get("pages"), list)
        or not model["pages"]
    ):
        raise RuntimeError("Hybrid public Snapshot evidence was stale or unreadable")


def validate_hybrid_input_evidence(evidence: dict[str, Any]) -> None:
    """Validate either real dotool admission or the explicitly disclosed fallback."""
    if evidence.get("dotoolProcessCount") != 1:
        raise RuntimeError("Hybrid input evidence did not use exactly one dotool process")
    if evidence.get("dotoolProcessStayedRunning") is not True:
        raise RuntimeError("dotool did not remain alive for the Hybrid workflow")
    injector = evidence.get("inputInjector")
    if injector == "dotool-uinput":
        devices = evidence.get("uinputDevices")
        if evidence.get("uinputAdmitted") is not True:
            raise RuntimeError("dotool evidence did not prove uinput admission")
        if not isinstance(devices, list) or len(devices) < 2:
            raise RuntimeError("KWin did not inventory dotool's uinput devices")
        return
    if injector != "qindaqt-development-input":
        raise RuntimeError(f"unexpected Hybrid input injector: {injector!r}")
    if evidence.get("uinputAdmitted") is not False:
        raise RuntimeError("development fallback hid a successful uinput admission")
    admission_failure = evidence.get("uinputAdmissionFailure")
    if not isinstance(admission_failure, str) or not admission_failure:
        raise RuntimeError("development fallback omitted the concrete uinput failure")
    if evidence.get("developmentInputDeviceId") != "qindaqt-development-input":
        raise RuntimeError("development fallback did not preserve its gated device identity")
    requests = evidence.get("developmentInputRequestCount")
    if not isinstance(requests, int) or isinstance(requests, bool) or requests <= 0:
        raise RuntimeError("development fallback did not report injected request batches")


def validate_hybrid_pointer_evidence(result: dict[str, Any]) -> None:
    evidence = result.get("compositorEvidence")
    if not isinstance(evidence, dict):
        raise RuntimeError("the Hybrid pointer workflow omitted structured evidence")
    expected = {
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
    }
    mismatched = {
        key: (evidence.get(key), value)
        for key, value in expected.items()
        if evidence.get(key) != value
    }
    if mismatched:
        raise RuntimeError(f"Hybrid pointer evidence mismatch: {mismatched}")

    validate_hybrid_public_snapshot(evidence)
    validate_hybrid_input_evidence(evidence)


def validate_hybrid_unload_evidence(evidence: dict[str, Any]) -> None:
    """Require the live grouped lifecycle and independent recovery proof."""
    expected = {
        "workflow": "hybrid-pointer-plugin-unload",
        "ownershipAuthority": "hybrid-process",
        "legacyBridgeContainersUsed": False,
        "exactModifierGesture": "Meta+Shift+Left",
        "validTargetSplit": True,
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
    }
    mismatched = {
        key: (evidence.get(key), value)
        for key, value in expected.items()
        if evidence.get(key) != value
    }
    if mismatched:
        raise RuntimeError(f"Hybrid unload evidence mismatch: {mismatched}")

    decorations = evidence.get("runtimeDecorationClasses")
    if (
        not isinstance(decorations, list)
        or len(decorations) != 3
        or not all(
            isinstance(value, str) and "QindaDecoration" in value
            for value in decorations
        )
    ):
        raise RuntimeError(
            f"Hybrid unload decoration evidence was invalid: {decorations!r}"
        )

    validate_hybrid_public_snapshot(evidence)
    validate_hybrid_input_evidence(evidence)
