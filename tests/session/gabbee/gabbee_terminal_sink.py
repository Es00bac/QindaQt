# SPDX-License-Identifier: GPL-3.0-or-later
"""Adapter around Gabbee's real ``AgentInputTextSink`` for the Terminal proof.

The proof must show that Gabbee's own production text sink — not a copy of
it, and not this repository's hand-rolled helper wire protocol — puts the
sentinel into the Terminal's PTY. This module imports the real class from
the mounted Gabbee checkout and adapts two of its properties that the proof
has to work with rather than around:

- ``AgentInputTextSink.deliver()`` owns the whole helper lifecycle. It spawns
  ``qindaqt-agent-input`` itself and blocks inside ``_ensure_started()``
  until the helper publishes ``READY``, which the helper only does after the
  RemoteDesktop portal dialog is approved. Nothing else in the proof gets a
  chance to click that dialog unless the approval runs concurrently, so
  :func:`deliver_with_concurrent_approval` drives the click from the caller's
  thread while ``deliver()`` blocks on a worker thread.
- ``AgentInputTextSink.deliver_key()`` deliberately reports failure: the sink
  is a text sink and offers no key-chord route. The Return and Ctrl-D strokes
  that frame the shell command therefore cannot come from it, and the proof
  keeps its separately approved direct helper session for exactly those
  strokes. The sentinel itself travels only through the sink.

AGENT-CONTRACT: the Gabbee checkout is a read-only input. Nothing here
writes into it, subclasses it, or reimplements it; the only supported
coupling is importing the real class and honouring its DeliveryResult.
"""

from __future__ import annotations

import os
import shutil
import threading
from typing import Any, Callable

# The Gabbee sink resolves its own helper through this variable before
# falling back to PATH, so it is also the seam the proof uses to pin the
# exact installed helper and its device set.
SINK_COMMAND_ENVIRONMENT = "GABBEE_AGENT_INPUT_COMMAND"
DEFAULT_SINK_COMMAND = "/usr/bin/qindaqt-agent-input --devices pointer,keyboard"


class TerminalSinkError(RuntimeError):
    """Raised when the real Gabbee sink cannot be imported or driven."""


def import_real_sink() -> type:
    """Return Gabbee's real ``AgentInputTextSink`` class.

    AGENT-GUARD: this import must resolve to the mounted Gabbee checkout
    (``/opt/gabbee/src`` on PYTHONPATH inside the sandbox). If it ever
    resolves to a local stand-in the proof stops being a Gabbee integration
    proof, so the failure is raised rather than substituted.
    """

    try:
        from gabbee.agent_input import AgentInputTextSink
    except Exception as error:  # pragma: no cover - depends on the mounted checkout
        raise TerminalSinkError(
            f"Gabbee's real AgentInputTextSink is not importable: {error}"
        ) from error
    return AgentInputTextSink


def sink_command(explicit: str | None = None) -> str:
    """Resolve the helper command the sink should run.

    Mirrors the sink's own precedence (explicit, then environment, then the
    installed helper on PATH) so the proof pins the same binary the sink
    would have chosen on its own.
    """

    for candidate in (explicit, os.environ.get(SINK_COMMAND_ENVIRONMENT, "").strip()):
        if candidate:
            return candidate
    resolved = shutil.which("qindaqt-agent-input")
    return resolved if resolved else DEFAULT_SINK_COMMAND


def build_sink(command: str | None = None, *, timeout: float = 60.0) -> Any:
    """Construct the real sink bound to the exact installed helper."""

    return import_real_sink()(sink_command(command), timeout=timeout)


def delivery_result_evidence(result: Any) -> dict[str, Any]:
    """Project Gabbee's ``DeliveryResult`` into the proof's evidence document.

    ``uncertain`` is carried through deliberately: the sink sets it when text
    may already be partially delivered, and a reader of this evidence must be
    able to tell that case apart from a clean failure.
    """

    return {
        "ok": bool(getattr(result, "ok", False)),
        "method": str(getattr(result, "method", "")),
        "detail": str(getattr(result, "detail", "")),
        "uncertain": bool(getattr(result, "uncertain", False)),
        "targetVerified": bool(getattr(result, "target_verified", False)),
    }


def deliver_with_concurrent_approval(
    sink: Any, text: str, approve: Callable[[], Any], *, timeout: float = 90.0
) -> tuple[Any | None, str | None, Any]:
    """Run ``sink.deliver(text)`` while the portal dialog is approved.

    Returns ``(result, error, approval)``. ``result`` is Gabbee's own
    DeliveryResult when the call completed, ``error`` a message when the sink
    raised or never returned, and ``approval`` whatever ``approve`` returned
    (the proof uses it as the pointer origin).

    AGENT-GUARD: the approval must run while ``deliver()`` is blocked, not
    before it. The helper is spawned inside ``deliver()``, so approving first
    would find no dialog, and approving after would deadlock against the
    sink's own READY wait.
    """

    box: dict[str, Any] = {}

    def _deliver() -> None:
        try:
            box["result"] = sink.deliver(text)
        except Exception as error:  # pragma: no cover - defensive
            box["error"] = f"sink.deliver raised: {error}"

    worker = threading.Thread(target=_deliver, name="gabbee-sink-deliver", daemon=True)
    worker.start()
    approval = approve()
    worker.join(timeout)
    if worker.is_alive():
        return None, f"sink.deliver did not return within {timeout:.0f}s", approval
    if "error" in box:
        return None, str(box["error"]), approval
    return box.get("result"), None, approval
