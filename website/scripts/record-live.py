#!/usr/bin/env python3
"""Record public workflows inside an isolated, scenario-gated QindaQt session.

The shell launcher creates the private bus and virtual output. This process
never connects to the owner's desktop. PNG frames remain in ignored build/.
"""

from __future__ import annotations

import json
import os
import subprocess
import threading
import time
from pathlib import Path

from demo_input import inject


ROOT = Path(__file__).resolve().parents[2]
STAGE = ROOT / "build/site-capture/live"
SAMPLE = Path(os.environ["QINDA_SHOWCASE_SAMPLES"])


def move(x: int, y: int) -> None:
    inject([{"type": "pointer-absolute", "x": x, "y": y}])


def click(x: int, y: int) -> None:
    inject([{"type": "pointer-absolute", "x": x, "y": y},
            {"type": "button", "button": "left", "pressed": True},
            {"type": "button", "button": "left", "pressed": False}])


def key(name: str) -> None:
    inject([{"type": "key", "key": name, "pressed": True},
            {"type": "key", "key": name, "pressed": False}])


def drag(start: tuple[int, int], end: tuple[int, int], *, dock: bool = False) -> None:
    move(*start)
    pause(.2)
    if dock:
        inject([{"type": "key", "key": key_name, "pressed": True}
                for key_name in ("left-meta", "left-shift")])
    inject([{"type": "button", "button": "left", "pressed": True}])
    for step in range(1, 19):
        fraction = step / 18
        x = start[0] + (end[0] - start[0]) * fraction
        y = start[1] + (end[1] - start[1]) * fraction
        inject([{"type": "pointer-absolute", "x": x, "y": y}])
        pause(.055)
    inject([{"type": "button", "button": "left", "pressed": False}])
    if dock:
        inject([{"type": "key", "key": key_name, "pressed": False}
                for key_name in ("left-shift", "left-meta")])


def paste(value: str) -> None:
    subprocess.run(["wl-copy", "--type", "text/plain", value], check=True)


def pause(seconds: float = .65) -> None:
    time.sleep(seconds)


