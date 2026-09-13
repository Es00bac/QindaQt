# SPDX-License-Identifier: GPL-3.0-or-later
"""In-session context shared by the shade-visibility flows.

Owns the Compositor1 connection, the development pointer, fixture processes,
framebuffer captures, client input observations, and the evidence document the
runner validates. Flows only describe scenario steps and verdicts.
"""

from __future__ import annotations

import json
import os
import signal
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from shade_control import (CompositorControl, DockGesture, PointerDriver, connect, plan_dock,
                           uncovered_content_point, wait_for)
from shade_fixtures import BACKDROP_COLOUR, BACKDROP_TITLE, FixtureLauncher
from shade_framebuffer import Frame, encode_png, select_fresh_buffer, swapchain_buffers

# Where the backdrop is clicked to give it focus and a fresh input serial
# before it mints an activation token; far from every group this suite builds.
NEUTRAL_BACKDROP_POINT = (60.0, 60.0)


@dataclass(frozen=True)
class SessionConfig:
    output: Path
    kind: str
    flow: str
    pixel_width: int
    pixel_height: int
    scale: float

    @classmethod
    def from_environment(cls) -> SessionConfig:
        return cls(output=Path(os.environ["SHADE_OUT"]),
                   kind=os.environ["SHADE_KIND"],
                   flow=os.environ["SHADE_FLOW"],
                   pixel_width=int(os.environ["SHADE_PIXEL_WIDTH"]),
                   pixel_height=int(os.environ["SHADE_PIXEL_HEIGHT"]),
                   scale=float(os.environ["SHADE_SCALE"]))

    @property
    def logical_size(self) -> tuple[float, float]:
        return (self.pixel_width / self.scale, self.pixel_height / self.scale)


