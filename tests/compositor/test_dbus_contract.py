# SPDX-License-Identifier: GPL-3.0-or-later
"""Validate the checked-in Compositor1 descriptor and deployment metadata."""

from __future__ import annotations

import argparse
import json
import xml.etree.ElementTree as ET
from pathlib import Path


EXPECTED_METHODS = {
    "Capabilities",
    "Windows",
    "Outputs",
    "InputCapabilities",
    "ShellVisibilitySnapshot",
    "Containers",
    "DockWindows",
    "ReleaseContainer",
    "Snapshot",
    "Submit",
    "InjectTestInput",
    "AddVirtualOutputForTest",
    "RemoveVirtualOutputForTest",
    "ReinitializeCompositingForTest",
}
EXPECTED_SIGNALS = {
    "ContainerCommitted",
    "WindowsChanged",
    "OutputsChanged",
    "InputCapabilitiesChanged",
    "ShellVisibilityChanged",
}


def validate_method_signature(
    interface: ET.Element,
    method_name: str,
    expected_arguments: list[tuple[str, str, str]],
) -> None:
    method = next(
        element
        for element in interface.findall("method")
        if element.get("name") == method_name
    )
    arguments = [
        (argument.get("name"), argument.get("type"), argument.get("direction"))
        for argument in method.findall("arg")
    ]
    if arguments != expected_arguments:
        raise ValueError(
            f"{method_name} signature drift: "
            f"expected {expected_arguments}, got {arguments}"
        )


def validate_descriptor(path: Path) -> None:
    interface = ET.parse(path).getroot().find("interface")
    if interface is None or interface.get("name") != "org.qindaqt.Compositor1":
        raise ValueError("descriptor must contain org.qindaqt.Compositor1")
    methods = {element.get("name") for element in interface.findall("method")}
    signals = {element.get("name") for element in interface.findall("signal")}
    if methods != EXPECTED_METHODS:
        raise ValueError(
            f"method drift: expected {sorted(EXPECTED_METHODS)}, got {sorted(methods)}"
        )
    if signals != EXPECTED_SIGNALS:
        raise ValueError(
            f"signal drift: expected {sorted(EXPECTED_SIGNALS)}, got {sorted(signals)}"
        )

    dock = next(
        element
        for element in interface.findall("method")
        if element.get("name") == "DockWindows"
    )
    inputs = [
        (argument.get("name"), argument.get("type"))
        for argument in dock.findall("arg")
        if argument.get("direction") == "in"
    ]
    expected_inputs = [
        ("targetWindowId", "s"),
        ("incomingWindowId", "s"),
        ("orientation", "s"),
        ("position", "s"),
        ("ratio", "d"),
    ]
    if inputs != expected_inputs:
        raise ValueError(
            f"DockWindows signature drift: expected {expected_inputs}, got {inputs}"
        )

    validate_method_signature(
        interface,
        "InjectTestInput",
        [("requestJson", "ay", "in"), ("replyJson", "ay", "out")],
    )

    expected_output_mutations = {
        "AddVirtualOutputForTest": [
            ("name", "s", "in"),
            ("width", "i", "in"),
            ("height", "i", "in"),
            ("scale", "d", "in"),
            ("replyJson", "ay", "out"),
        ],
        "RemoveVirtualOutputForTest": [
            ("name", "s", "in"),
            ("replyJson", "ay", "out"),
        ],
    }
    for method_name, expected_arguments in expected_output_mutations.items():
        validate_method_signature(interface, method_name, expected_arguments)

    validate_method_signature(
        interface, "ReinitializeCompositingForTest", [("replyJson", "ay", "out")]
    )
    validate_method_signature(
        interface, "ShellVisibilitySnapshot", [("snapshotJson", "ay", "out")]
    )

    visibility_changed = next(
        element
        for element in interface.findall("signal")
        if element.get("name") == "ShellVisibilityChanged"
    )
    if visibility_changed.findall("arg"):
        raise ValueError("ShellVisibilityChanged must remain a no-argument hint")


def validate_service_metadata(path: Path) -> None:
    document = json.loads(path.read_text(encoding="utf-8"))
    expected = {
        "schemaVersion": 1,
        "service": "org.qindaqt.Compositor",
        "objectPath": "/org/qindaqt/Compositor",
        "interface": "org.qindaqt.Compositor1",
        "transport": "user-session-bus",
        "productionControlMode": "read-only",
        "protocolVersion": "1.1",
    }
    for key, value in expected.items():
        if document.get(key) != value:
            raise ValueError(f"service metadata {key!r} must be {value!r}")
    if "no caller authentication" not in document.get("authorization", ""):
        raise ValueError("service metadata must not imply caller authentication")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("descriptor", type=Path)
    parser.add_argument("service_metadata", type=Path)
    arguments = parser.parse_args()
    validate_descriptor(arguments.descriptor)
    validate_service_metadata(arguments.service_metadata)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
