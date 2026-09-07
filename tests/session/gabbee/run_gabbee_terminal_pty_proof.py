# SPDX-License-Identifier: GPL-3.0-or-later
"""Lane-gated proof that Gabbee's real text sink reaches the Terminal's PTY.

Entry point only. The work lives in four collaborators, split by the
boundary each one owns:

- ``gabbee_terminal_sentinel`` — the pure sentinel/readback byte contract.
- ``gabbee_terminal_lane`` — the outer bubblewrap lane driver (host side).
- ``gabbee_terminal_boot`` — the inner private session boot and teardown.
- ``gabbee_terminal_delivery`` — the real portal sessions, the Gabbee sink
  delivery, and the byte-exact readback; it uses ``gabbee_terminal_sink``
  to drive Gabbee's own ``AgentInputTextSink``.

OUTER (run by root with the private-runtime lane allocated): validates the
exact ``QINDAQT_PRIVATE_RUNTIME_LANE=interactive-virtual-desktop``
acknowledgement, takes the cross-worktree private-session lock, stages the
Terminal component, and launches the sandbox.

INNER (inside the sandbox): boots the private session bus, a private
PipeWire core, the parent Weston compositor, the child QindaQt compositor
windowed on it, and the Terminal and Editor. It then approves a real
``org.freedesktop.portal.RemoteDesktop`` session against the installed
xdg-desktop-portal-kde backend and has Gabbee's real sink type a fresh
Unicode sentinel into the Terminal, whose own shell writes it to a file.
The proof is the byte-exact readback of that file — never AT-SPI, never the
helper's ACK, never the sink's own DeliveryResult. Both the standalone
Terminal and the Terminal grouped with the Editor are proven, each with its
own sentinel and output file.

AGENT-CONTRACT: no microphone, no typing tool, no uinput, no host input
injection, no host D-Bus or Wayland socket. Every process lives inside the
bubblewrap PID/network namespace; the only host-visible effect is the
locked, per-user private-session lane.
"""

from __future__ import annotations

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))


def main() -> int:
    if "--inner" in sys.argv:
        from gabbee_terminal_boot import run_inner

        return run_inner()
    from gabbee_terminal_lane import run_outer

    return run_outer()


if __name__ == "__main__":
    raise SystemExit(main())
