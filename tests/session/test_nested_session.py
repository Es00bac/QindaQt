# SPDX-License-Identifier: GPL-3.0-or-later
"""Boot QindaQt/KWin with one exactly representable display scenario."""

from __future__ import annotations

import argparse
import json
import math
import os
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any

from hybrid_pointer_validation import validate_hybrid_pointer_evidence
from host_input_consent import (
    HOST_UINPUT_CONSENT_ENV,
    HOST_UINPUT_SKIP_CODE,
    host_uinput_consent_error,
)
from nested_session_scenario import (
    ScenarioCoverageError,
    VirtualOutputSpec,
    isolated_environment,
    load_virtual_spec,
    virtual_spec_from_document,
    write_virtual_output_config,
)


KWIN_ABI = "6.6.5"


def extract_probe_result(stdout: str) -> dict[str, Any]:
    marker = next(
        (
            line.removeprefix("QINDAQT_PROBE=")
            for line in stdout.splitlines()
            if line.startswith("QINDAQT_PROBE=")
        ),
        None,
    )
    if marker is None:
        raise RuntimeError("probe result marker was missing")
    result = json.loads(marker)
    if not isinstance(result, dict):
        raise RuntimeError("probe result must be a JSON object")
    return result


def _close(first: Any, second: float) -> bool:
    return isinstance(first, (int, float)) and not isinstance(first, bool) and math.isclose(
        float(first), second, rel_tol=0.0, abs_tol=1e-6
    )


def validate_probe_result(
    result: dict[str, Any],
    spec: VirtualOutputSpec,
    *,
    inspect_compositor_outputs: bool,
    expect_workflow: bool,
    expect_read_only: bool = False,
    expect_hybrid_pointer: bool = False,
) -> None:
    if (
        result.get("platform") != "wayland"
        or result.get("xwaylandReachable") is not True
        or result.get("windowExposed") is not True
    ):
        raise RuntimeError("probe did not expose mapped Wayland windows plus XWayland")
    qt_outputs = result.get("outputs")
    if not isinstance(qt_outputs, list) or len(qt_outputs) != spec.output_count:
        raise RuntimeError(
            f"Qt observed {len(qt_outputs) if isinstance(qt_outputs, list) else 'invalid'} "
            f"outputs; expected {spec.output_count}"
        )
    if (
        not all(isinstance(output, dict) for output in qt_outputs)
        or any(
            output.get("width") != spec.logical_width
            or output.get("height") != spec.logical_height
            or not _close(output.get("bufferScale"), math.ceil(spec.scale))
            for output in qt_outputs
        )
    ):
        raise RuntimeError(
            "Qt output geometry/buffer scale did not match "
            f"{spec.logical_width}x{spec.logical_height}@{math.ceil(spec.scale)}"
        )

    if inspect_compositor_outputs:
        compositor_outputs = result.get("compositorOutputs")
        if (
            result.get("compositorService") is not True
            or result.get("compositorKWinAbi") != KWIN_ABI
            or result.get("compositorInputObserverActive") is not True
            or result.get("compositorInputConsumesEvents") is not False
            or not isinstance(result.get("compositorInputDevices"), list)
        ):
            raise RuntimeError("the release-matched compositor output/input endpoint was unavailable")
        if (
            not isinstance(compositor_outputs, list)
            or len(compositor_outputs) != spec.output_count
        ):
            raise RuntimeError(
                "compositor output inventory did not match the requested enabled output count"
            )
        for output in compositor_outputs:
            geometry = output.get("geometry") if isinstance(output, dict) else None
            if (
                not isinstance(geometry, dict)
                or not _close(geometry.get("width"), spec.logical_width)
                or not _close(geometry.get("height"), spec.logical_height)
                or not _close(output.get("scale"), spec.scale)
            ):
                raise RuntimeError(
                    "compositor output geometry/scale did not match "
                    f"{spec.logical_width}x{spec.logical_height}@{spec.scale:g}"
                )
    if expect_workflow and result.get("compositorWorkflow") is not True:
        raise RuntimeError("the compositor container workflow did not pass")
    if expect_workflow and (
        result.get("compositorControlMode") != "development-test"
        or result.get("compositorMutationsEnabled") is not True
    ):
        raise RuntimeError("the development workflow mutation gate was not enabled")
    if expect_read_only and (
        result.get("compositorWorkflow") is not True
        or result.get("compositorControlMode") != "read-only"
        or result.get("compositorMutationsEnabled") is not False
    ):
        raise RuntimeError("the production control endpoint was not proven read-only")
    if expect_hybrid_pointer:
        validate_hybrid_pointer_evidence(result)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("launcher")
    parser.add_argument("probe")
    parser.add_argument("dbus_runner")
    parser.add_argument("scenario", type=Path)
    parser.add_argument("--plugin-root", type=Path, required=True)
    parser.add_argument("--inspect-compositor-outputs", action="store_true")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--expect-plugin", action="store_true")
    mode.add_argument("--expect-read-only", action="store_true")
    mode.add_argument("--expect-hybrid-pointer", metavar="DOTOOL", type=Path)
    return parser.parse_args()


