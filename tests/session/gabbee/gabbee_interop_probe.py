# SPDX-License-Identifier: GPL-3.0-or-later
"""In-sandbox Gabbee dictation/focus/grouped-member interop probe for QindaQt.

Runs INSIDE the root-reserved private nested runtime (bubblewrap + nested
KWin), with Gabbee's real production stack and no input injection:

- focus capture/activation: Gabbee's KWin-scripting window backend against
  the QindaQt compositor's stock ``org.kde.KWin`` Scripting service;
- insertion: Gabbee's real ``TextDeliveryRouter`` recovery chain.  Direct
  typing tools are absent by sandbox design (no uinput node), so delivery
  exercises the AT-SPI EditableText path and the clipboard mirror — Gabbee's
  production non-injection paths;
- STT: Gabbee's mock provider with the fixed harmless transcript; the
  recorder is the probe's ``SyntheticRecorder`` (fixed silent WAV, never an
  audio device);
- grouped member: the compositor's ``org.qindaqt.Compositor.DockWindows``
  development API groups the editor and terminal windows; Gabbee must capture
  and deliver for exactly the focused member.

AGENT-GUARD: This probe must never create or admit a typing tool.  It strips
``dotool``/``xdotool`` from PATH before importing Gabbee so the router's
direct-typing attempt fails deterministically instead of reaching for uinput.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

from gabbee_probe_support import (  # noqa: E402
    PhaseResult,
    ResultDocument,
    SyntheticRecorder,
    synthetic_transcript,
)

TRANSCRIPT = synthetic_transcript()
FOCUS_TIMEOUT_SECONDS = 20.0
DELIVERY_TIMEOUT_SECONDS = 40.0
COMPOSITOR_SERVICE = "org.qindaqt.Compositor"
COMPOSITOR_PATH = "/org/qindaqt/Compositor"


def _strip_typing_tools() -> list[str]:
    """Remove every typing tool from PATH; return what was found."""

    found = [tool for tool in ("dotool", "xdotool", "ydotool") if shutil.which(tool)]
    kept = []
    for entry in os.environ.get("PATH", "").split(os.pathsep):
        if not entry:
            continue
        if any((Path(entry) / tool).is_file() for tool in ("dotool", "xdotool", "ydotool")):
            continue
        kept.append(entry)
    os.environ["PATH"] = os.pathsep.join(kept)
    return found


class Probe:
    def __init__(self, arguments: argparse.Namespace) -> None:
        self.arguments = arguments
        self.document = ResultDocument(arguments.run_id, "nested-dictation")
        self.editor_context = None
        self.terminal_context = None

    # -- helpers ---------------------------------------------------------

    def _gabbee(self):
        import gabbee  # noqa: F401  (importable means the checkout is on path)

        from gabbee.config import load_config

        return load_config()

    def _backend(self):
        from gabbee.desktop import KWinWindowBackend

        return KWinWindowBackend()

    def _find_window(self, backend, desktop_id: str):
        wanted = desktop_id.casefold().removesuffix(".desktop")
        for window in backend.list_windows():
            if window.desktop_file_id.casefold().removesuffix(".desktop") == wanted:
                return window
        return None

    def _activate_and_capture(self, backend, target, label: str) -> dict[str, object]:
        """Activate via Gabbee's backend and return its captured focus context."""

        import gabbee.desktop as desktop

        if not backend.activate_window(target):
            raise RuntimeError(f"Gabbee could not activate the {label} window")
        service = desktop.AppContextService(backend, self._atspi_backend())
        deadline = time.monotonic() + FOCUS_TIMEOUT_SECONDS
        captured = None
        while time.monotonic() < deadline:
            active = backend.active_window()
            if active is not None and active.window_id == target.window_id:
                captured = service.capture()
                if captured is not None:
                    break
            time.sleep(0.25)
        if captured is None:
            raise RuntimeError(f"Gabbee never captured the focused {label} window")
        return {
            "windowId": captured.window_id,
            "desktopFileId": captured.desktop_file_id,
            "title": captured.title,
            "pid": captured.pid,
            "focusedRole": captured.focused_role,
            "focusedName": captured.focused_name,
            "surrounding": (captured.surrounding_before or "") + "|" + (captured.surrounding_after or ""),
        }

    def _atspi_backend(self):
        from gabbee.desktop import AtSpiAccessibilityBackend

        return AtSpiAccessibilityBackend()

    def _dictate(self, controller) -> dict[str, object]:
        controller.start()
        time.sleep(0.1)
        controller.stop()
        deadline = time.monotonic() + DELIVERY_TIMEOUT_SECONDS
        from gabbee.models import ControllerState

        while time.monotonic() < deadline:
            snapshot = controller.snapshot()
            if snapshot.state in (ControllerState.IDLE, ControllerState.ERROR):
                break
            time.sleep(0.2)
        snapshot = controller.snapshot()
        last = controller.last_dictation
        return {
            "state": str(snapshot.state),
            "lastText": snapshot.last_text,
            "deliveryMethod": snapshot.delivery_method,
            "errorMessage": snapshot.error_message,
            "targetApp": snapshot.target_app,
            "rawTranscript": last.raw_text if last else None,
            "delivered": bool(last.delivered) if last else False,
        }

    def _clipboard_text(self) -> str | None:
        import subprocess

        for tool, argv in (("wl-paste", ["wl-paste", "--no-newline"]),):
            if shutil.which(tool):
                result = subprocess.run(argv, capture_output=True, timeout=10, check=False)
                if result.returncode == 0:
                    return result.stdout.decode("utf-8", "replace")
        return None

    def _dock_windows(self, target_id: str, incoming_id: str) -> dict[str, object]:
        import dbus

        bus = dbus.SessionBus()
        endpoint = bus.get_object(COMPOSITOR_SERVICE, COMPOSITOR_PATH)
        interface = dbus.Interface(endpoint, "org.qindaqt.Compositor1")
        reply = interface.DockWindows(
            target_id, incoming_id, "horizontal", "second", dbus.Double(0.5)
        )
        if isinstance(reply, (bytes, bytearray)):
            reply = reply.decode("utf-8", "replace")
        return json.loads(str(reply)) if reply else {}

    # -- phases ----------------------------------------------------------

    def preflight(self) -> PhaseResult:
        facts: dict[str, object] = {
            "transcript": TRANSCRIPT,
            "typingToolsFoundAndRemoved": _strip_typing_tools(),
            "waylandDisplay": bool(os.environ.get("WAYLAND_DISPLAY")),
        }
        backend = self._backend()
        facts["kwinScriptingBridge"] = backend.bridge is not None
        facts["atSpiAvailable"] = self._atspi_backend().available
        facts["wlCopyAvailable"] = bool(shutil.which("wl-copy"))
        ok = bool(facts["kwinScriptingBridge"] and facts["waylandDisplay"])
        return PhaseResult(
            "preflight",
            ok,
            "Gabbee KWin-scripting bridge available; typing tools stripped from PATH",
            facts,
        )

    def windows(self) -> PhaseResult:
        backend = self._backend()
        deadline = time.monotonic() + FOCUS_TIMEOUT_SECONDS
        windows: list[dict[str, object]] = []
        while True:
            windows = [
                {
                    "windowId": window.window_id,
                    "desktopFileId": window.desktop_file_id,
                    "title": window.title,
                }
                for window in backend.list_windows()
            ]
            self.editor_context = self._find_window(backend, self.arguments.editor_desktop_id)
            self.terminal_context = self._find_window(
                backend, self.arguments.terminal_desktop_id
            )
            if self.editor_context is not None and self.terminal_context is not None:
                break
            if time.monotonic() >= deadline:
                break
            time.sleep(0.5)
        ok = self.editor_context is not None and self.terminal_context is not None
        return PhaseResult(
            "window-inventory",
            ok,
            "Gabbee enumerated both target windows through KWin scripting",
            {"windows": windows},
        )

    def dictation_into(self, target, label: str) -> PhaseResult:
        from gabbee.controller import GabbeeController

        config = self._gabbee()
        controller = GabbeeController(config, recorder=SyntheticRecorder())
        focused = self._activate_and_capture(controller.window_backend, target, label)
        delivery = self._dictate(controller)
        clipboard = self._clipboard_text()
        context_after = controller.context_service.capture() if controller.context_service else None
        surrounding_after = (
            (context_after.surrounding_before or "") + "|" + (context_after.surrounding_after or "")
            if context_after
            else ""
        )
        inserted_via_atspi = TRANSCRIPT in surrounding_after or TRANSCRIPT in str(
            focused["surrounding"] or ""
        )
        clipboard_mirror = clipboard == TRANSCRIPT
        ok = focused["windowId"] == target.window_id and (
            inserted_via_atspi or clipboard_mirror
        )
        return PhaseResult(
            f"dictation-{label}",
            ok,
            f"synthetic dictation captured focused {label} member and delivered via Gabbee",
            {
                "capturedFocus": focused,
                "delivery": delivery,
                "clipboardAfter": clipboard,
                "insertedViaAtSpi": inserted_via_atspi,
                "clipboardMirror": clipboard_mirror,
            },
        )

    def grouped_member(self) -> PhaseResult:
        dock = self._dock_windows(
            self.editor_context.window_id, self.terminal_context.window_id
        )
        docked = dock.get("status") == "docked"
        if not docked:
            return PhaseResult(
                "grouped-member-focus",
                False,
                "compositor DockWindows did not report success",
                {"dockResponse": dock},
            )
        editor_result = self.dictation_into(self.editor_context, "group-member-editor")
        terminal_result = self.dictation_into(self.terminal_context, "group-member-terminal")
        ids = {
            "editor": editor_result.evidence["capturedFocus"]["windowId"],
            "terminal": terminal_result.evidence["capturedFocus"]["windowId"],
        }
        distinct = ids["editor"] == self.editor_context.window_id and ids[
            "terminal"
        ] == self.terminal_context.window_id and ids["editor"] != ids["terminal"]
        ok = distinct and editor_result.ok and terminal_result.ok
        return PhaseResult(
            "grouped-member-focus",
            ok,
            "Gabbee captured and delivered for exactly the focused grouped member on both members",
            {
                "dockResponse": dock,
                "memberFocus": ids,
                "editor": editor_result.evidence,
                "terminal": terminal_result.evidence,
            },
        )

    def run(self) -> int:
        phases = [self.preflight(), self.windows()]
        for phase in phases:
            self.document.add(phase)
        if not all(phase.ok for phase in phases):
            self._finish()
            return 1
        self.document.add(self.dictation_into(self.editor_context, "editor"))
        self.document.add(self.dictation_into(self.terminal_context, "terminal"))
        self.document.add(self.grouped_member())
        self._finish()
        return 0 if self.document.outcome() else 1

    def _finish(self) -> None:
        self.document.write(Path(self.arguments.result_path))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--result-path", required=True)
    parser.add_argument("--run-id", default="nested")
    parser.add_argument("--editor-desktop-id", default="org.qindaqt.TextEditor")
    parser.add_argument("--terminal-desktop-id", default="org.qindaqt.Terminal")
    arguments = parser.parse_args()
    return Probe(arguments).run()


if __name__ == "__main__":
    raise SystemExit(main())
