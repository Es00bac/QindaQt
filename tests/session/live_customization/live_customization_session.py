# SPDX-License-Identifier: GPL-3.0-or-later
"""In-session context and flows for the live-customization nested rows.

Starts the production shell on the proof profile inside the private session,
finds its panels through the compositor's development surface inventory,
drives the Meta+right-click chord and the resulting menus through the
development seat (keys and pointer only), reads the profile the shell
persists, captures the private compositor's framebuffer, and replays the same
intents through the Settings route's own editor host for the parity verdict.
"""

from __future__ import annotations

import configparser
import json
import os
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from shade_control import CompositorControl, PointerDriver, button, connect, key, pointer, wait_for
from shade_framebuffer import Frame, encode_png, select_fresh_buffer, swapchain_buffers

@dataclass(frozen=True)
class SessionConfig:
    output: Path
    flow: str
    shell: Path
    parity_tool: Path
    profile_dir: Path
    profile_id: str
    theme_dir: Path
    applet_dir: Path
    applet_policy: Path
    pixel_width: int
    pixel_height: int
    scale: float

    @classmethod
    def from_environment(cls) -> SessionConfig:
        env = os.environ
        return cls(output=Path(env["LC_OUT"]), flow=env["LC_FLOW"], shell=Path(env["LC_SHELL"]),
                   parity_tool=Path(env["LC_PARITY_TOOL"]), profile_dir=Path(env["LC_PROFILE_DIR"]),
                   profile_id=env["LC_PROFILE_ID"], theme_dir=Path(env["LC_THEME_DIR"]),
                   applet_dir=Path(env["LC_APPLET_DIR"]), applet_policy=Path(env["LC_APPLET_POLICY"]),
                   pixel_width=int(env["LC_PIXEL_WIDTH"]), pixel_height=int(env["LC_PIXEL_HEIGHT"]),
                   scale=float(env["LC_SCALE"]))

    @property
    def logical_size(self) -> tuple[float, float]:
        return (self.pixel_width / self.scale, self.pixel_height / self.scale)

    @property
    def user_profile(self) -> Path:
        return Path(os.environ["XDG_DATA_HOME"]) / "qindaqt" / "profiles" / f"{self.profile_id}.json"