def main() -> int:
    # AGENT-NOTE: Keep the disposable XDG tree, launcher lifecycle, and final
    # probe extraction together here. Feature-specific evidence assertions
    # belong in small validation modules such as hybrid_pointer_validation.py.
    arguments = parse_arguments()
    consent_error = host_uinput_consent_error(
        arguments.expect_hybrid_pointer is not None, os.environ
    )
    if consent_error is not None:
        print(consent_error, file=sys.stderr)
        return HOST_UINPUT_SKIP_CODE
    try:
        spec = load_virtual_spec(arguments.scenario)
    except ScenarioCoverageError as error:
        print(f"scenario is not representable: {error}", file=sys.stderr)
        return 2
    inspect_outputs = (
        arguments.inspect_compositor_outputs
        or arguments.expect_plugin
        or arguments.expect_read_only
        or arguments.expect_hybrid_pointer is not None
    )
    with tempfile.TemporaryDirectory(prefix="qindaqt-nested-test-") as directory:
        environment = isolated_environment(Path(directory))
        environment.pop(HOST_UINPUT_CONSENT_ENV, None)
        write_virtual_output_config(Path(environment["XDG_CONFIG_HOME"]), spec)
        if arguments.expect_plugin:
            environment["QINDAQT_EXPECT_COMPOSITOR_PLUGIN"] = "1"
        if arguments.expect_read_only:
            environment["QINDAQT_EXPECT_READ_ONLY_CONTROL"] = "1"
        if arguments.expect_hybrid_pointer is not None:
            environment["QINDAQT_EXPECT_HYBRID_POINTER"] = "1"
            environment["QINDAQT_DOTOOL"] = str(arguments.expect_hybrid_pointer)
        if inspect_outputs:
            environment["QINDAQT_EXPECT_COMPOSITOR_OUTPUTS"] = "1"
        # AGENT-GUARD: Integration tests exercise build artifacts, while the
        # launcher's default is intentionally the installed KDE plugin root.
        # Never infer a build layout from the launcher path here.
        launcher_command = [
            arguments.launcher,
            "--plugin-root",
            str(arguments.plugin_root),
        ]
        launcher_command.extend(
            [
                "--virtual",
                "--width",
                str(spec.logical_width),
                "--height",
                str(spec.logical_height),
                "--scale",
                f"{spec.scale:.12g}",
                "--output-count",
                str(spec.output_count),
            ]
        )
        # AGENT-GUARD: This mode must not pass the launcher marker that enables
        # the development mutation surface. The same display document is used
        # only to derive deterministic virtual dimensions for validation.
        if not arguments.expect_read_only:
            launcher_command.extend(["--test-scenario", str(arguments.scenario)])
        launcher_command.extend(
            [
                "--no-lockscreen",
                "--no-global-shortcuts",
                "--session",
                arguments.probe,
            ]
        )
        completed = subprocess.run(
            [arguments.dbus_runner, "--", *launcher_command],
            env=environment,
            text=True,
            capture_output=True,
            timeout=35 if arguments.expect_hybrid_pointer is not None else 20,
            check=False,
        )
    if completed.returncode != 0:
        print(completed.stdout, file=sys.stderr)
        print(completed.stderr, file=sys.stderr)
        return completed.returncode or 1
    try:
        result = extract_probe_result(completed.stdout)
        validate_probe_result(
            result,
            spec,
            inspect_compositor_outputs=inspect_outputs,
            expect_workflow=arguments.expect_plugin or arguments.expect_hybrid_pointer is not None,
            expect_read_only=arguments.expect_read_only,
            expect_hybrid_pointer=arguments.expect_hybrid_pointer is not None,
        )
    except (RuntimeError, json.JSONDecodeError) as error:
        print(f"nested session validation failed: {error}", file=sys.stderr)
        print(completed.stdout, file=sys.stderr)
        return 1
    result["scenarioCoverage"] = spec.coverage
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
