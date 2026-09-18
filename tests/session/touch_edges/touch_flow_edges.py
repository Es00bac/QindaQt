# SPDX-License-Identifier: GPL-3.0-or-later
"""Touch edge gestures (ADR-0205), judged by the compositor's own announcement.

dbus-monitor watches org.qindaqt.Compositor1 on the private bus. A finger is
then swiped in from each edge of the output: left and top and bottom carry
their default actions (overview, notifications, task switcher) and must be
announced as EdgeGestureTriggered(edge, action); the right edge has no
action and an interior swipe never started on an edge, so both stay silent.
"""

from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
import time
from pathlib import Path
from typing import Any

from shade_session import ShadeSession
from touch_flow_chrome import FingerDriver, _attempt

# KWin activates an edge swipe after 44 logical px; go well past it.
SWIPE_DISTANCE = 160.0
SETTLE_SECONDS = 1.2

MATCH = "type='signal',interface='org.qindaqt.Compositor1',member='EdgeGestureTriggered'"


class SignalRecorder:
    """dbus-monitor on the private session bus, parsed into (edge, action)."""

    def __init__(self, output: Path) -> None:
        self._log = output / "edge-gestures-dbus-monitor.log"
        self._file = open(self._log, "w", encoding="utf-8")  # noqa: SIM115 - lifetime is the flow
        self._process = subprocess.Popen(
            [shutil.which("dbus-monitor") or "dbus-monitor", "--session", MATCH],
            stdout=self._file, stderr=subprocess.STDOUT, env=dict(os.environ))
        time.sleep(0.6)

    def gestures(self) -> list[tuple[str, str]]:
        self._file.flush()
        text = self._log.read_text(encoding="utf-8")
        found: list[tuple[str, str]] = []
        for block in re.split(r"^signal ", text, flags=re.M)[1:]:
            if "member=EdgeGestureTriggered" not in block:
                continue
            strings = re.findall(r'^\s+string "([^"]*)"', block, flags=re.M)
            if len(strings) >= 2:
                found.append((strings[0], strings[1]))
        return found

    def close(self) -> None:
        self._process.terminate()
        try:
            self._process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            self._process.kill()
        self._file.close()


def _swipe(fingers: FingerDriver, start: tuple[float, float], end: tuple[float, float]) -> None:
    fingers.drag(start, end)
    time.sleep(SETTLE_SECONDS)


def _new_gestures(recorder: SignalRecorder, before: int) -> list[tuple[str, str]]:
    return recorder.gestures()[before:]


def _expect_gesture(session: ShadeSession, recorder: SignalRecorder, fingers: FingerDriver, name: str,
                    start: tuple[float, float], end: tuple[float, float], expected: tuple[str, str] | None,
                    verdict: str) -> None:
    before = len(recorder.gestures())
    _swipe(fingers, start, end)
    seen = _new_gestures(recorder, before)
    session.step(name, start=list(start), end=list(end), announced=[list(g) for g in seen])
    session.verdict(verdict, seen == ([expected] if expected else []))


def run(session: ShadeSession) -> None:
    fingers = FingerDriver(session)
    width, height = session.config.logical_size
    recorder = SignalRecorder(session.config.output)
    try:
        reach = SWIPE_DISTANCE
        steps: list[tuple[str, tuple[float, float], tuple[float, float], tuple[str, str] | None, str]] = [
            ("swipe-from-left", (1.0, height / 2), (1.0 + reach, height / 2), ("left", "overview"),
             "leftEdgeAnnouncesOverview"),
            ("swipe-from-top", (width / 2, 1.0), (width / 2, 1.0 + reach), ("top", "notifications"),
             "topEdgeAnnouncesNotifications"),
            ("swipe-from-bottom", (width / 2, height - 1.0), (width / 2, height - 1.0 - reach),
             ("bottom", "task-switcher"), "bottomEdgeAnnouncesTaskSwitcher"),
            ("swipe-from-right", (width - 1.0, height / 2), (width - 1.0 - reach, height / 2), None,
             "rightEdgeStaysSilent"),
            ("swipe-interior", (width / 2, height / 2), (width / 2 + reach, height / 2), None,
             "interiorSwipeStaysSilent"),
        ]
        for name, start, end, expected, verdict in steps:
            _attempt(session, name, (verdict,),
                     lambda n=name, s=start, e=end, x=expected, v=verdict: _expect_gesture(
                         session, recorder, fingers, n, s, e, x, v))
        session.step("final", announced=[list(g) for g in recorder.gestures()])
        session.evidence["edgeGestures"] = [list(g) for g in recorder.gestures()]
    finally:
        recorder.close()


def summary(session: ShadeSession) -> dict[str, Any]:
    return {"verdicts": session.evidence.get("verdicts", {}),
            "gestures": json.dumps(session.evidence.get("edgeGestures", []))}
