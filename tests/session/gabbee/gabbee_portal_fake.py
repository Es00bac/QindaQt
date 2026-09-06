# SPDX-License-Identifier: GPL-3.0-or-later
"""Fake xdg-desktop-portal backend standing in for the KDE GlobalShortcuts backend.

The probe's staged ``portals.conf`` routes
``org.freedesktop.impl.portal.GlobalShortcuts`` to the reviewed ``kde``
selector, exactly like the independently accepted portal candidate 8215a8cd.
Inside the probe's private session bus no real KDE backend exists, so this
service claims ``org.freedesktop.impl.portal.desktop.kde`` (the bus name the
installed ``kde.portal`` descriptor declares) and implements the GlobalShortcuts
implementation interface exactly as specified by the installed
org.freedesktop.impl.portal.GlobalShortcuts.xml D-Bus interface description.
The real ``xdg-desktop-portal`` frontend then performs genuine frontend-side
registration, request/response handling, and Activated/Deactivated signal
forwarding against it.

AGENT-CONTRACT: This process is test scaffolding on a private bus only.  It
must never claim the backend name on a user's real session bus; the launcher
always spawns or D-Bus-activates it on an explicitly private
DBUS_SESSION_BUS_ADDRESS whose only service directory is probe-owned.
"""

from __future__ import annotations

import argparse
import json
import time
from pathlib import Path

import dbus
import dbus.service
from dbus.mainloop.glib import DBusGMainLoop
from gi.repository import GLib

from gabbee_probe_support import FAKE_BACKEND_BUS_NAME, FAKE_CONTROL_BUS_NAME

IMPL_PATH = "/org/freedesktop/portal/desktop"
IMPL_INTERFACE = "org.freedesktop.impl.portal.GlobalShortcuts"
IMPL_SESSION_INTERFACE = "org.freedesktop.impl.portal.Session"
CONTROL_PATH = "/org/qindaqt/test/gabbee_portal_fake"
CONTROL_INTERFACE = FAKE_CONTROL_BUS_NAME


class FakeSession(dbus.service.Object):
    """impl Session object at the exact handle the frontend allocated."""

    closed = False

    @dbus.service.method(IMPL_SESSION_INTERFACE, in_signature="", out_signature="")
    def Close(self) -> None:  # noqa: N802 (D-Bus casing)
        self.closed = True


class FakeGlobalShortcutsBackend(dbus.service.Object):
    def __init__(self, bus: dbus.Bus) -> None:
        super().__init__(bus, IMPL_PATH)
        self.journal: list[dict[str, object]] = []
        self.session_handles: set[str] = set()
        self.session_shortcuts: dict[str, list[str]] = {}
        self.emitted: list[dict[str, object]] = []

    def _record(self, event: str, **values: object) -> None:
        self.journal.append({"event": event, "timestamp": time.time(), **values})

    @dbus.service.method(
        IMPL_INTERFACE,
        in_signature="oosa{sv}",
        out_signature="ua{sv}",
    )
    def CreateSession(self, handle, session_handle, app_id, options):  # noqa: N802
        session = str(session_handle)
        self.session_handles.add(session)
        self._record(
            "CreateSession",
            handle=str(handle),
            session=session,
            appId=str(app_id),
        )
        # AGENT-NOTE: the frontend owns session-handle allocation; the spec
        # defines no CreateSession result keys for this interface.
        return dbus.UInt32(0), {}

    @dbus.service.method(
        IMPL_INTERFACE,
        in_signature="ooa(sa{sv})sa{sv}",
        out_signature="ua{sv}",
    )
    def BindShortcuts(self, handle, session_handle, shortcuts, parent_window, options):  # noqa: N802
        echoed = []
        ids: list[str] = []
        for shortcut_id, shortcut_options in shortcuts:
            identifier = str(shortcut_id)
            ids.append(identifier)
            merged = dict(shortcut_options)
            merged.setdefault("description", dbus.String("synthetic probe shortcut"))
            merged.setdefault("preferred_trigger", dbus.String("F5"))
            echoed.append((identifier, merged))
        self.session_shortcuts[str(session_handle)] = ids
        self._record(
            "BindShortcuts",
            handle=str(handle),
            session=str(session_handle),
            shortcutIds=ids,
            parentWindow=str(parent_window),
        )
        results = {
            "shortcuts": dbus.Array(echoed, signature="(sa{sv})", variant_level=1)
        }
        return dbus.UInt32(0), results

    @dbus.service.method(
        IMPL_INTERFACE,
        in_signature="ooa{sv}",
        out_signature="ua{sv}",
    )
    def ListShortcuts(self, handle, session_handle, options):  # noqa: N802
        self._record("ListShortcuts", handle=str(handle), session=str(session_handle))
        results = {
            "shortcuts": dbus.Array([], signature="(sa{sv})", variant_level=1)
        }
        return dbus.UInt32(0), results

    # AGENT-NOTE: the frontend drops Activated/Deactivated unless the session
    # handle is an object path ('o'); the installed interface expectation is
    # (osta{sv}) even though the XML annotation only names the argument.
    @dbus.service.signal(IMPL_INTERFACE, signature="osta{sv}")
    def Activated(self, session_handle, shortcut_id, timestamp, options):  # noqa: N802
        self.emitted.append(
            {
                "signal": "Activated",
                "session": str(session_handle),
                "shortcutId": str(shortcut_id),
            }
        )

    @dbus.service.signal(IMPL_INTERFACE, signature="osta{sv}")
    def Deactivated(self, session_handle, shortcut_id, timestamp, options):  # noqa: N802
        self.emitted.append(
            {
                "signal": "Deactivated",
                "session": str(session_handle),
                "shortcutId": str(shortcut_id),
            }
        )


