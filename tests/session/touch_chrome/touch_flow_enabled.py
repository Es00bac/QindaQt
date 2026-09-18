# SPDX-License-Identifier: GPL-3.0-or-later
"""The Touchscreen switch is real (ADR-0205 §3): `input.touch.enabled=false`
stops every touch device at KWin's seat, and `true` brings only those back.

Judged through the development input device, the private session's only
touch device (the touch rows claim it with QINDAQT_DEVELOPMENT_INPUT_TOUCH=1):
Compositor1 `Capabilities` reports it available only while it is enabled, and
`InjectTestInput` refuses a finger while the preference is off. The preference
travels the production path: a Settings1 user transaction on the private bus,
observed by the plugin's own purpose-scoped client.
"""

from __future__ import annotations

import time
from typing import Any

import dbus

from shade_control import wait_for
from shade_session import ShadeSession

SETTINGS_SERVICE = "org.qindaqt.Settings1"
SETTINGS_PATH = "/org/qindaqt/Settings1"
ENABLED_KEY = "input.touch.enabled"


def _settings() -> dbus.Interface:
    return dbus.Interface(dbus.SessionBus().get_object(SETTINGS_SERVICE, SETTINGS_PATH),
                          SETTINGS_SERVICE)


def commit_touch_enabled(enabled: bool) -> dict[str, Any]:
    """One user-value write of input.touch.enabled through Settings1."""
    settings = _settings()
    snapshot = settings.GetSnapshot(dbus.Array([ENABLED_KEY], signature="s"))
    if int(snapshot["status"]) != 0:
        raise RuntimeError(f"GetSnapshot refused: {dict(snapshot)}")
    operation = dbus.Dictionary({"key": ENABLED_KEY, "kind": "set", "value": dbus.Boolean(enabled)},
                                signature="sv")
    reply = settings.CommitUserTransaction(str(snapshot["epoch"]), dbus.UInt64(int(snapshot["revision"])),
                                           dbus.Array([operation], signature="a{sv}"))
    if int(reply["status"]) != 0:
        raise RuntimeError(f"CommitUserTransaction refused: {dict(reply)}")
    return {"revisionAfter": int(reply["revisionAfter"]),
            "value": bool(reply["values"][ENABLED_KEY])}


def touch_available(session: ShadeSession) -> bool:
    capabilities = session.control.capabilities()
    development = capabilities.get("developmentInput") or {}
    return bool(development.get("available")) and bool(development.get("enabled", True))


def injection_accepted(session: ShadeSession) -> bool:
    try:
        session.control.inject([{"type": "touch-down", "id": 7, "x": 30.0, "y": 30.0},
                                {"type": "touch-up", "id": 7}])
        return True
    except RuntimeError:
        return False


def run(session: ShadeSession) -> None:
    session.step("touch-enabled-initial", available=touch_available(session))
    session.verdict("touchDeviceAvailableByDefault", touch_available(session))
    session.verdict("fingerAcceptedByDefault", injection_accepted(session))

    off = commit_touch_enabled(False)
    session.step("settings-commit-off", **off)
    wait_for("touch device stopped", lambda: not touch_available(session), 8)
    session.verdict("touchDeviceStoppedByPreference", not touch_available(session))
    session.verdict("fingerRefusedWhileOff", not injection_accepted(session))
    time.sleep(0.5)
    session.verdict("touchStaysOff", not touch_available(session))

    on = commit_touch_enabled(True)
    session.step("settings-commit-on", **on)
    wait_for("touch device restored", lambda: touch_available(session), 8)
    session.verdict("touchDeviceRestoredByPreference", touch_available(session))
    session.verdict("fingerAcceptedAgain", injection_accepted(session))
