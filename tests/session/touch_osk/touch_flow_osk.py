# SPDX-License-Identifier: GPL-3.0-or-later
"""The first-party on-screen keyboard under fingers (ADR-0204).

A GTK client with two text entries is mapped. A finger taps the first entry:
KWin, whose last input was that finger, shows the keyboard it launched from
the private session's kwinrc. Fingers on the keyboard's keys type into the
entry (a letter, a shifted letter, backspace); an injected hardware key
hides the keyboard and still reaches the entry; a finger on the second entry
brings the keyboard back. Judged by KWin's VirtualKeyboard interface, the
keyboard's own evidence file, the entry's text events and captures.
"""

from __future__ import annotations

import json
import os
import time
from pathlib import Path
from typing import Any

import dbus

from shade_control import key, wait_for
from shade_session import ShadeSession, window_summary
from touch_flow_chrome import FingerDriver, _attempt

ENTRY_TITLE = "osk-entry-fixture"
ENTRY_COLOUR = "#2e6fb0"
HARDWARE_KEY = "c"


def _kwin_keyboard_visible() -> bool:
    proxy = dbus.SessionBus().get_object("org.kde.KWin", "/VirtualKeyboard")
    properties = dbus.Interface(proxy, "org.freedesktop.DBus.Properties")
    return bool(properties.Get("org.kde.kwin.VirtualKeyboard", "visible"))


def _osk_evidence() -> dict[str, Any]:
    path = Path(os.environ.get("QINDAQT_OSK_EVIDENCE_FILE", ""))
    if not path.is_file():
        return {}
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return {}


def _entry_events(session: ShadeSession, kind: str, entry: str | None = None) -> list[dict[str, Any]]:
    return [event for event in session.client_events()
            if event.get("title") == ENTRY_TITLE and event.get("event") == kind
            and (entry is None or event.get("entry") == entry)]


def _entry_text(session: ShadeSession, entry: str) -> str:
    events = _entry_events(session, "text", entry)
    return str(events[-1]["value"]) if events else ""


def _entry_point(session: ShadeSession, entry: str) -> tuple[float, float]:
    geometry = session.control.windows()[ENTRY_TITLE]["geometry"]
    allocation = _entry_events(session, "entry-allocation", entry)[-1]
    return (geometry["x"] + allocation["x"] + allocation["width"] / 2,
            geometry["y"] + allocation["y"] + allocation["height"] / 2)


def _key_point(session: ShadeSession, kind: str, label: str | None = None) -> tuple[float, float]:
    """A placed key's centre on screen: the compositor parks the panel along
    the bottom of the output, centred horizontally."""
    evidence = _osk_evidence()
    width, height = evidence["panelSize"]
    logical_width, logical_height = session.config.logical_size
    origin = ((logical_width - width) / 2, logical_height - height)
    for placed in evidence["keys"]:
        if placed["kind"] == kind and (label is None or placed["label"] == label):
            return (origin[0] + placed["x"] + placed["width"] / 2,
                    origin[1] + placed["y"] + placed["height"] / 2)
    raise RuntimeError(f"no {kind} key {label!r} on the keyboard")


def _wait_visible(expected: bool, timeout: float = 10.0) -> None:
    wait_for(f"keyboard visible={expected}", lambda: _kwin_keyboard_visible() == expected, timeout)


def _wait_text(session: ShadeSession, entry: str, expected: str, timeout: float = 6.0) -> None:
    wait_for(f"{entry} text {expected!r}", lambda: _entry_text(session, entry) == expected, timeout)


def _map_entry_client(session: ShadeSession) -> None:
    session.fixtures.gtk(ENTRY_TITLE, ENTRY_COLOUR, "entry", 640, 420)
    wait_for("entry client", lambda: ENTRY_TITLE in session.control.windows(), 15)
    wait_for("entry allocations", lambda: len(_entry_events(session, "entry-allocation")) >= 2, 10)
    time.sleep(0.8)
    session.step("entry-mapped", window=window_summary(session.control.windows()[ENTRY_TITLE]),
                 kwinKeyboardVisible=_kwin_keyboard_visible())
    session.capture("01-entry-mapped")


def _show_by_touch(session: ShadeSession, fingers: FingerDriver, entry: str, verdict: str, capture: str) -> None:
    point = _entry_point(session, entry)
    fingers.tap(point)
    _wait_visible(True)
    session.step(f"tapped-{entry}", point=list(point))
    evidence = _osk_evidence()
    session.verdict(verdict, True)
    if verdict == "oskShowsOnTouchFocus":
        session.verdict("keyboardReportsActivated",
                        evidence.get("activated") is True and evidence.get("visible") is True)
    time.sleep(0.6)
    session.capture(capture)


def _type_with_fingers(session: ShadeSession, fingers: FingerDriver) -> None:
    fingers.tap(_key_point(session, "text", "q"))
    _wait_text(session, "entry-a", "q")
    session.verdict("oskKeyCommitsText", True)
    fingers.tap(_key_point(session, "shift"))
    time.sleep(0.3)
    fingers.tap(_key_point(session, "text", "Q"))
    _wait_text(session, "entry-a", "qQ")
    session.verdict("oskShiftTypesUpperCase", True)
    fingers.tap(_key_point(session, "backspace"))
    _wait_text(session, "entry-a", "q")
    session.verdict("oskBackspaceDeletes", True)
    session.step("typed", presses=_osk_evidence().get("presses"))
    session.capture("03-typed")


def _hardware_key_hides(session: ShadeSession) -> None:
    session.control.inject([key(HARDWARE_KEY, True), key(HARDWARE_KEY, False)])
    _wait_visible(False)
    session.verdict("hardwareKeyHidesOsk", True)
    _wait_text(session, "entry-a", "q" + HARDWARE_KEY)
    session.verdict("hardwareKeyReachesEntry", True)
    time.sleep(0.5)
    session.capture("04-osk-hidden")


def run(session: ShadeSession) -> None:
    fingers = FingerDriver(session)
    _map_entry_client(session)
    _attempt(session, "show-by-touch", ("oskShowsOnTouchFocus", "keyboardReportsActivated"),
             lambda: _show_by_touch(session, fingers, "entry-a", "oskShowsOnTouchFocus", "02-osk-shown"))
    _attempt(session, "type-with-fingers",
             ("oskKeyCommitsText", "oskShiftTypesUpperCase", "oskBackspaceDeletes"),
             lambda: _type_with_fingers(session, fingers))
    _attempt(session, "hardware-key", ("hardwareKeyHidesOsk", "hardwareKeyReachesEntry"),
             lambda: _hardware_key_hides(session))
    _attempt(session, "return-on-next-entry", ("oskReturnsOnNextTouchedEntry",),
             lambda: _show_by_touch(session, fingers, "entry-b", "oskReturnsOnNextTouchedEntry", "05-osk-returns"))
    session.step("final", kwinKeyboardVisible=_kwin_keyboard_visible(), osk=_osk_evidence())