class FakeControl(dbus.service.Object):
    """Probe-side control surface for driving and observing the fake backend."""

    def __init__(self, bus: dbus.Bus, backend: FakeGlobalShortcutsBackend) -> None:
        super().__init__(bus, CONTROL_PATH)
        self.backend = backend
        self._sessions: dict[str, FakeSession] = {}

    def register_session(self, session_handle: str, session: FakeSession) -> None:
        self._sessions[session_handle] = session

    @dbus.service.method(CONTROL_INTERFACE, in_signature="", out_signature="s")
    def GetJournal(self) -> str:  # noqa: N802
        return json.dumps(
            {
                "journal": self.backend.journal,
                "sessions": sorted(self.backend.session_handles),
                "sessionShortcuts": self.backend.session_shortcuts,
                "emitted": self.backend.emitted,
                "closedSessions": sorted(
                    handle for handle, session in self._sessions.items() if session.closed
                ),
            }
        )

    @dbus.service.method(CONTROL_INTERFACE, in_signature="s", out_signature="b")
    def EmitActivated(self, shortcut_id) -> bool:  # noqa: N802
        session = self._single_session()
        if session is None:
            return False
        self.backend.Activated(
            dbus.ObjectPath(session),
            str(shortcut_id),
            dbus.UInt64(int(time.time() * 1000)),
            {},
        )
        return True

    @dbus.service.method(CONTROL_INTERFACE, in_signature="s", out_signature="b")
    def EmitDeactivated(self, shortcut_id) -> bool:  # noqa: N802
        session = self._single_session()
        if session is None:
            return False
        self.backend.Deactivated(
            dbus.ObjectPath(session),
            str(shortcut_id),
            dbus.UInt64(int(time.time() * 1000)),
            {},
        )
        return True

    def _single_session(self) -> str | None:
        # AGENT-NOTE: exactly one Gabbee client registers per probe run; if
        # that invariant ever breaks, refuse to guess which session to signal.
        handles = sorted(self.backend.session_handles)
        return handles[0] if len(handles) == 1 else None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args()
    loop = DBusGMainLoop(set_as_default=True)
    bus = dbus.SessionBus()
    try:
        existing = bus.get_name_owner(FAKE_BACKEND_BUS_NAME)
    except dbus.DBusException:
        existing = None
    if existing:
        raise SystemExit(f"{FAKE_BACKEND_BUS_NAME} is already owned on this bus")
    backend = FakeGlobalShortcutsBackend(bus)
    control = FakeControl(bus, backend)

    def watch_name(owner: object) -> None:
        # The frontend allocates session objects itself; expose impl Session
        # Close for every session the backend saw, lazily via GetJournal is
        # too late, so hook CreateSession through a small idle poll.
        for handle in list(backend.session_handles):
            if handle not in control._sessions:
                control.register_session(handle, FakeSession(bus, handle))

    GLib.timeout_add(50, watch_name, True)
    if not bus.request_name(FAKE_BACKEND_BUS_NAME):
        raise SystemExit(f"could not acquire {FAKE_BACKEND_BUS_NAME}")
    if not bus.request_name(CONTROL_INTERFACE):
        raise SystemExit(f"could not acquire {CONTROL_INTERFACE}")
    GLib.MainLoop().run()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