class LiveSession:
    def __init__(self, config: SessionConfig) -> None:
        self.config = config
        self.evidence: dict[str, Any] = {
            "flow": config.flow, "scale": config.scale,
            "pixelSize": [config.pixel_width, config.pixel_height],
            "steps": [], "verdicts": {}, "captures": [], "intents": []}
        self._control: CompositorControl | None = None
        self._pointer: PointerDriver | None = None
        self._shell: subprocess.Popen[bytes] | None = None
        self._last_profile_bytes: bytes | None = None
        self._last_pointer: tuple[float, float] = (0.0, 0.0)

    @property
    def control(self) -> CompositorControl:
        assert self._control is not None
        return self._control

    @property
    def pointer(self) -> PointerDriver:
        assert self._pointer is not None
        return self._pointer

    # -- lifecycle --

    def start(self) -> None:
        self._control = connect()
        self._pointer = PointerDriver(self._control)
        maps = Path(f"/proc/{os.getppid()}/maps").read_text(encoding="utf-8")
        self.evidence["mappedQindaqtLibraries"] = sorted({
            line.split()[-1] for line in maps.splitlines() if "qindaqt" in line and ".so" in line})
        capabilities = self.control.capabilities()
        self.step("capabilities", controlMode=capabilities.get("controlMode"),
                  developmentInput=capabilities.get("developmentInput"))
        # AGENT-GUARD: the launcher exports QINDAQT_DEVELOPMENT_CONTROL=1 to
        # this program; a shell started with it insists on the supervised
        # notification authority this row does not provide, so drop it.
        # AGENT-CONTRACT: the shell runs exactly as in production for layout
        # purposes: no --profile (an explicit profile locks live adoption
        # for the process) and no --profile-dir (an explicit catalog excludes
        # the user store). The fixture reaches it as a distribution profile
        # through a private XDG_DATA_DIRS entry, and the user store under
        # XDG_DATA_HOME starts absent so the first-write adoption is proven.
        environment = dict(os.environ)
        environment.pop("QINDAQT_DEVELOPMENT_CONTROL", None)
        fixture_root = self.config.output / "fixture-data"
        fixture_profiles = fixture_root / "qindaqt" / "profiles"
        fixture_profiles.mkdir(parents=True, exist_ok=True)
        source = self.config.profile_dir / f"{self.config.profile_id}.json"
        (fixture_profiles / source.name).write_bytes(source.read_bytes())
        environment["XDG_DATA_DIRS"] = str(fixture_root) + ":" + environment.get(
            "XDG_DATA_DIRS", "/usr/local/share:/usr/share")
        self._shell = subprocess.Popen(
            [str(self.config.shell), "--theme", "qinda-dark",
             "--theme-dir", str(self.config.theme_dir),
             "--applet-dir", str(self.config.applet_dir),
             "--applet-policy", str(self.config.applet_policy)],
            env=environment,
            stdout=open(self.config.output / "shell-stdout.log", "wb"),
            stderr=open(self.config.output / "shell-stderr.log", "wb"))
        wait_for("shell panels mapped", lambda: len(self.mapped_panels()) >= 2, 30)
        time.sleep(1.5)
        self.step("panels", panels=self.mapped_panels())

    def close(self) -> None:
        if self._shell is not None and self._shell.poll() is None:
            self._shell.terminate()
            try:
                self._shell.wait(5)
            except subprocess.TimeoutExpired:
                self._shell.kill()
        if self._shell is not None:
            self.evidence["shellExit"] = self._shell.returncode

    def step(self, name: str, **data: Any) -> None:
        self.evidence["steps"].append({"step": name, **data})

    def verdict(self, name: str, value: bool) -> None:
        self.evidence["verdicts"][name] = bool(value)

    def write_evidence(self) -> None:
        log = self.config.output / "shell-stderr.log"
        if log.exists():
            self.evidence["shellStderrTail"] = log.read_text(encoding="utf-8", errors="replace")[-4000:]
        (self.config.output / "evidence.json").write_text(
            json.dumps(self.evidence, indent=2, default=str), encoding="utf-8")

    # -- observations --

    def mapped_panels(self) -> list[dict[str, Any]]:
        reply = self.control.call("DevelopmentShellSurfaces")
        if reply.get("status") not in ("ok", None) and "surfaces" not in reply:
            return []
        return [surface for surface in reply.get("surfaces", [])
                if surface.get("scope") == "dock" and surface.get("mapped")]

    def panel_rects(self) -> dict[str, dict[str, float]]:
        """The two proof panels by role: the top `bar` and the bottom `tray`."""
        panels = sorted(self.mapped_panels(), key=lambda surface: surface["geometry"]["y"])
        if len(panels) < 2:
            raise RuntimeError("both proof panels must be mapped")
        return {"bar": panels[0]["geometry"], "tray": panels[-1]["geometry"]}

    def profile_bytes(self) -> bytes | None:
        path = self.config.user_profile
        return path.read_bytes() if path.exists() else None

    def profile(self) -> dict[str, Any]:
        raw = self.profile_bytes()
        return json.loads(raw.decode("utf-8")) if raw else {}

    def wait_profile_change(self, description: str, timeout: float = 10.0) -> dict[str, Any]:
        previous = self._last_profile_bytes

        def changed() -> bytes | None:
            current = self.profile_bytes()
            return current if current is not None and current != previous else None

        self._last_profile_bytes = wait_for(description, changed, timeout)
        return json.loads(self._last_profile_bytes.decode("utf-8"))

    @staticmethod
    def panel(profile: dict[str, Any], panel_id: str) -> dict[str, Any] | None:
        return next((panel for panel in profile.get("panels", []) if panel.get("id") == panel_id), None)

    @staticmethod
    def applet_ids(profile: dict[str, Any], panel_id: str) -> list[str]:
        panel = LiveSession.panel(profile, panel_id)
        return [applet["id"] for applet in (panel or {}).get("applets", [])]

    @staticmethod
    def applet_zone(profile: dict[str, Any], panel_id: str, applet_id: str) -> str | None:
        panel = LiveSession.panel(profile, panel_id) or {}
        for applet in panel.get("applets", []):
            if applet["id"] == applet_id:
                return applet.get("settings", {}).get("zone", "start")
        return None

    def capture_still(self, name: str) -> Frame:
        """Capture without moving the pointer (keeps an open menu's hover state)."""
        width, height = self.config.pixel_width, self.config.pixel_height
        time.sleep(0.4)
        before = swapchain_buffers(os.getppid(), width, height)
        x, y = self._last_pointer
        self.pointer.move(x + 1, y)
        self._last_pointer = (x + 1, y)
        time.sleep(0.3)
        after = swapchain_buffers(os.getppid(), width, height)
        pixels, method = select_fresh_buffer(before, after)
        frame = Frame(pixels, width, height, self.config.scale, method)
        (self.config.output / f"{name}.png").write_bytes(encode_png(frame))
        self.evidence["captures"].append({"name": name, "method": method,
                                          "buffers": len(after), "sha256": frame.sha256})
        return frame

    def capture(self, name: str) -> Frame:
        width, height = self.config.pixel_width, self.config.pixel_height
        park = (self.config.logical_size[0] / 2, self.config.logical_size[1] / 2)
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

    # -- input --

    def chord(self, x: float, y: float) -> None:
        """Meta+right-click at a logical point, through the development seat."""
        self.control.inject([key("escape", True), key("escape", False)])
        time.sleep(0.15)
        self.control.inject([pointer(x, y)])
        self._last_pointer = (x, y)
        time.sleep(0.1)
        self.control.inject([key("left-meta", True)])
        time.sleep(0.05)
        self.control.inject([button("right", True)])
        time.sleep(0.05)
        self.control.inject([button("right", False)])
        time.sleep(0.05)
        self.control.inject([key("left-meta", False)])
        time.sleep(0.5)
        self.step("chord", x=x, y=y)

    def press(self, name: str, times: int = 1) -> None:
        for _ in range(times):
            self.control.inject([key(name, True), key(name, False)])
            time.sleep(0.12)

    def menu_select(self, index: int, count: int, sub_index: int | None = None,
                    sub_count: int | None = None, capture_name: str | None = None) -> None:
        """Activate entry `index` of the open menu (of `count` entries), or entry
        `sub_index` of that entry's submenu (`sub_count` entries; -1 = last).

        AGENT-GUARD: QQC2 menus neither wrap nor agree on whether a freshly
        opened menu already has a current entry (a popup that maps under or
        next to the pointer pre-hovers entry 0). Walking past the end with
        Down and back up with Up converges on the same entry either way.
        """
        self.press("down", count + 1)
        self.press("up", count - 1 - index)
        if capture_name is not None:
            self.capture_still(f"{capture_name}-entry")
        if sub_index is not None:
            self.press("right")
            time.sleep(0.25)
            if sub_index < 0:
                self.press("down", 40)
            else:
                assert sub_count is not None
                self.press("down", sub_count + 1)
                self.press("up", sub_count - 1 - sub_index)
            if capture_name is not None:
                self.capture_still(f"{capture_name}-subentry")
        self.press("enter")
        time.sleep(0.6)
        self.step("menu-select", index=index, count=count, subIndex=sub_index, subCount=sub_count)

    def drag(self, start: tuple[float, float], end: tuple[float, float]) -> None:
        self.control.inject([pointer(*start)])
        time.sleep(0.1)
        self.control.inject([button("left", True)])
        time.sleep(0.1)
        for index in range(1, 16):
            progress = index / 15
            self.control.inject([pointer(start[0] + (end[0] - start[0]) * progress,
                                          start[1] + (end[1] - start[1]) * progress)])
            time.sleep(0.04)
        time.sleep(0.2)
        self.control.inject([button("left", False)])
        time.sleep(0.6)
        self.step("drag", start=list(start), end=list(end))

    # -- parity --

    def parity(self, name: str, intents: list[dict[str, Any]]) -> None:
        """Replay `intents` through the Settings route's host and compare bytes."""
        intents_path = self.config.output / f"{name}-intents.json"
        intents_path.write_text(json.dumps(intents, indent=2), encoding="utf-8")
        output_dir = self.config.output / f"{name}-settings-route"
        output_dir.mkdir(exist_ok=True)
        completed = subprocess.run(
            [str(self.config.parity_tool), "--profile-dir", str(self.config.profile_dir),
             "--profile-id", self.config.profile_id, "--applet-dir", str(self.config.applet_dir),
             "--output-dir", str(output_dir), "--intents", str(intents_path),
             "--width", str(int(self.config.logical_size[0])),
             "--height", str(int(self.config.logical_size[1])), "--scale", f"{self.config.scale:.12g}"],
            text=True, capture_output=True, timeout=30, check=False)
        self.step("parity-tool", exit=completed.returncode, stdout=completed.stdout.strip(),
                  stderr=completed.stderr.strip()[-2000:])
        self.evidence["intents"].append({"name": name, "steps": intents})
        route_bytes = (output_dir / f"{self.config.profile_id}.json").read_bytes() \
            if completed.returncode == 0 else b""
        shell_bytes = self.profile_bytes() or b"\x00"
        self.verdict(f"{name}-parity-with-settings-route",
                     completed.returncode == 0 and route_bytes == shell_bytes)
