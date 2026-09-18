#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""KWin --exit-with-session program for the windowManagement bridge row (ADR-0209).

Proves the compositor half of the live bridge inside a private virtual session:
rewriting kwinrc's `[QindaQt] DockingModifier` and `[Windows] FocusPolicy` and
asking KWin to reconfigure (exactly what qindaqt-session does after a confirmed
Settings1 change) rebinds the docking chord and the focus policy without a
restart. Reuses the shade-visibility harness (session, GTK fixtures, planned
dock gestures); configuration arrives through SHADE_* variables.

AGENT-GUARD: every path here is the private session's own XDG root; the live
user's kwinrc is never touched because qindaqt-wm exports XDG_CONFIG_HOME for
the whole nested session.
"""

from __future__ import annotations

import os
import sys
import time
import traceback
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "shade_visibility"))

import dbus  # noqa: E402

from shade_control import DockGesture, button, key, plan_dock, pointer, wait_for  # noqa: E402
from shade_control import uncovered_content_point  # noqa: E402
from shade_session import (NEUTRAL_BACKDROP_POINT, SessionConfig, ShadeSession,  # noqa: E402
                           window_summary)

META_SHIFT = ("left-meta", "left-shift")
ALT_SHIFT = ("left-alt", "left-shift")
COLOURS = ("#b03a2e", "#2e5cb0", "#8a2eb0", "#b08a2e", "#2eb0a0", "#6e6e6e", "#b02e8a", "#2eb04a")


def kwinrc_path() -> Path:
    return Path(os.environ["XDG_CONFIG_HOME"]) / "kwinrc"


def write_kwinrc_entries(entries: dict[str, dict[str, str]]) -> str:
    """Set `group -> key -> value` in the private kwinrc, keeping everything else."""
    path = kwinrc_path()
    lines = path.read_text(encoding="utf-8").splitlines() if path.exists() else []
    output: list[str] = []
    pending = {group: dict(keys) for group, keys in entries.items()}
    current: str | None = None

    def flush(group: str | None) -> None:
        for name, value in sorted(pending.pop(group, {}).items()) if group else []:
            output.append(f"{name}={value}")

    for line in lines:
        stripped = line.strip()
        if stripped.startswith("[") and stripped.endswith("]"):
            flush(current)
            current = stripped[1:-1]
            output.append(line)
            continue
        name = stripped.split("=", 1)[0].strip() if "=" in stripped else None
        if current in pending and name in pending[current]:
            output.append(f"{name}={pending[current].pop(name)}")
            continue
        output.append(line)
    flush(current)
    for group in sorted(pending):
        if pending[group]:
            output.extend(["", f"[{group}]"] + [f"{k}={v}" for k, v in sorted(pending[group].items())])
    text = "\n".join(output).rstrip("\n") + "\n"
    path.write_text(text, encoding="utf-8")
    return text


def reconfigure_kwin() -> None:
    """The bridge's own signal: org.kde.KWin.reconfigure on the session bus."""
    proxy = dbus.SessionBus().get_object("org.kde.KWin", "/KWin")
    dbus.Interface(proxy, "org.kde.KWin").reconfigure()
    time.sleep(1.0)


def drag(session: ShadeSession, start: tuple[float, float], end: tuple[float, float],
         modifiers: tuple[str, ...]) -> None:
    control = session.control
    control.inject([pointer(*start)])
    time.sleep(0.05)
    control.inject([key(name, True) for name in modifiers])
    control.inject([button("left", True)])
    for index in range(1, 13):
        progress = index / 12
        control.inject([pointer(start[0] + (end[0] - start[0]) * progress,
                                start[1] + (end[1] - start[1]) * progress)])
        time.sleep(0.02)
    control.inject([button("left", False)])
    control.inject([key(name, False) for name in reversed(modifiers)])
    time.sleep(0.4)


def dock_attempt(session: ShadeSession, source: str, target: str,
                 modifiers: tuple[str, ...], label: str) -> None:
    """One planned dock gesture from `source` onto `target` with `modifiers` held."""
    for _ in range(3):
        inventory = session.control.windows()
        plan = plan_dock(inventory, source, target)
        if isinstance(plan, DockGesture):
            session.step(f"{label}-drag", modifiers=list(modifiers), start=list(plan.start),
                         drop=list(plan.drop), dragged=plan.dragged, onto=plan.onto,
                         members={t: window_summary(inventory[t]) for t in (source, target)})
            drag(session, plan.start, plan.drop, modifiers)
            return
        if plan is None:
            break
        session.pointer.click(*plan.point)
        blocked = plan.title
        wait_for(f"{blocked} raised", lambda: session.control.windows()[blocked]["active"], 5)
    raise RuntimeError(f"no uncovered dock gesture from {source} onto {target}")


def grouped(session: ShadeSession, first: str, second: str) -> bool:
    inventory = session.control.windows()
    a, b = inventory.get(first, {}), inventory.get(second, {})
    return bool(a.get("containerId")) and a.get("containerId") == b.get("containerId")


