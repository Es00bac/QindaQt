# ADR-0087: Deliver agent and Gabbee input through the RemoteDesktop portal

- **Status:** Accepted
- **Date:** 2026-09-06
- **Owners:** Platform tools
- **Supersedes:** None
- **Superseded by:** None

## Context

QindaQt agents and the Gabbee dictation utility need to deliver pointer movement,
pointer clicks, scroll events, and keyboard input (including Unicode text) to the
running desktop without root privileges or `uinput` access.

The project already owns `src/compositor/kwin/kwindevelopmentinputinjector.cpp`
(a KWin-internal path, off-limits for production use) and the `InjectTestInput`
protocol (scoped to the test lane). Neither is the right path for user-facing
input delivery.

The installed KDE portal backend (`xdg-desktop-portal-kde`, confirmed on this
host by reading `/usr/share/xdg-desktop-portal/portals/kde.portal`) advertises
`org.freedesktop.impl.portal.RemoteDesktop`. The QindaQt portal selector already
routes `org.freedesktop.impl.portal.RemoteDesktop=kde;gtk;lxqt` (ADR-0059).
For QindaQt sessions, the portal package supplies the backend-only KDE service
environment described in [ADR-0088](0088-enable-kde-remote-desktop-for-qindaqt.md);
the frontend remains QindaQt and the QindaQt backend remains Settings-only.
The standard `org.freedesktop.portal.RemoteDesktop` frontend API provides:

- `CreateSession` / `SelectDevices` / `Start` — one native approval dialog per
  session; the user can close or revoke the session at any time.
- `NotifyPointerMotion`, `NotifyPointerMotionAbsolute`, `NotifyPointerButton`,
  `NotifyPointerAxis` — pointer delivery without raw device access.
- `NotifyKeyboardKeysym`, `NotifyKeyboardKeycode` — keyboard delivery using the
  X11 keysym encoding (Unicode above U+00FF uses the `0x01000000 | codepoint`
  allocation policy).
- `ConnectToEIS` — an EIS-based stream alternative for higher-throughput use.

## Decision

A new small Python CLI utility (`tools/agent-input`) wraps the RemoteDesktop
portal in a minimal session with bounded lifetime:

1. Opens one session with the standard three-step handshake.
2. Prints `READY` on stderr after user approval.
3. Reads newline-delimited JSON events from stdin and dispatches them to the
   portal using `Notify*` calls. An event with `requestId` receives one matching
   stdout acknowledgement only after that portal call returns.
4. Closes the session when stdin closes or a `{"action":"close"}` event arrives.

The tool lives in `tools/agent_input/` (package) and `tools/agent-input` (CLI).
It owns no authorization service, no session persistence, and no screen-capture
or clipboard authority. `ConnectToEIS` is not implemented; `Notify*` is
sufficient for agent and dictation workloads and avoids the additional EIS
socket plumbing.

The portal selector (`qindaqt-portals.conf`) and QindaQt's `.portal` metadata
are unchanged; this ADR adds no backend interface to QindaQt's advertised set.

### Installed command and dependencies

The install component `AgentInput` provides `qindaqt-agent-input` in the
normal `${CMAKE_INSTALL_BINDIR}` alongside its private `agent_input` Python
package. The package is installed with the command so the helper does not
fall back to a checkout or a symlink into the source tree. A normal terminal
therefore runs:

```text
qindaqt-agent-input --devices pointer,keyboard
```

The runtime requires Python 3.10 or newer, `dbus-python`, and PyGObject
(`gi.repository.GLib`). It also requires a session D-Bus and a portal backend
that implements `org.freedesktop.portal.RemoteDesktop`; the helper does not
start either service. The package's installed smoke test runs `--help` with
an empty `PYTHONPATH`, proving the command imports its installed package and
bindings without opening a bus or requesting approval. The lifecycle tests
continue to use only the private fake portal.

### Backend readiness proof

The test `tests/tools/test_agent_input.py::AgentInputBackendReadinessTest`
reads the installed `kde.portal` metadata at test time and fails loudly if
`org.freedesktop.impl.portal.RemoteDesktop` is ever removed, ensuring the
`qindaqt-portals.conf` entry is re-reviewed before that happens.

### Fake-portal lifecycle proof

`tests/tools/run_agent_input_portal_proof.py` runs inside `dbus-run-session`
with a fake `org.freedesktop.portal.Desktop` service, without connecting to
the real session bus or triggering a live approval dialog. Its modes are:

- `basic` — full `CreateSession → SelectDevices → Start` handshake, every
  `Notify*` method dispatched with exact arguments, stdin EOF after the last
  event retained.
- `denial` — denied `Start`: the tool sends `Session.Close`, exits with
  status 1, and delivers no `Notify*`.
- `no_events_before_start` — events written before the `Start` response never
  reach `Notify*`; the stdin source is attached only after approval.
- `notify_failure` — a `Notify*` D-Bus error closes the session and exits 1.
- `revoke` — an external `Session::Closed` signal ends the tool cleanly even
  with stdin still open.
- acknowledgement — one successful and one failed `requestId` event prove that
  stdout reports portal acceptance or rejection, never a stdin-write claim.

The exact acknowledgement contract is [ADR-0093](0093-acknowledge-agent-input-portal-acceptance.md).

### Session lifecycle contract

Input delivery is gated on explicit approval only: the internal approved flag
is set exclusively by a zero `Start` response whose granted device set covers
the request, and every `Notify*` call checks it. Every terminal failure and
external `Session::Closed` revocation clears the flag and closes the portal
session; denial and transport errors exit nonzero. Absolute pointer motion is
not offered because `NotifyPointerMotionAbsolute` requires a PipeWire
screencast stream node this tool deliberately does not open; use relative
`move` events instead.

## Consequences

- Agents and Gabbee can deliver pointer and keyboard input under QindaQt with
  exactly one native approval prompt per session, using no root privileges,
  `uinput`, or KWin-internal APIs.
- The session is user-revocable: closing the portal session object or
  terminating `agent-input` stops all input delivery immediately.
- Input delivery requires an active KDE portal backend (`xdg-desktop-portal-kde`
  running); the tool fails with a descriptive error if the backend is absent.
- `ConnectToEIS` remains out of scope; a future ADR can adopt it if EIS
  throughput becomes necessary.
- This tool is not a QindaQt shell component; it does not appear in the portal
  service, shell_orchestration, or any compositor module.

## Revisit when

Revisit if agents require higher-throughput EIS delivery, if the RemoteDesktop
portal interface changes, or if QindaQt implements a native input-delivery
authority that supersedes the portal path.
