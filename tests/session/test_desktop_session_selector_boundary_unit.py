# SPDX-License-Identifier: GPL-3.0-or-later
"""Hostile discovery and causal-order regressions for the S3 selector."""

from __future__ import annotations

import tempfile
import unittest
from argparse import Namespace
from pathlib import Path, PurePosixPath
from unittest.mock import Mock, patch

from desktop_session_host_tools import tool_root
from desktop_session_matrix import load_matrix_scenario
from desktop_session_output import OutputInventoryError
from desktop_session_runtime import _add_interactive_evidence
from desktop_session_sandbox import (
    ReadOnlyMount,
    create_run_root,
    remove_run_root,
)
from test_desktop_session_nested import _make_spec


def executable(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("fixture\n", encoding="utf-8")
    path.chmod(0o700)


class SelectorBoundaryTests(unittest.TestCase):
    def test_relocated_kscreen_tools_are_explicit_read_only_inputs(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            prefix = root / "relocated/runtime/usr"
            tools = {
                name: prefix / "bin" / name
                for name in (
                    "python3", "dbus-daemon", "kwin_wayland", "weston",
                    "weston-screenshooter", "kscreen-doctor",
                )
            }
            for tool in tools.values():
                executable(tool)
            for module in (
                prefix / "lib64/libweston-15/headless-backend.so",
                prefix / "lib64/weston/kiosk-shell.so",
            ):
                executable(module)
            backend = (
                prefix / "lib64/qt6/plugins/kf6/kscreen/KSC_KWayland.so"
            )
            executable(backend)
            build = root / "build"
            stage = root / "stage"
            source = root / "source"
            build.mkdir()
            stage.mkdir()
            source.mkdir()
            probe = root / "probe"
            bwrap = root / "bwrap"
            executable(probe)
            executable(bwrap)
            run_id = "d" * 32
            paths = create_run_root(build, run_id)
            arguments = Namespace(
                python=tools["python3"],
                dbus_daemon=tools["dbus-daemon"],
                kwin_wayland=tools["kwin_wayland"],
                weston=tools["weston"],
                weston_screenshooter=tools["weston-screenshooter"],
                kscreen_doctor=tools["kscreen-doctor"],
                kscreen_wayland_backend=backend,
                interactive=True,
                scenario_id="dual-1080p-horizontal",
                bin_directory="bin",
                plugin_relative="lib/qt6/plugins/qindaqt.so",
                decoration_relative="lib/qt6/plugins/qindaqt-decoration.so",
                settings_service_directory="share/dbus-1/services",
                audio_service_directory="share/dbus-1/services",
                bwrap=bwrap,
                stage_root=stage,
                source_root=source,
                probe=probe,
            )
            try:
                spec = _make_spec(arguments, run_id, paths)
                command = list(spec.command)
                self.assertEqual(
                    spec.environment["WESTON_MODULE_MAP"],
                    f"headless-backend.so={prefix}/lib64/libweston-15/headless-backend.so;"
                    f"kiosk-shell.so={prefix}/lib64/weston/kiosk-shell.so",
                )
                self.assertEqual(tool_root(backend), prefix)
                self.assertEqual(
                    command[command.index("--kscreen-doctor") + 1],
                    str(tools["kscreen-doctor"]),
                )
                self.assertEqual(
                    command[command.index("--kscreen-wayland-backend") + 1],
                    str(backend),
                )
                self.assertNotIn("/usr/bin/kscreen-doctor", command)
                self.assertNotIn(
                    "/usr/lib64/qt6/plugins/kf6/kscreen/KSC_KWayland.so",
                    command,
                )
                self.assertIn(
                    ReadOnlyMount(prefix, PurePosixPath(str(prefix))),
                    spec.system_mounts,
                )
            finally:
                remove_run_root(paths, build, run_id)

    def test_stale_post_selector_order_blocks_pointer_injection(self) -> None:
        source_root = Path(__file__).resolve().parents[2]
        scenario = load_matrix_scenario(source_root, "dual-1080p-horizontal")
        outputs = [
            {
                "name": f"WL-{output.ordinal}",
                "geometry": {
                    "x": output.logical_x,
                    "y": output.logical_y,
                    "width": output.logical_width,
                    "height": output.logical_height,
                },
                "scale": output.scale,
            }
            for output in scenario.outputs
        ]
        stale = {
            "schemaVersion": 1,
            "status": "ok",
            "outputGeneration": "8",
            "outputs": [
                {**outputs[0], "priority": 1},
                {**outputs[1], "priority": 2},
            ],
        }
        evidence = {
            "containment": {},
            "outputs": outputs,
            "generations": {"outputs": "7"},
        }
        launch = Namespace(
            parent_environment={}, app_environment={}, child_socket="qindaqt-child"
        )
        state = Mock()
        with (
            patch(
                "desktop_session_interaction_runtime._select_secondary_primary"
            ) as select,
            patch(
                "desktop_session_interaction_runtime._read_post_selector_outputs",
                return_value=stale,
            ) as read,
            patch("desktop_session_runtime._run_interaction") as interact,
        ):
            with self.assertRaisesRegex(
                OutputInventoryError, "not ordered WL-1 then WL-0"
            ):
                _add_interactive_evidence(
                    Namespace(), launch, state, scenario, evidence,
                    (None, None, None),
                )
        select.assert_called_once()
        read.assert_called_once()
        interact.assert_not_called()


if __name__ == "__main__":
    unittest.main()
