# SPDX-License-Identifier: GPL-3.0-or-later
"""Compositor1 control and development-input helpers for shade-visibility rows.

Only the documented development-test surface is used: `Windows`,
`Capabilities`, and `InjectTestInput` (docs/wiki/reference/
compositor-control-v1.md). Input is injected through KWin's normal input chain
inside the private session; host uinput is never involved.
"""

from __future__ import annotations

import json
import time
from collections.abc import Callable
from typing import Any

import dbus

SERVICE = "org.qindaqt.Compositor"
OBJECT_PATH = "/org/qindaqt/Compositor"
INTERFACE = "org.qindaqt.Compositor1"

# AGENT-GUARD: KWinGroupContextMenu::prepare() order is Saved workspaces…(0),
# Arrange windows(1), Detach active window(2), Ungroup(3), Minimize group(4),
# Roll up/Unroll group(5). Keyboard navigation selects by position, so a new
# leading entry silently retargets these rows; keep this in step with
# tests/session/hybridpointershade.cpp.
MINIMIZE_GROUP_MENU_INDEX = 4
ROLL_UP_MENU_INDEX = 5

# Shared title row of a container: outerBorder(1) + titleBarHeight(28) above
# its member frames (kwinhybridsession.cpp sceneMetrics()).
SHARED_ROW_HEIGHT = 29.0

Rect = tuple[float, float, float, float]


class CompositorControl:
    """Blocking client for the private session's Compositor1 endpoint."""

    def __init__(self) -> None:
        proxy = dbus.SessionBus().get_object(SERVICE, OBJECT_PATH)
        self._interface = dbus.Interface(proxy, INTERFACE)

    def call(self, method: str, *arguments: Any) -> dict[str, Any]:
        reply = getattr(self._interface, method)(*arguments, byte_arrays=True)
        return json.loads(bytes(reply).decode("utf-8"))

    def capabilities(self) -> dict[str, Any]:
        return self.call("Capabilities")

    def hybrid(self) -> dict[str, Any]:
        return self.capabilities().get("hybrid", {})

    def windows(self) -> dict[str, dict[str, Any]]:
        reply = self.call("Windows")
        if reply.get("status") != "ok":
            raise RuntimeError(f"Windows failed: {reply}")
        return {window["title"]: window for window in reply["windows"]}

    def inject(self, events: list[dict[str, Any]]) -> None:
        payload = json.dumps({"schemaVersion": 1, "events": events}).encode("utf-8")
        reply = self.call("InjectTestInput", dbus.ByteArray(payload))
        if reply.get("status") != "injected":
            raise RuntimeError(f"InjectTestInput rejected: {reply}")


def connect(timeout: float = 20.0) -> CompositorControl:
    def attempt() -> CompositorControl | None:
        control = CompositorControl()
        return control if control.capabilities() else None

    return wait_for("compositor control", attempt, timeout)


def wait_for(description: str, predicate: Callable[[], Any], timeout: float = 8.0) -> Any:
    """Poll `predicate` until it returns a truthy value or `timeout` expires."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            result = predicate()
        except (dbus.exceptions.DBusException, KeyError):
            result = None
        if result:
            return result
        time.sleep(0.1)
    raise RuntimeError(f"timed out waiting for {description}")


def rect(geometry: dict[str, float]) -> Rect:
    return (geometry["x"], geometry["y"], geometry["width"], geometry["height"])


def content_point(geometry: dict[str, float]) -> tuple[float, float]:
    """A point well inside a member's client content, below any title bar."""
    x, y, width, height = rect(geometry)
    return (x + width / 2, y + height * 0.6)


def united(frames: list[dict[str, float]]) -> Rect:
    boxes = [rect(frame) for frame in frames]
    left = min(box[0] for box in boxes)
    top = min(box[1] for box in boxes)
    right = max(box[0] + box[2] for box in boxes)
    bottom = max(box[1] + box[3] for box in boxes)
    return (left, top, right - left, bottom - top)


# Client-side-decorated toolkits accept pointer input in invisible resize
# borders beyond their frame geometry, so KWin's hit test can pick a window
# above a point that looks uncovered by frames alone.
INPUT_MARGIN = 32.0

# Candidate fractions, centre first, then outward to near the edges, so a thin
# uncovered band beside an overlapping window is still found.
_GRID = (0.5, 0.42, 0.58, 0.34, 0.66, 0.26, 0.74, 0.18, 0.82, 0.1, 0.9, 0.05, 0.95)


def _contains(frame: Rect, point: tuple[float, float]) -> bool:
    x, y, width, height = frame
    return x <= point[0] <= x + width and y <= point[1] <= y + height


