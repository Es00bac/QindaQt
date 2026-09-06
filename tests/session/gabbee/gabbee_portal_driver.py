# SPDX-License-Identifier: GPL-3.0-or-later
"""Drive Gabbee's real portal global-shortcut client against a provided session bus.

Runs Gabbee's production ``PortalPushToTalkBinding`` (PyQt6 + dbus-python +
GLib, exactly as the bar uses it), waits for real registration through the
``xdg-desktop-portal`` frontend, then asks the fake KDE backend control
interface to emit Activated/Deactivated and verifies Gabbee's pressed and
released callbacks fire.  Prints one final JSON line with the evidence.

Intended interpreter: the Gabbee venv Python with the host site-packages on
PYTHONPATH (see gabbee_probe_support.gabbee_probe_python_environment).
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path
from typing import Callable

sys.path.insert(0, str(Path(__file__).resolve().parent))

from gabbee_probe_support import FAKE_CONTROL_BUS_NAME  # noqa: E402

CONTROL_PATH = "/org/qindaqt/test/gabbee_portal_fake"

REGISTRATION_TIMEOUT_SECONDS = 30.0
SIGNAL_TIMEOUT_SECONDS = 10.0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--shortcut-ids", nargs="+", default=["push_to_talk", "command"])
    parser.add_argument("--shortcut-texts", nargs="+", default=["F5", "F6"])
    arguments = parser.parse_args()

    import dbus
    from PyQt6.QtCore import QCoreApplication

    from gabbee.ui.global_shortcuts import PortalPushToTalkBinding, PortalShortcutSpec

    app = QCoreApplication.instance() or QCoreApplication([])  # noqa: F841 (kept alive for cross-thread signals)
    statuses: dict[str, list[tuple[bool, str]]] = {}
    events: list[tuple[str, str]] = []

    def pump(poll: Callable[[], bool], timeout: float) -> bool:
        """Run the Qt loop so cross-thread pyqtSignals can deliver."""

        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            app.processEvents()
            if poll():
                return True
            time.sleep(0.05)
        app.processEvents()
        return poll()

    specs = []
    for shortcut_id, shortcut_text in zip(
        arguments.shortcut_ids, arguments.shortcut_texts, strict=True
    ):
        specs.append(
            PortalShortcutSpec(
                shortcut_id=shortcut_id,
                shortcut_text=shortcut_text,
                description=f"Gabbee probe {shortcut_id}",
                on_pressed=lambda ident=shortcut_id: events.append(("pressed", ident)),
                on_released=lambda ident=shortcut_id: events.append(("released", ident)),
                on_status_change=lambda registered, message, ident=shortcut_id: statuses.setdefault(
                    ident, []
                ).append((bool(registered), str(message))),
            )
        )

    binding = PortalPushToTalkBinding(shortcut_specs=specs)
    binding.start()

    def all_registered() -> bool:
        return (
            len(statuses) == len(specs)
            and statuses
            and all(any(registered for registered, _ in entries) for entries in statuses.values())
        )

    pump(all_registered, REGISTRATION_TIMEOUT_SECONDS)

    registered_ids = sorted(
        ident for ident, entries in statuses.items() if any(r for r, _ in entries)
    )
    registration_complete = len(registered_ids) == len(specs)

    signal_results: dict[str, bool] = {}
    if registration_complete:
        bus = dbus.SessionBus()
        control = bus.get_object(FAKE_CONTROL_BUS_NAME, CONTROL_PATH)
        probe_id = arguments.shortcut_ids[0]
        control.EmitActivated(probe_id)
        pump(lambda: ("pressed", probe_id) in events, SIGNAL_TIMEOUT_SECONDS)
        control.EmitDeactivated(probe_id)
        pump(lambda: ("released", probe_id) in events, SIGNAL_TIMEOUT_SECONDS)
        signal_results = {
            "activatedRoutedPressed": ("pressed", probe_id) in events,
            "deactivatedRoutedReleased": ("released", probe_id) in events,
        }

    binding.close()
    document = {
        "registered": registration_complete,
        "registeredIds": registered_ids,
        "statuses": {
            ident: [
                {"registered": registered, "message": message}
                for registered, message in entries
            ]
            for ident, entries in statuses.items()
        },
        "events": [{"event": event, "shortcutId": ident} for event, ident in events],
        **signal_results,
    }
    print(json.dumps(document), flush=True)
    return 0 if registration_complete and all(signal_results.values()) else 1


if __name__ == "__main__":
    raise SystemExit(main())
