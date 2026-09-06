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
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

from gabbee_probe_support import (  # noqa: E402
    PhaseResult,
    ResultDocument,
    SyntheticRecorder,
    readback_proves_insertion,
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
        if not bus.name_has_owner(COMPOSITOR_SERVICE):
            return {"error": f"{COMPOSITOR_SERVICE} not registered on session bus"}
        endpoint = bus.get_object(COMPOSITOR_SERVICE, COMPOSITOR_PATH)
        interface = dbus.Interface(endpoint, "org.qindaqt.Compositor1")
        reply = interface.DockWindows(
            target_id, incoming_id, "horizontal", "second", dbus.Double(0.5),
            timeout=10,
        )
        if isinstance(reply, (bytes, bytearray)):
            reply = reply.decode("utf-8", "replace")
        return json.loads(str(reply)) if reply else {}

    # -- phases ----------------------------------------------------------

    def preflight(self) -> PhaseResult:
        atspi = self._atspi_backend()
        facts: dict[str, object] = {
            "transcript": TRANSCRIPT,
            "typingToolsFoundAndRemoved": _strip_typing_tools(),
            "waylandDisplay": bool(os.environ.get("WAYLAND_DISPLAY")),
            "qtAccessibilityAlwaysOn": os.environ.get("QT_LINUX_ACCESSIBILITY_ALWAYS_ON") == "1",
            "sessionBusPrivatePath": os.environ.get("DBUS_SESSION_BUS_ADDRESS", "").startswith(
                "unix:path=" + os.environ.get("XDG_RUNTIME_DIR", "")
            ),
        }
        # Verify qdbus6 and org.kde.KWin registration independently before
        # attempting the bridge, so a None bridge can be disambiguated.
        qdbus = shutil.which("qdbus6") or shutil.which("qdbus")
        facts["qdbusAvailable"] = bool(qdbus)
        if qdbus:
            try:
                probe = subprocess.run(
                    [qdbus, "org.kde.KWin", "/Scripting",
                     "org.kde.kwin.Scripting.isScriptLoaded", "probe"],
                    capture_output=True, text=True, timeout=3,
                )
                facts["kwinServiceReachable"] = probe.returncode == 0
                facts["kwinServiceOutput"] = probe.stdout.strip() or probe.stderr.strip()
            except Exception as exc:
                facts["kwinServiceReachable"] = False
                facts["kwinServiceOutput"] = str(exc)
        else:
            facts["kwinServiceReachable"] = False
        backend = self._backend()
        facts["kwinScriptingBridge"] = backend.bridge is not None
        facts["atSpiAvailable"] = atspi.available
        facts["atSpiBrokerReachable"] = self._atspi_broker_reachable()
        facts["wlCopyAvailable"] = bool(shutil.which("wl-copy"))
        ok = bool(facts["kwinScriptingBridge"] and facts["waylandDisplay"])
        bridge_status = "alive" if facts["kwinScriptingBridge"] else "unavailable"
        return PhaseResult(
            "preflight",
            ok,
            f"KWin-scripting bridge {bridge_status}; typing tools stripped from PATH",
            facts,
        )

    def _atspi_broker_reachable(self) -> bool:
        result = subprocess.run(
            ["gdbus", "introspect", "--session", "--dest", "org.a11y.Bus",
             "--object-path", "/org/a11y/bus"],
            capture_output=True, text=True, timeout=3,
        )
        return result.returncode == 0

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
        found = sum([self.editor_context is not None, self.terminal_context is not None])
        summary = (
            "Gabbee enumerated both target windows through KWin scripting"
            if ok
            else f"window-inventory incomplete: found {found}/2 target windows"
        )
        return PhaseResult(
            "window-inventory",
            ok,
            summary,
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
        inserted_via_atspi = readback_proves_insertion(
            str(focused["surrounding"] or ""), surrounding_after, TRANSCRIPT
        )
        # AT-SPI surrounding text readback is the only acceptable proof of insertion
        # on Wayland — clipboard delivery is a fallback path that cannot confirm the
        # text reached the focused widget.  Report clipboard evidence separately but
        # never let it make the phase green.
        last_delivered = delivery.get("lastText", "")
        clipboard_fallback = bool(last_delivered) and clipboard == last_delivered
        delivery_method = str(delivery.get("deliveryMethod") or "")
        atspi_delivery = delivery_method == "at-spi"
        ok = focused["windowId"] == target.window_id and atspi_delivery and inserted_via_atspi
        if ok:
            summary = f"synthetic dictation captured focused {label} member and delivered via Gabbee"
        elif clipboard_fallback:
            summary = (
                f"dictation-{label}: focus captured, clipboard fallback only "
                f"(AT-SPI surrounding readback empty — Wayland insertion unconfirmed)"
            )
        else:
            summary = f"dictation-{label}: delivery incomplete or focus mismatch"
        return PhaseResult(
            f"dictation-{label}",
            ok,
            summary,
            {
                "capturedFocus": focused,
                "delivery": delivery,
                "clipboardAfter": clipboard,
                "insertedViaAtSpi": inserted_via_atspi,
                "atSpiDelivery": atspi_delivery,
                "clipboardFallback": clipboard_fallback,
            },
        )

    def grouped_member(self) -> PhaseResult:
        dock = self._dock_windows(
            self.editor_context.window_id, self.terminal_context.window_id
        )
        docked = dock.get("status") == "docked"
        if not docked:
            reason = dock.get("error", "compositor returned non-docked status")
            return PhaseResult(
                "grouped-member-focus",
                False,
                f"compositor DockWindows unavailable: {reason}",
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
        try:
            phases = [self.preflight(), self.windows()]
            for phase in phases:
                self.document.add(phase)
            if not all(phase.ok for phase in phases):
                return 1
            self.document.add(self.dictation_into(self.editor_context, "editor"))
            self.document.add(self.dictation_into(self.terminal_context, "terminal"))
            self.document.add(self.grouped_member())
            return 0 if self.document.outcome() else 1
        finally:
            self._finish()

    def _finish(self) -> None:
        self.document.write(Path(self.arguments.result_path))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--result-path", required=True)
    parser.add_argument("--run-id", default="nested")
    parser.add_argument("--editor-desktop-id", default="org.qindaqt.TextEditor")
    parser.add_argument("--terminal-desktop-id", default="org.qindaqt.Terminal")
    arguments = parser.parse_args()
    # KWinQtScriptBridge.try_create() returns None when QCoreApplication.instance()
    # is None — it uses QDBusConnection.sessionBus() and a nested QEventLoop to
    # receive KWin script callbacks.  Create the app before the Probe runs so the
    # bridge is reachable.  QCoreApplication (not QApplication) is correct here:
    # no display or GUI platform is needed for D-Bus + scripting.
    try:
        from PyQt6.QtCore import QCoreApplication
        _qt_app = QCoreApplication.instance() or QCoreApplication(sys.argv)
    except Exception as exc:
        print(f"warning: QCoreApplication unavailable: {exc}", file=sys.stderr)
        _qt_app = None
    return Probe(arguments).run()


if __name__ == "__main__":
    raise SystemExit(main())
