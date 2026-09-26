#!/usr/bin/env python3
"""Drive only the scenario-gated QindaQt virtual session over its private bus."""

from __future__ import annotations

import json
import sys

import dbus


SERVICE = "org.qindaqt.Compositor"
OBJECT = "/org/qindaqt/Compositor"
INTERFACE = "org.qindaqt.Compositor1"


def inject(events: list[dict]) -> dict:
    proxy = dbus.SessionBus().get_object(SERVICE, OBJECT)
    request = json.dumps({"schemaVersion": 1, "events": events}, separators=(",", ":"))
    result = proxy.InjectTestInput(dbus.ByteArray(request.encode()), dbus_interface=INTERFACE)
    reply = json.loads(bytes(result))
    if reply.get("status") != "injected":
        raise RuntimeError(reply)
    return reply


def main() -> None:
    operation = sys.argv[1]
    if operation == "move":
        x, y = map(float, sys.argv[2:4])
        events = [{"type": "pointer-absolute", "x": x, "y": y}]
    elif operation == "click":
        x, y = map(float, sys.argv[2:4])
        events = [{"type": "pointer-absolute", "x": x, "y": y},
                  {"type": "button", "button": "left", "pressed": True},
                  {"type": "button", "button": "left", "pressed": False}]
    elif operation == "key":
        key = sys.argv[2]
        events = [{"type": "key", "key": key, "pressed": True},
                  {"type": "key", "key": key, "pressed": False}]
    elif operation == "paste":
        events = [{"type": "key", "key": "left-control", "pressed": True},
                  {"type": "key", "key": "v", "pressed": True},
                  {"type": "key", "key": "v", "pressed": False},
                  {"type": "key", "key": "left-control", "pressed": False}]
    elif operation == "events":
        events = json.loads(sys.argv[2])
    else:
        raise SystemExit(f"Unknown operation: {operation}")
    print(json.dumps(inject(events)))


if __name__ == "__main__":
    main()