class Pair:
    """Two fresh CSD GTK members whose aspect ratios keep an uncovered area each."""

    counter = 0

    def __init__(self, session: ShadeSession) -> None:
        Pair.counter += 1
        index = Pair.counter
        self.session = session
        self.first = f"Chord source {index}"
        self.second = f"Chord target {index}"
        colours = COLOURS[(2 * index) % len(COLOURS)], COLOURS[(2 * index + 1) % len(COLOURS)]
        self.processes = {
            self.first: session.fixtures.gtk(self.first, colours[0], "csd", 700, 300),
            self.second: session.fixtures.gtk(self.second, colours[1], "csd", 360, 520)}
        wait_for("pair mapped", lambda: all(t in session.control.windows() for t in self.processes), 30)
        time.sleep(0.8)
        session.wait_frames_settled(list(self.processes))

    def close(self) -> None:
        for process in self.processes.values():
            if process.poll() is None:
                process.terminate()
        wait_for("pair unmapped",
                 lambda: not any(t in self.session.control.windows() for t in self.processes), 15)
        time.sleep(0.5)

    def docks_with(self, modifiers: tuple[str, ...], label: str) -> bool:
        dock_attempt(self.session, self.first, self.second, modifiers, label)
        try:
            wait_for(label, lambda: grouped(self.session, self.first, self.second), 6)
            return True
        except RuntimeError:
            return False

    def stays_apart_with(self, modifiers: tuple[str, ...], label: str) -> bool:
        dock_attempt(self.session, self.first, self.second, modifiers, label)
        time.sleep(2.0)
        return not grouped(self.session, self.first, self.second)


def hover_activates(session: ShadeSession, title: str, timeout: float) -> bool:
    """Leave for the backdrop, then enter `title`: KWin judges focus-follows-mouse
    on pointer-focus changes, so a pointer already resting over the window
    would never produce one."""
    inventory = session.control.windows()
    point = uncovered_content_point(inventory, title)
    if point is None:
        raise RuntimeError(f"{title} has no uncovered content point")
    session.pointer.move(*NEUTRAL_BACKDROP_POINT)
    time.sleep(0.6)
    session.pointer.move(point[0] - 4, point[1] - 4)
    time.sleep(0.1)
    session.pointer.move(*point)
    try:
        wait_for(f"{title} active by hover", lambda: session.control.windows()[title]["active"],
                 timeout)
        return True
    except RuntimeError:
        return False


def run(session: ShadeSession) -> None:
    session.step("kwinrc-initial", text=kwinrc_path().read_text(encoding="utf-8"))
    pair = Pair(session)
    session.verdict("default-chord-docks", pair.docks_with(META_SHIFT, "default-meta-shift"))
    pair.close()

    session.step("kwinrc-alt", text=write_kwinrc_entries({"QindaQt": {"DockingModifier": "alt"}}))
    reconfigure_kwin()
    pair = Pair(session)
    session.verdict("old-chord-inert-after-rebind",
                    pair.stays_apart_with(META_SHIFT, "rebound-meta-shift"))
    session.verdict("new-chord-docks-after-rebind", pair.docks_with(ALT_SHIFT, "rebound-alt-shift"))
    pair.close()

    session.step("kwinrc-disabled",
                 text=write_kwinrc_entries({"QindaQt": {"DockingModifier": "disabled"}}))
    reconfigure_kwin()
    pair = Pair(session)
    session.verdict("disabled-ignores-meta-shift", pair.stays_apart_with(META_SHIFT, "disabled-meta"))
    session.verdict("disabled-ignores-alt-shift", pair.stays_apart_with(ALT_SHIFT, "disabled-alt"))
    pair.close()

    # Back to the shipped chord, plus KWin's own focus policy through the
    # same file and the same reconfigure.
    pair = Pair(session)
    session.activate(pair.first)
    session.verdict("click-to-focus-ignores-hover", not hover_activates(session, pair.second, 1.5))
    session.step("kwinrc-super-ffm", text=write_kwinrc_entries(
        {"QindaQt": {"DockingModifier": "super"}, "Windows": {"FocusPolicy": "FocusFollowsMouse"}}))
    reconfigure_kwin()
    session.verdict("focus-follows-mouse-after-reconfigure", hover_activates(session, pair.second, 4))
    session.verdict("focus-follows-mouse-back-again", hover_activates(session, pair.first, 4))
    session.verdict("restored-chord-docks", pair.docks_with(META_SHIFT, "restored-meta-shift"))
    session.step("kwinrc-final", text=kwinrc_path().read_text(encoding="utf-8"))
    pair.close()


def main() -> int:
    config = SessionConfig.from_environment()
    config.output.mkdir(parents=True, exist_ok=True)
    session = ShadeSession(config)
    status = 0
    try:
        session.start()
        run(session)
        session.evidence["result"] = "completed"
    except Exception as error:  # noqa: BLE001 - recorded as evidence for the runner
        session.evidence["result"] = f"failed: {error}"
        session.evidence["traceback"] = traceback.format_exc()
        status = 1
    finally:
        session.write_evidence()
        session.close()
    return status


if __name__ == "__main__":
    raise SystemExit(main())
