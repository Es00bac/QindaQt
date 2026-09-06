# SPDX-License-Identifier: GPL-3.0-or-later
"""RemoteDesktop portal session: create, approve, and deliver input events.

AGENT-CONTRACT: Call start(), wait for on_ready, then call notify_* methods.
close() is always safe to call. No input is delivered before on_ready fires.
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
_BUTTON = {"left": 272, "right": 273, "middle": 274, "side": 275, "extra": 276}

DEVICE_KEYBOARD: int = 1
DEVICE_POINTER: int = 2


class ApprovedInputSession:
    """One user-approved RemoteDesktop portal session.

    AGENT-GUARD: The three-step handshake (CreateSession → SelectDevices →
    Start) must complete before any Notify* method is called. Skipping Start
    or ignoring a non-zero response code delivers input without user consent,
    violating the portal contract.
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
        self._create_done = False
        self._select_done = False
        self._start_done = False

    def start(self) -> None:
        self._portal_obj = self._bus.get_object(PORTAL_SERVICE, PORTAL_PATH)
        GLib.idle_add(self._do_create)

    def close(self) -> None:
        if self._session:
            try:
                obj = self._bus.get_object(PORTAL_SERVICE, self._session)
                dbus.Interface(obj, _SESS_IFACE).Close()
            except dbus.DBusException:
                pass
            finally:
                self._session = None
        if self._on_closed:
            self._on_closed()

    # --- notify methods: only valid after on_ready ---

    def notify_pointer_motion(self, dx: float, dy: float) -> None:
        self._call("NotifyPointerMotion",
                   dbus.ObjectPath(self._session), {}, dx, dy)

    def notify_pointer_motion_absolute(
        self, stream: int, x: float, y: float
    ) -> None:
        self._call("NotifyPointerMotionAbsolute",
                   dbus.ObjectPath(self._session), {},
                   dbus.UInt32(stream), x, y)

    def notify_pointer_button(self, button: str, pressed: bool) -> None:
        code = _BUTTON.get(button.lower(), _BUTTON["left"])
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
        if self._on_ready:
            self._on_ready()

    def _call(self, method: str, *args) -> None:
        if not self._session:
            return
        try:
            iface = dbus.Interface(self._portal_obj, _RD_IFACE)
            getattr(iface, method)(*args)
        except dbus.DBusException as exc:
            self._fail(f"{method}: {exc}")

    def _fail(self, msg: str) -> None:
        if self._on_error:
            self._on_error(msg)
        self._loop.quit()