class Capture:
    def __init__(self, name: str):
        self.dir = STAGE / name
        self.dir.mkdir(parents=True, exist_ok=True)
        for old in self.dir.glob("*.png"):
            old.unlink()
        self.timestamps: list[float] = []
        self.running = False
        self.thread: threading.Thread | None = None

    def __enter__(self) -> "Capture":
        self.running = True
        self.started = time.monotonic()
        self.thread = threading.Thread(target=self._loop, daemon=True)
        self.thread.start()
        return self

    def _loop(self) -> None:
        while self.running:
            frame = self.dir / f"{len(self.timestamps):04}.png"
            subprocess.run(["spectacle", "-b", "-n", "-f", "-o", str(frame)],
                           check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            self.timestamps.append(time.monotonic() - self.started)
            time.sleep(.12)

    def __exit__(self, *_: object) -> None:
        self.running = False
        assert self.thread is not None
        self.thread.join()
        (self.dir / "timing.json").write_text(json.dumps(self.timestamps))
        print(f"{self.dir.name}: {len(self.timestamps)} frames in {self.timestamps[-1]:.1f}s", flush=True)


def app_scene(name: str, command: list[str], action) -> None:
    if os.environ.get("QINDA_SCENE") and os.environ["QINDA_SCENE"] != name:
        return
    log = (STAGE / f"{name}-app.log").open("w")
    app = subprocess.Popen(command, stdout=log, stderr=log)
    try:
        pause(2.7)
        with Capture(name):
            pause(.9)
            action()
            pause(1.1)
    finally:
        app.terminate()
        try:
            app.wait(timeout=3)
        except subprocess.TimeoutExpired:
            app.kill()
            app.wait()
        log.close()
        pause(.4)


def main() -> None:
    STAGE.mkdir(parents=True, exist_ok=True)
    if not os.environ.get("QINDA_SCENE") or os.environ["QINDA_SCENE"] == "desktop-welcome":
        with Capture("desktop-welcome"):
            move(367, 329)
            pause(.45)
            click(367, 329)  # The real Arrange tutorial page.
            pause(.8)
            move(367, 376)
            pause(.45)
            click(367, 376)  # The Groups page explains tabs and splits.
            pause(1.0)

    def settings() -> None:
        move(371, 661)
        pause(.4)
        click(371, 661)  # Appearance, followed by Customization.
        pause(1.5)
        move(373, 756)
        pause(.4)
        click(373, 756)
        pause(1.6)
        move(738, 449)
        pause(.45)
        click(738, 449)  # Apply Classic Taskbar.
        pause(1.7)
        move(887, 321)
        pause(.45)
        click(887, 321)  # Restore Menu and Dock.
        pause(1.6)
    app_scene("desktop-settings", ["qindaqt-settings"], settings)

    def files() -> None:
        move(550, 320)
        pause(.4)
        click(550, 320)
        pause(.75)
        inject([{"type": "pointer-axis", "axis": "vertical", "delta": 120.0}])
        pause(.75)
    app_scene("desktop-files", ["qindaqt-file-manager", "/usr/share/qindaqt/wallpapers"], files)

    if not os.environ.get("QINDA_SCENE") or os.environ["QINDA_SCENE"] == "desktop-windows":
        logs = [(STAGE / f"desktop-windows-{name}.log").open("w")
                for name in ("files", "settings")]
        apps = [subprocess.Popen(["qindaqt-file-manager", "/usr/share/qindaqt/wallpapers"],
                                 stdout=logs[0], stderr=logs[0])]
        try:
            pause(2.5)
            apps.append(subprocess.Popen(["qindaqt-settings"], stdout=logs[1], stderr=logs[1]))
            pause(2.7)
            with Capture("desktop-windows"):
                pause(.9)
                drag((640, 179), (960, 205))  # Expose both ordinary windows.
                pause(.8)
                drag((960, 205), (288, 401), dock=True)  # Split at the file window edge.
                pause(2.0)
        finally:
            for app in apps:
                app.terminate()
                try:
                    app.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    app.kill()
                    app.wait()
            for log in logs:
                log.close()
            pause(.4)

    def write() -> None:
        move(656, 609)
        pause(.4)
        click(656, 609)
        pause(.65)
        inject([{"type": "pointer-axis", "axis": "vertical", "delta": 120.0}])
        pause(.8)
        move(803, 311)
        pause(.4)
        click(803, 311)  # End of the Friday decision paragraph.
        paste(" The approval is due Friday.")
        pause(.4)
        move(224, 127)
        pause(.35)
        click(224, 127)  # QindaWrite's Paste action.
        pause(.9)
    app_scene("office-write", ["qindawrite", "--new-instance", str(SAMPLE / "signal-garden-brief.docx")], write)

    def calc() -> None:
        move(452, 357)
        pause(.5)
        click(452, 357)  # B5: lighting quantity.
        pause(.8)
        paste("2")
        move(239, 126)
        pause(.45)
        click(239, 126)  # QindaCalc's real Paste command commits the cell.
        pause(1.4)
        move(555, 677)  # Project balance, visibly recalculated.
        pause(.45)
        click(555, 677)
        pause(.5)
    app_scene("office-calc", ["qindacalc", "--new-instance", str(SAMPLE / "signal-garden-budget.xlsx")], calc)

    def chart() -> None:
        move(185, 830)
        pause(.5)
        click(185, 830)  # Attendance tab.
        pause(1.0)
        inject([{"type": "pointer-axis", "axis": "vertical", "delta": 160.0}])
        pause(.8)
    app_scene("office-chart", ["qindacalc", "--new-instance", str(SAMPLE / "signal-garden-scenarios.xlsx")], chart)

    def show() -> None:
        for y in (394, 510, 630):
            move(230, y)
            pause(.35)
            click(230, y)
            pause(.9)
        move(1178, 317)  # Speaker notes for the decision slide.
        pause(.4)
    app_scene("office-show", ["qindashow", "--new-instance", str(SAMPLE / "signal-garden-pitch.fodp")], show)

    def note() -> None:
        move(449, 395)
        pause(.5)
        click(449, 395)  # Complete the low-glare marker check.
        pause(1.1)
        move(241, 298)
        pause(.4)
        click(241, 298)  # Launch checklist page.
        pause(1.1)
    app_scene("office-note", ["qindanote", "--new-instance", str(SAMPLE / "signal-garden-notes")], note)


if __name__ == "__main__":
    main()
