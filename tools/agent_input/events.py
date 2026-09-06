# SPDX-License-Identifier: GPL-3.0-or-later
"""JSON event parser for the agent-input RemoteDesktop session.

Each input event is one JSON object on a single line. Unknown keys are ignored.

AGENT-NOTE: move_abs (absolute pointer motion) is intentionally absent.
The portal's NotifyPointerMotionAbsolute requires a PipeWire screencast stream
node ID; this tool opens no ScreenCast stream (ADR-0087). Use move (relative
motion) instead.
"""
from __future__ import annotations

import json
from typing import Any


class EventError(ValueError):
    pass


def parse_event(line: str) -> dict[str, Any]:
    """Parse one newline-terminated JSON event line.

    Raises EventError on malformed input. Returns an empty dict for blank lines.
    """
    line = line.strip()
    if not line:
        return {}
    try:
        obj = json.loads(line)
    except json.JSONDecodeError as exc:
        raise EventError(f"invalid JSON: {exc}") from exc
    if not isinstance(obj, dict):
        raise EventError("event must be a JSON object")
    action = obj.get("action")
    if not isinstance(action, str):
        raise EventError("event missing string 'action'")
    return obj


def dispatch_event(event: dict[str, Any], session) -> bool:
    """Apply one parsed event to session. Returns False on 'close'.

    Raises EventError for rejected inputs (e.g. unknown button name).

    AGENT-CONTRACT: session must be an ApprovedInputSession that has fired
    on_ready before this function is called.
    """
    action = event.get("action", "")
    if action == "move":
        session.notify_pointer_motion(float(event.get("dx", 0)),
                                      float(event.get("dy", 0)))
    elif action in ("press", "release"):
        try:
            session.notify_pointer_button(event.get("button", ""),
                                          action == "press")
        except ValueError as exc:
            raise EventError(str(exc)) from exc
    elif action == "scroll":
        session.notify_pointer_axis(float(event.get("dx", 0)),
                                    float(event.get("dy", 0)))
    elif action in ("key_press", "key_release"):
        session.notify_key_keysym(int(event.get("keysym", 0)),
                                  action == "key_press")
    elif action == "text":
        session.notify_text(str(event.get("text", "")))
    elif action == "close":
        return False
    # Unknown actions are silently ignored; callers should not depend on
    # undocumented behavior of future actions.
    return True