def uncovered(inventory: dict[str, dict[str, Any]], title: str, point: tuple[float, float],
              ignore: tuple[str, ...] = ()) -> bool:
    """Whether `point` lies on `title` and no shown window stacked above it covers it.

    AGENT-NOTE: KWin's placement of new clients is not deterministic (it may
    centre or offset overlapping windows), so gesture points are derived from
    live stacking and frames, never fixed fractions. A grouped window also
    covers its container's shared row and border, which no inventory frame has,
    and every window above covers its INPUT_MARGIN resize border.
    """
    if not _contains(rect(inventory[title]["geometry"]), point):
        return False
    index = inventory[title]["stackIndex"]
    container = inventory[title].get("containerId")
    for other, window in inventory.items():
        if other == title or other in ignore or window["stackIndex"] <= index \
                or window["hidden"] or window["minimized"]:
            continue
        ox, oy, ow, oh = rect(window["geometry"])
        if container and window.get("containerId") == container:
            pass  # Tiled siblings never overlap: only their exact frame counts.
        else:
            if window.get("containerId"):
                oy, oh = oy - SHARED_ROW_HEIGHT, oh + SHARED_ROW_HEIGHT
            ox, oy, ow, oh = (ox - INPUT_MARGIN, oy - INPUT_MARGIN,
                              ow + 2 * INPUT_MARGIN, oh + 2 * INPUT_MARGIN)
        if _contains((ox, oy, ow, oh), point):
            return False
    return True


def title_point(inventory: dict[str, dict[str, Any]], title: str) -> tuple[float, float] | None:
    """An uncovered point in `title`'s top title/header band."""
    x, y, width, _ = rect(inventory[title]["geometry"])
    for fraction in _GRID:
        point = (x + width * fraction, y + 12)
        if uncovered(inventory, title, point):
            return point
    return None


def dock_drop_point(inventory: dict[str, dict[str, Any]], source: str,
                    target: str) -> tuple[float, float] | None:
    """A left/right/bottom edge drop point on `target` that only `source` may cover.

    The docking resolver ignores the dragged source itself, so the source's
    own frame never blocks a drop point.
    """
    x, y, width, height = rect(inventory[target]["geometry"])
    for along in _GRID:
        for point in ((x + width * 0.08, y + height * along),
                      (x + width * 0.92, y + height * along),
                      (x + width * along, y + height * 0.92)):
            if uncovered(inventory, target, point, ignore=(source,)):
                return point
    return None


def uncovered_content_point(inventory: dict[str, dict[str, Any]],
                            title: str) -> tuple[float, float] | None:
    x, y, width, height = rect(inventory[title]["geometry"])
    for fy in _GRID:
        for fx in _GRID:
            if fy < 0.2:
                continue  # stay below client-side header controls
            point = (x + width * fx, y + height * fy)
            if uncovered(inventory, title, point):
                return point
    return None


def pointer(x: float, y: float) -> dict[str, Any]:
    return {"type": "pointer-absolute", "x": float(x), "y": float(y)}


def button(name: str, pressed: bool) -> dict[str, Any]:
    return {"type": "button", "button": name, "pressed": pressed}


def key(name: str, pressed: bool) -> dict[str, Any]:
    return {"type": "key", "key": name, "pressed": pressed}


class PointerDriver:
    """Real pointer/keyboard gestures through the development input device."""

    def __init__(self, control: CompositorControl) -> None:
        self._control = control

    def move(self, x: float, y: float) -> None:
        self._control.inject([pointer(x, y)])

    def click(self, x: float, y: float, name: str = "left") -> None:
        self._control.inject([pointer(x, y)])
        time.sleep(0.05)
        self._control.inject([button(name, True)])
        time.sleep(0.05)
        self._control.inject([button(name, False)])
        time.sleep(0.25)

    def drag(self, start: tuple[float, float], end: tuple[float, float],
             meta_shift: bool = False) -> None:
        self._control.inject([pointer(*start)])
        time.sleep(0.05)
        if meta_shift:
            self._control.inject([key("left-meta", True), key("left-shift", True)])
        self._control.inject([button("left", True)])
        for index in range(1, 13):
            progress = index / 12
            self._control.inject([pointer(start[0] + (end[0] - start[0]) * progress,
                                          start[1] + (end[1] - start[1]) * progress)])
            time.sleep(0.02)
        self._control.inject([button("left", False)])
        if meta_shift:
            self._control.inject([key("left-shift", False), key("left-meta", False)])
        time.sleep(0.4)

    def group_menu_action(self, point: tuple[float, float], index: int,
                          while_open: Callable[[], None] | None = None) -> None:
        """Open the group context menu at `point` and activate entry `index`."""
        self.click(point[0], point[1], "right")
        time.sleep(0.2)
        if while_open:
            while_open()
        for _ in range(index + 1):
            self._control.inject([key("down", True), key("down", False)])
            time.sleep(0.1)
        self._control.inject([key("enter", True), key("enter", False)])
        time.sleep(0.5)
