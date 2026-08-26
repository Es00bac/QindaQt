# SPDX-License-Identifier: GPL-3.0-or-later
"""Translate a validated scenario into a backend-specific launch plan."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from .scenarios import Scenario


BACKEND_NAMES = ("preview", "wayland", "virtual", "weston", "xephyr")
REPOSITORY_ROOT = Path(__file__).resolve().parents[2]

# AGENT-CONTRACT: Backend plans cross from scenario data into process execution.
# Keep them as argument vectors; never introduce shell interpolation or divergence
# between the tokens shown by dry-run and those passed to subprocess.


@dataclass(frozen=True)
class BackendPlan:
    """One foreground process and limitations relevant to a development run."""

    backend: str
    argv: tuple[str, ...]
    notes: tuple[str, ...]
    environment: dict[str, str]


def _default_program(backend: str) -> str:
    if backend == "preview":
        developer_build = REPOSITORY_ROOT / "build" / "dev" / "src" / "shell" / "qindaqt-shell-preview"
        if developer_build.is_file():
            return str(developer_build)
    if backend in {"wayland", "virtual"}:
        developer_build = REPOSITORY_ROOT / "build" / "dev" / "src" / "session" / "qindaqt-wm"
        if developer_build.is_file():
            return str(developer_build)
    return {
        "preview": "qindaqt-shell-preview",
        "wayland": "qindaqt-wm",
        "virtual": "qindaqt-wm",
        "weston": "weston",
        "xephyr": "Xephyr",
    }[backend]


def build_plan(
    scenario: Scenario,
    backend: str,
    *,
    program: str | None = None,
    xephyr_display: str = ":99",
    smoke_test: bool = False,
) -> BackendPlan:
    """Create a side-effect-free launch plan for one supported backend."""
    if backend not in BACKEND_NAMES:
        raise ValueError(f"unsupported backend {backend!r}")
    executable = program or _default_program(backend)
    scenario_path = str(scenario.path)
    width, height = scenario.canvas_size
    environment = {
        "QINDAQT_DEV_BACKEND": backend,
        "QINDAQT_TEST_SCENARIO": scenario_path,
    }
    if backend == "preview":
        argv = (
            executable,
            "--profile",
            scenario.profile,
            "--theme",
            scenario.theme,
            "--width",
            str(width),
            "--height",
            str(height),
        )
        if smoke_test:
            argv = (*argv, "--list")
        notes = ("Shell-only profile preview; it does not exercise compositor behavior.",)
    elif backend in {"wayland", "virtual"}:
        primary = next(output for output in scenario.enabled_outputs if output.primary)
        mode = "--windowed" if backend == "wayland" else "--virtual"
        argv = (
            executable,
            mode,
            "--width",
            str(width),
            "--height",
            str(height),
            "--scale",
            str(primary.scale),
            "--output-count",
            str(len(scenario.enabled_outputs)),
            "--test-scenario",
            scenario_path,
            "--no-lockscreen",
            "--no-global-shortcuts",
        )
        if backend == "wayland":
            notes = (
                "Primary interactive path: QindaQt runs as a nested Wayland compositor.",
                "The compositor adapter replays per-output scenario state after KWin bootstraps its windows.",
            )
        else:
            environment.update({"KWIN_COMPOSE": "Q", "QT_QUICK_BACKEND": "software"})
            notes = (
                "Headless deterministic KWin path intended for automated input and screenshots.",
                "The compositor adapter replays per-output scenario state after virtual outputs exist.",
            )
    elif backend == "weston":
        socket_name = "qindaqt-weston-reference"
        argv = (
            executable,
            "--backend=headless-backend.so",
            f"--width={width}",
            f"--height={height}",
            f"--socket={socket_name}",
            "--idle-time=0",
        )
        environment["WAYLAND_DISPLAY"] = socket_name
        notes = (
            "Reference compositor path for shell/protocol checks, not QindaQt window-management tests.",
            "Weston represents the scenario as one enclosing canvas; scripted output events are ignored.",
        )
    else:
        argv = (
            executable,
            xephyr_display,
            "-screen",
            f"{width}x{height}x24",
            "-resizeable",
            "-terminate",
            "-nolisten",
            "tcp",
        )
        # AGENT-GUARD: DISPLAY must continue pointing at Xephyr's parent server.
        # Clients use QINDAQT_CHILD_DISPLAY after the nested server is ready.
        environment.update({"QINDAQT_CHILD_DISPLAY": xephyr_display, "XDG_SESSION_TYPE": "x11"})
        notes = (
            "Legacy X11 client/style path only; QindaQt's complete desktop is Wayland-native.",
            "Xephyr flattens multiple outputs into one canvas and does not replay output events.",
        )
    return BackendPlan(backend=backend, argv=argv, notes=notes, environment=environment)


def plan_as_dict(plan: BackendPlan, runtime_root: Path | str) -> dict[str, object]:
    """Serialize a plan without exposing unrelated inherited environment values."""
    return {
        "backend": plan.backend,
        "argv": list(plan.argv),
        "isolated_runtime_root": str(runtime_root),
        "environment_overrides": dict(sorted(plan.environment.items())),
        "notes": list(plan.notes),
    }
