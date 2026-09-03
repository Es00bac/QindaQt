# ADR-0057: Authenticate shell window actions by the panel Wayland owner

- **Status:** Accepted
- **Date:** 2026-09-02
- **Owners:** Compositor integration and shell clients
- **Supersedes:** None
- **Superseded by:** None

## Context

The production task list and launcher must activate, minimize, restore, close,
and raise ordinary windows. The existing `org.qindaqt.Compositor1` object is on
the ordinary user session bus and intentionally has no caller authentication.
Production therefore keeps every mutator on that interface disabled. Enabling
those methods for the shell would also enable them for every local process and
would reverse the read-only decision documented by the
[Compositor1 reference](../reference/compositor-control-v1.md).

The compositor already receives an unforgeable kernel PID for each connected
Wayland client and knows which client owns every layer-shell surface. The
production panel adapter gives each QindaQt panel the fixed `dock` scope. The
session bus daemon independently exposes the kernel PID behind a caller's
unique bus name through `GetConnectionCredentials`. Joining those two
credentials gives the compositor a narrow authority without putting a token in
arguments, the environment, or persistent storage.

Any same-user local process can call the session bus. A process that has only a
bus connection must not control windows. The design does not defend against a
same-user process that can both connect to the QindaQt Wayland socket and
successfully impersonate a committed `dock` layer client, nor against a
compromised production shell. Session-bus and Wayland-socket access control,
process isolation, and code integrity remain defense-in-depth outside this
boundary.

## Decision

Publish a second object on the existing `org.qindaqt.Compositor` bus name:
`/org/qindaqt/CompositorShell`, interface
`org.qindaqt.CompositorShell1`. It is a separately authenticated shell-only
mutation boundary, not an extension of `Compositor1`.

The compositor binds shell authority only while at least one committed
layer-shell surface with exact scope `dock` exists and every such live surface
belongs to the same positive Wayland client PID. No surface, an uncommitted
surface, conflicting PIDs during replacement, or loss of the owning Wayland
client makes the authority unbound. Each method obtains the caller's unique
bus name from the current D-Bus message, asks the bus daemon for that
connection's credentials, and admits only an exact PID match. Credential
lookup failure is denial. A well-known bus name, executable name, UID alone,
or caller-supplied PID is never authority.

`ActivateWindow`, `MinimizeWindow`, `UnminimizeWindow`, `CloseWindow`, and
`RaiseWindow` take one KWin window UUID plus the epoch and decimal-string
revision of the coherent `ShellVisibilitySnapshot` generation the shell used.
Authentication precedes generation and window lookup; a stale generation is
rejected before window lookup. The compositor revalidates the live window and
its current Hybrid ownership immediately before dispatch. Independent windows
use KWin's ordinary public actions. A Hybrid member routes through the existing
page, group-minimize, close-confirmation, and group-stacking policy; the shell
boundary never directly changes one member in isolation. Close always requests
the client's normal close path and never kills a process.

Replies use the closed statuses `admitted`, `stale`, `unknown-window`,
`unauthorized`, and `control-disabled`, echo the action/window/generation, and
carry a stable failure code when not admitted. Admission is rate bounded per
authenticated unique bus owner. The server never queues or replays an action,
and the asynchronous shell client permits one request in flight, binds it to
the exact compositor unique owner and observed generation, and treats timeout,
owner replacement, or transport loss as uncertain without retry.

## Consequences

- Production `Compositor1` remains read-only and retains its existing public
  threat model, descriptor, and pre-parse mutation rejection.
- A shell crash or replacement revokes authority as its panel surfaces vanish;
  an overlap with two panel-owner PIDs fails closed until only one remains.
- The task-list and launcher lanes consume one public asynchronous client and
  must supply the exact generation associated with the displayed intent.
- Unit tests require fake credentials, owner, registry, clock, and executor;
  private-bus tests require exact-owner replacement and no-replay coverage;
  nested KWin evidence must prove a bound panel owner is admitted and a second
  local caller is rejected.
- PID equality is a live cross-transport credential join, not a durable shell
  identity or a general capability for applets and applications.

## Revisit when

Replace this join with a descriptor-provisioned capability if QindaQt permits
untrusted clients to create layer-shell `dock` roles on the production socket,
splits panels and shell action handling across processes, or moves shell window
actions to a standardized compositor protocol with equivalent authenticated
ownership and generation fencing.
