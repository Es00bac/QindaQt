# SPDX-License-Identifier: GPL-3.0-or-later
"""RemoteDesktop portal session: create, approve, and deliver input events.

AGENT-CONTRACT: Call start(), wait for on_ready, then call notify_* methods.
close() is always safe to call. No input is delivered before on_ready fires;
the _approved flag is the exclusive gate.
"""
from __future__ import annotations

import secrets
from typing import Callable

import dbus
import dbus.mainloop.glib
from gi.repository import GLib

PORTAL_SERVICE = "org.freedesktop.portal.Desktop"
PORTAL_PATH = "/org/freedesktop/portal/desktop"
_RD_IFACE = "org.freedesktop.portal.RemoteDesktop"
_REQ_IFACE = "org.freedesktop.portal.Request"
_SESS_IFACE = "org.freedesktop.portal.Session"

# Linux input button codes (include/uapi/linux/input-event-codes.h)
_BUTTON: dict[str, int] = {
    "left": 272, "right": 273, "middle": 274, "side": 275, "extra": 276,
}

DEVICE_KEYBOARD: int = 1
DEVICE_POINTER: int = 2


class ApprovedInputSession:
    """One user-approved RemoteDesktop portal session.

    AGENT-GUARD: Three invariants must hold:
    1. _approved becomes True only after a zero Start response whose granted
       devices satisfy the requested set. No Notify* call reaches the portal
       before that point; _call() enforces this.
    2. _send_close() is idempotent: it clears _session before the D-Bus call
       so a second invocation is a no-op.
    3. _fail() always calls _send_close() before any callback so the portal
       session is never left open after an error.
    """

    def __init__(
        self,
        bus: dbus.SessionBus,
        loop: GLib.MainLoop,
        devices: int = DEVICE_KEYBOARD | DEVICE_POINTER,
        on_ready: Callable[[], None] | None = None,
        on_closed: Callable[[], None] | None = None,
        on_error: Callable[[str], None] | None = None,
    ) -> None:
        self._bus = bus
        self._loop = loop
        self._devices = devices
        self._on_ready = on_ready
        self._on_closed = on_closed
        self._on_error = on_error
        self._session: str | None = None
        self._portal_obj = None
        self._approved = False
        self._create_done = False
        self._select_done = False
        self._start_done = False

    def start(self) -> None:
        self._portal_obj = self._bus.get_object(PORTAL_SERVICE, PORTAL_PATH)
        GLib.idle_add(self._do_create)

    def close(self) -> None:
        """User-initiated close. Safe to call before or after on_ready."""
        self._send_close()
        if self._on_closed:
            self._on_closed()

    # --- notify methods: only valid after on_ready ---

    def notify_pointer_motion(self, dx: float, dy: float) -> None:
        self._call("NotifyPointerMotion",
                   dbus.ObjectPath(self._session), {}, dx, dy)

    def notify_pointer_button(self, button: str, pressed: bool) -> None:
        """Raise ValueError for unknown button names instead of silently falling back."""
        code = _BUTTON.get(button.lower())
        if code is None:
            raise ValueError(
                f"unknown pointer button {button!r}; valid: {', '.join(_BUTTON)}"
            )
        self._call("NotifyPointerButton",
                   dbus.ObjectPath(self._session), {},
                   dbus.Int32(code), dbus.UInt32(1 if pressed else 0))

    def notify_pointer_axis(self, dx: float, dy: float) -> None:
        self._call("NotifyPointerAxis",
                   dbus.ObjectPath(self._session), {},
                   dbus.Double(dx), dbus.Double(dy))

    def notify_key_keysym(self, keysym: int, pressed: bool) -> None:
        self._call("NotifyKeyboardKeysym",
                   dbus.ObjectPath(self._session), {},
                   dbus.Int32(keysym), dbus.UInt32(1 if pressed else 0))

    def notify_text(self, text: str) -> None:
        """Deliver text as paired keysym press/release events.

        AGENT-NOTE: X11 keysym encoding for Unicode: codepoints 0x20–0xFF map
        directly; U+0100 and above use 0x01000000 | codepoint per the X
        keysym allocation policy. ASCII printable range is a subset of 0x20–0xFF.
        """
        for char in text:
            cp = ord(char)
            keysym = cp if cp <= 0xFF else (0x01000000 | cp)
            self.notify_key_keysym(keysym, True)
            self.notify_key_keysym(keysym, False)

    # --- internal portal handshake ---

    def _token(self, prefix: str) -> str:
        return f"qindaqt_{prefix}_{secrets.token_hex(4)}"

    def _request_path(self, token: str) -> str:
        name = self._bus.get_unique_name()
        safe = name[1:].replace(".", "_") if name.startswith(":") else name
        return f"/org/freedesktop/portal/desktop/request/{safe}/{token}"

    def _subscribe(self, path: str, handler: Callable) -> None:
        self._bus.add_signal_receiver(
            handler, signal_name="Response",
            dbus_interface=_REQ_IFACE, bus_name=PORTAL_SERVICE, path=path,
        )

    def _do_create(self) -> bool:
        token = self._token("create")
        path = self._request_path(token)
        self._subscribe(path, self._on_create)
        try:
            iface = dbus.Interface(self._portal_obj, _RD_IFACE)
            returned = iface.CreateSession({
                "handle_token": token,
                "session_handle_token": self._token("sess"),
            })
            if str(returned) != path:
                self._subscribe(str(returned), self._on_create)
        except dbus.DBusException as exc:
            self._fail(f"CreateSession: {exc}")
        return False

    def _on_create(self, response: int, results: dict) -> None:
        if self._create_done:
            return
        self._create_done = True
        if response != 0:
            self._fail("CreateSession not approved")
            return
        handle = results.get("session_handle")
        if not handle:
            self._fail("CreateSession: missing session_handle")
            return
        self._session = str(handle)
        # Subscribe to external session revocation before any further calls.
        self._bus.add_signal_receiver(
            self._on_session_closed_signal,
            signal_name="Closed",
            dbus_interface=_SESS_IFACE,
            path=self._session,
        )
        GLib.idle_add(self._do_select)

    def _do_select(self) -> bool:
        token = self._token("sel")
        path = self._request_path(token)
        self._subscribe(path, self._on_select)
        try:
            iface = dbus.Interface(self._portal_obj, _RD_IFACE)
            returned = iface.SelectDevices(
                dbus.ObjectPath(self._session),
                {"handle_token": token, "types": dbus.UInt32(self._devices)},
            )
            if str(returned) != path:
                self._subscribe(str(returned), self._on_select)
        except dbus.DBusException as exc:
            self._fail(f"SelectDevices: {exc}")
        return False

    def _on_select(self, response: int, results: dict) -> None:
        if self._select_done:
            return
        self._select_done = True
        if response != 0:
            self._fail("SelectDevices not approved")
            return
        GLib.idle_add(self._do_start)

    def _do_start(self) -> bool:
        token = self._token("start")
        path = self._request_path(token)
        self._subscribe(path, self._on_start)
        try:
            iface = dbus.Interface(self._portal_obj, _RD_IFACE)
            returned = iface.Start(
                dbus.ObjectPath(self._session),
                "",  # no parent window
                {"handle_token": token},
            )
            if str(returned) != path:
                self._subscribe(str(returned), self._on_start)
        except dbus.DBusException as exc:
            self._fail(f"Start: {exc}")
        return False

    def _on_start(self, response: int, results: dict) -> None:
        if self._start_done:
            return
        self._start_done = True
        if response != 0:
            self._fail("Start not approved by user")
            return
        granted = int(results.get("devices", 0))
        if (granted & self._devices) != self._devices:
            self._fail(
                f"Start: requested devices {self._devices:#x} not granted "
                f"(got {granted:#x})"
            )
            return
        # AGENT-GUARD: Set _approved only here. _call() checks this flag;
        # nothing writes it before a zero Start response with full device grant.
        self._approved = True
        if self._on_ready:
            self._on_ready()

    def _on_session_closed_signal(self, details: dict) -> None:
        """Portal revoked the session externally (e.g. user revoked in KDE settings)."""
        self._session = None
        if self._on_closed:
            self._on_closed()
        self._loop.quit()

    def _send_close(self) -> None:
        """Send Close to the portal. Idempotent: clears _session first."""
        session = self._session
        self._session = None
        self._approved = False
        if session:
            try:
                obj = self._bus.get_object(PORTAL_SERVICE, session)
                dbus.Interface(obj, _SESS_IFACE).Close()
            except dbus.DBusException:
                pass

    def _call(self, method: str, *args) -> None:
        if not self._approved:
            return
        try:
            iface = dbus.Interface(self._portal_obj, _RD_IFACE)
            getattr(iface, method)(*args)
        except dbus.DBusException as exc:
            self._fail(f"{method}: {exc}")

    def _fail(self, msg: str) -> None:
        self._send_close()
        if self._on_error:
            self._on_error(msg)
        self._loop.quit()
