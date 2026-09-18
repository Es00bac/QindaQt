#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Solid-colour GTK4 client used as a real shade-visibility fixture.

Usage: shade_fixture_gtk.py TITLE #RRGGBB EVENT_LOG MODE WIDTH HEIGHT

MODE is `csd` (client-side decorations via a custom header bar, with GTK's own
client-side shadow and opaque region), `ssd` (GTK negotiates server-side
decorations), `borderless`, `maximized` (borderless, used as the backdrop), or
`entry` (borderless with two text entries above the solid area, for the
on-screen keyboard rows: their allocations and every text change are logged).
Every press, activation change, and control request is appended to EVENT_LOG
as one JSON object per line, so the driver observes which client actually
received pointer input.

Signals (the driver's control channel):
- SIGUSR2 mints an xdg-activation token from this client's latest input serial
  and writes it to $SHADE_TOKEN_FILE (only a focused client gets a valid one).
- SIGUSR1 presents the window with that token when present -- the same path a
  real application follows when another application hands it activation.
- SIGHUP maps a transient dialog painted #d09020.
"""

from __future__ import annotations

import json
import os
import signal
import sys
import time
from pathlib import Path

import gi

gi.require_version("Gtk", "4.0")
from gi.repository import Gio, GLib, Gtk  # noqa: E402

DIALOG_COLOUR = (0xD0 / 255, 0x90 / 255, 0x20 / 255)


def solid_area(rgb: tuple[float, float, float]) -> Gtk.DrawingArea:
    area = Gtk.DrawingArea()
    area.set_hexpand(True)
    area.set_vexpand(True)

    def draw(_area, context, _width, _height):
        context.set_source_rgb(*rgb)
        context.paint()

    area.set_draw_func(draw)
    return area


def entry_column(area: Gtk.DrawingArea, record) -> Gtk.Box:
    """Two text entries over the solid area; the driver taps them by the
    allocations they log and reads back what the keyboard typed."""
    column = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
    column.set_margin_top(8)
    column.set_margin_start(8)
    column.set_margin_end(8)
    for name in ("entry-a", "entry-b"):
        entry = Gtk.Entry()
        entry.set_name(name)
        entry.set_placeholder_text(name)
        entry.set_size_request(-1, 40)
        entry.connect("changed", lambda e, n=name: record("text", entry=n, value=e.get_text()))
        entry.connect("notify::has-focus", lambda e, _p, n=name: record(
            "entry-focus", entry=n, value=e.has_focus()))

        def log_allocation(e: Gtk.Entry, n: str = name) -> bool:
            ok, bounds = e.compute_bounds(e.get_root())
            if ok:
                record("entry-allocation", entry=n, x=bounds.origin.x, y=bounds.origin.y,
                       width=bounds.size.width, height=bounds.size.height)
            return GLib.SOURCE_REMOVE

        entry.connect("map", lambda e, n=name: GLib.timeout_add(
            300, lambda: log_allocation(e, n)))
        column.append(entry)
    area.set_vexpand(True)
    column.append(area)
    return column


def main() -> int:
    title, colour, log_path, mode, width, height = sys.argv[1:7]
    rgb = tuple(int(colour[index : index + 2], 16) / 255 for index in (1, 3, 5))
    log = open(log_path, "a", buffering=1, encoding="utf-8")
    token_path = os.environ.get("SHADE_TOKEN_FILE")

    def record(kind: str, **fields: object) -> None:
        log.write(json.dumps(
            {"event": kind, "title": title, "time": time.monotonic(), **fields}) + "\n")

    Gtk.init()
    window = Gtk.Window(title=title)
    window.set_default_size(int(width), int(height))
    if mode in ("borderless", "maximized"):
        window.set_decorated(False)
    elif mode == "csd":
        header = Gtk.HeaderBar()
        header.set_title_widget(Gtk.Label(label=title))
        window.set_titlebar(header)
    area = solid_area(rgb)  # type: ignore[arg-type]
    gesture = Gtk.GestureClick()
    gesture.set_button(0)
    gesture.connect("pressed", lambda g, _n, x, y: record(
        "press", x=x, y=y, button=g.get_current_button()))
    area.add_controller(gesture)
    if mode == "entry":
        window.set_decorated(False)
        window.set_child(entry_column(area, record))
    else:
        window.set_child(area)
    window.connect("notify::is-active", lambda w, _p: record("active", value=w.is_active()))
    if mode == "maximized":
        window.maximize()

    loop = GLib.MainLoop()
    dialogs: list[Gtk.Window] = []

    def close(*_args: object) -> bool:
        loop.quit()
        return False

    def mint_token() -> bool:
        try:
            context = window.get_display().get_app_launch_context()
            info = Gio.AppInfo.create_from_commandline(
                "true", "shade-activation", Gio.AppInfoCreateFlags.NONE)
            token = context.get_startup_notify_id(info, [])
        except Exception as error:  # noqa: BLE001 - reported to the driver
            record("token", ok=False, error=str(error))
            return GLib.SOURCE_CONTINUE
        record("token", ok=bool(token))
        if token and token_path:
            Path(token_path).write_text(token, encoding="utf-8")
        return GLib.SOURCE_CONTINUE

    def present() -> bool:
        token = None
        if token_path and Path(token_path).exists():
            token = Path(token_path).read_text(encoding="utf-8").strip() or None
        record("present-request", token=bool(token))
        if token:
            window.set_startup_id(token)
        window.present()
        return GLib.SOURCE_CONTINUE

    def open_dialog() -> bool:
        dialog = Gtk.Window(title=f"{title} dialog")
        dialog.set_transient_for(window)
        dialog.set_default_size(300, 200)
        dialog.set_child(solid_area(DIALOG_COLOUR))
        dialog.present()
        dialogs.append(dialog)
        record("dialog-opened")
        return GLib.SOURCE_CONTINUE

    window.connect("close-request", close)
    GLib.unix_signal_add(GLib.PRIORITY_DEFAULT, signal.SIGUSR1, present)
    GLib.unix_signal_add(GLib.PRIORITY_DEFAULT, signal.SIGUSR2, mint_token)
    GLib.unix_signal_add(GLib.PRIORITY_DEFAULT, signal.SIGHUP, open_dialog)
    GLib.unix_signal_add(GLib.PRIORITY_DEFAULT, signal.SIGTERM,
                         lambda: close() or GLib.SOURCE_REMOVE)
    window.present()
    record("mapped")
    loop.run()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
