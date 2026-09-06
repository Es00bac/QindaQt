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
   portal using `Notify*` calls.
4. Closes the session when stdin closes or a `{"action":"close"}` event arrives.

The tool lives in `tools/agent_input/` (package) and `tools/agent-input` (CLI).
It owns no authorization service, no session persistence, and no screen-capture
or clipboard authority. `ConnectToEIS` is not implemented; `Notify*` is
sufficient for agent and dictation workloads and avoids the additional EIS
socket plumbing.

The portal selector (`qindaqt-portals.conf`) and QindaQt's `.portal` metadata
are unchanged; this ADR adds no backend interface to QindaQt's advertised set.

### Backend readiness proof

The test `tests/tools/test_agent_input.py::AgentInputBackendReadinessTest`
reads the installed `kde.portal` metadata at test time and fails loudly if
`org.freedesktop.impl.portal.RemoteDesktop` is ever removed, ensuring the
`qindaqt-portals.conf` entry is re-reviewed before that happens.

### Fake-portal lifecycle proof

`tests/tools/run_agent_input_portal_proof.py` runs inside `dbus-run-session`
with a fake `org.freedesktop.portal.Desktop` service. It proves the full
`CreateSession → SelectDevices → Start` handshake and every `Notify*` method
dispatch without connecting to the real session bus or triggering a live
approval dialog.

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