class ShadeSession:
    def __init__(self, config: SessionConfig) -> None:
        self.config = config
        self.event_log = config.output / "client-events.jsonl"
        self.token_file = config.output / "activation-token.txt"
        self.fixtures = FixtureLauncher(config.output, self.event_log, self.token_file)
        self.evidence: dict[str, Any] = {
            "kind": config.kind, "flow": config.flow, "scale": config.scale,
            "pixelSize": [config.pixel_width, config.pixel_height],
            "steps": [], "verdicts": {}, "captures": []}
        self._control: CompositorControl | None = None
        self._pointer: PointerDriver | None = None
        self._backdrop = None

    @property
    def control(self) -> CompositorControl:
        assert self._control is not None
        return self._control

    @property
    def pointer(self) -> PointerDriver:
        assert self._pointer is not None
        return self._pointer

    def start(self) -> None:
        self._control = connect()
        self._pointer = PointerDriver(self._control)
        # KWin runs this driver as its --exit-with-session child, so the parent
        # is exactly the private compositor under test.
        maps = Path(f"/proc/{os.getppid()}/maps").read_text(encoding="utf-8")
        libraries = sorted({line.split()[-1] for line in maps.splitlines()
                            if "qindaqt" in line and ".so" in line})
        capabilities = self.control.capabilities()
        self.evidence["mappedQindaqtLibraries"] = libraries
        self.step("capabilities", controlMode=capabilities.get("controlMode"),
                  developmentInput=capabilities.get("developmentInput"))
        self._backdrop = self.fixtures.gtk(BACKDROP_TITLE, BACKDROP_COLOUR, "maximized", 800, 600)
        wait_for("backdrop", lambda: BACKDROP_TITLE in self.control.windows(), 15)
        time.sleep(1.0)

    def step(self, name: str, **data: Any) -> None:
        self.evidence["steps"].append({"step": name, **data})

    def verdict(self, name: str, value: bool) -> None:
        self.evidence["verdicts"][name] = bool(value)

    def capture(self, name: str) -> Frame:
        """Capture the private compositor's current output and save a PNG."""
        width, height = self.config.pixel_width, self.config.pixel_height
        park = (self.config.logical_size[0] - 1, self.config.logical_size[1] - 1)
        self.pointer.move(*park)
        time.sleep(0.45)
        before = swapchain_buffers(os.getppid(), width, height)
        self.pointer.move(park[0] - 1, park[1])
        time.sleep(0.3)
        after = swapchain_buffers(os.getppid(), width, height)
        pixels, method = select_fresh_buffer(before, after)
        frame = Frame(pixels, width, height, self.config.scale, method)
        (self.config.output / f"{name}.png").write_bytes(encode_png(frame))
        self.evidence["captures"].append({"name": name, "method": method,
                                          "buffers": len(after), "sha256": frame.sha256})
        return frame

    def dock(self, source: str, target: str) -> None:
        """Group `source` with `target` through one real Meta+Shift drag.

        Every attempt re-plans from live inventory with `plan_dock`. A blocking
        window is raised by clicking its uncovered content first; raising the
        target and then the source always converges because the dragged source
        may cover the drop point. A source mapped entirely beneath its target
        is grouped by dragging the target onto it, recorded as `reversed`.
        """
        for _ in range(3):
            inventory = self.control.windows()
            plan = plan_dock(inventory, source, target)
            if isinstance(plan, DockGesture):
                self.step(f"dock-{source}-onto-{target}", start=list(plan.start),
                          drop=list(plan.drop), dragged=plan.dragged, onto=plan.onto,
                          reversed=plan.reversed,
                          members={title: window_summary(window)
                                   for title, window in inventory.items()})
                self.pointer.drag(plan.start, plan.drop, meta_shift=True)
                return
            if plan is None:
                break
            blocked = plan.title
            self.pointer.click(*plan.point)
            wait_for(f"{blocked} raised", lambda: self.control.windows()[blocked]["active"], 5)
        raise RuntimeError(f"no uncovered dock gesture from {source} onto {target}")

    def wait_frames_settled(self, titles: list[str] | tuple[str, ...], timeout: float = 15.0) -> None:
        """Require identical frames for `titles` across consecutive inventory reads.

        AGENT-GUARD: KWin may still place a new client, and a Hybrid reflow may
        still be applying member frames, after the inventory first reports
        them. Gesture points computed from unsettled frames hit the wrong window.
        """
        previous: list[Any] = []

        def settled() -> bool:
            time.sleep(0.5)
            current = self.control.windows()
            frames = [current[title]["geometry"] for title in titles]
            stable = frames == previous
            previous[:] = frames
            return stable

        wait_for("member frames settled", settled, timeout)

    def activate(self, title: str) -> None:
        """Click an uncovered content point of `title` and require it to become active."""
        inventory = self.control.windows()
        point = uncovered_content_point(inventory, title)
        self.step(f"activate-{title}", point=list(point) if point else None,
                  members={name: window_summary(window) for name, window in inventory.items()})
        try:
            if point is None:
                raise RuntimeError(f"{title} has no uncovered content point")
            self.pointer.click(*point)
            wait_for(f"{title} active", lambda: self.control.windows()[title]["active"], 5)
        except RuntimeError:
            current = self.control.windows()
            self.step(f"activate-{title}-failed", hybrid=self.hybrid_summary(),
                      members={name: window_summary(window) for name, window in current.items()})
            self.capture(f"activate-{title}-failed")
            raise

    def client_events(self) -> list[dict[str, Any]]:
        if not self.event_log.exists():
            return []
        return [json.loads(line) for line in
                self.event_log.read_text(encoding="utf-8").splitlines() if line]

    def probe_click(self, label: str, point: tuple[float, float]) -> dict[str, Any]:
        """Click `point` and report which fixture clients received the press."""
        before = len(self.client_events())
        self.pointer.click(*point)
        time.sleep(0.25)
        receivers = [event["title"] for event in self.client_events()[before:]
                     if event["event"] == "press"]
        return {"label": label, "point": list(point), "pressReceivers": receivers,
                "active": self.active_titles()}

    def active_titles(self, inventory: dict[str, dict[str, Any]] | None = None) -> list[str]:
        inventory = inventory if inventory is not None else self.control.windows()
        return [title for title, window in inventory.items() if window["active"]]

    def mint_backdrop_token(self) -> str:
        """A real xdg-activation token minted by the focused backdrop client."""
        self.token_file.unlink(missing_ok=True)
        self.pointer.click(*NEUTRAL_BACKDROP_POINT)
        self._backdrop.send_signal(signal.SIGUSR2)
        return wait_for("activation token",
                        lambda: self.token_file.exists() and self.token_file.read_text().strip(), 5)

    def hybrid_summary(self) -> dict[str, Any]:
        hybrid = self.control.hybrid()
        return {key: hybrid.get(key) for key in (
            "containerCount", "shadedContainerCount", "shadedStripFrames",
            "visibleAnchoredChromeSceneItemCount", "publishedGroupStackingCount",
            "lastGroupStackingFailure")}

    def write_evidence(self) -> None:
        (self.config.output / "evidence.json").write_text(
            json.dumps(self.evidence, indent=2, default=str), encoding="utf-8")

    def close(self) -> None:
        self.fixtures.terminate_all()


def window_summary(window: dict[str, Any] | None) -> dict[str, Any] | None:
    if window is None:
        return None
    return {key: window.get(key) for key in (
        "title", "active", "hidden", "minimized", "stackIndex", "geometry", "targetGeometry",
        "serverDecorated", "containerId")}
