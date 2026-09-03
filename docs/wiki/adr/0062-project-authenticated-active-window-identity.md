# ADR-0062: Project authenticated active-window identity to the shell

- **Status:** Accepted
- **Date:** 2026-09-03
- **Owners:** Compositor integration and Global Menu composition
- **Supersedes:** None
- **Superseded by:** None

## Context

[ADR-0033](0033-canonical-menu-model-and-authenticated-menu-ownership.md)
requires Global Menu to join a registrar entry to independently authenticated
active-window identity. The standard AppMenu registrar is deliberately not that
authority: any session-bus peer can initially register a numeric window id it
does not own. Deriving the active PID or the compositor-window-to-registrar-id
mapping from registrar contents would make the proof circular.

KWin already owns the required live relationships. For a native Wayland window
it obtains the client PID from the Wayland connection credentials. For an
XWayland window it resolves the X client PID with XRes
`LOCAL_CLIENT_PID` and owns the exact X11 window id used by
`RegisterWindow`. KWin also retains the paired appmenu service name and object
path announced through the KDE Wayland appmenu protocol or the corresponding
X11 properties. These values are sensitive focus/process facts and cannot be
added to unauthenticated `Compositor1`.

## Decision

Extend the panel-owner-authenticated `org.qindaqt.CompositorShell1` boundary
from [ADR-0061](0061-authenticate-shell-window-actions-by-panel-owner.md) with
one `ActiveWindowIdentity` snapshot and a no-payload
`ActiveWindowIdentityChanged` invalidation. The method authenticates the D-Bus
caller's unique-name PID against the sole committed `dock` layer-surface owner
before consulting KWin. After one successful read, invalidations are targeted
only to that exact bus peer; they are not broadcast session-bus signals.
The endpoint still declares and exports the signal through its Qt meta-object so
live introspection matches the immutable XML. It never emits the Qt signal;
delivery uses a targeted D-Bus message with the declared member name.

The complete schema-1 snapshot has its own monotonic `(epoch, revision)`
lineage. Its epoch is the window-action/visibility service epoch and it carries
the exact `actionRevision` sampled with the identity. A focus move, appmenu
announcement change, or action/visibility-generation change republishes the
snapshot. The shell client uses `revision` as the focus generation required by
ADR-0033 and uses `(epoch, actionRevision)` to correlate it with the compositor
window inventory.

An admitted active window contains its compositor UUID and optional PID,
optional AppMenu numeric id, and optional paired appmenu service/path. The
numeric id is the exact X11 client window id for XWayland and is `null` for
native Wayland, whose KDE appmenu protocol identifies the endpoint directly by
service/path. Missing, invalid, or unauthenticated facts remain typed absence;
the compositor never substitutes `_NET_WM_PID`, invents a Wayland numeric id,
or derives identity from registrar state. A partially announced service/path
pair is omitted. Invalid complete facts make the snapshot unavailable rather
than publishing a partial guess.

The existing exact-owner shell window-actions client performs identity reads
over its existing transport and compositor-owner binding. It accepts one
monotonic identity lineage, withdraws facts immediately on invalidation,
owner replacement, timeout, malformed payload, revision regression, or an
equal-revision content collision, and never creates a second compositor
client. Publication and decoding both apply the public
`ShellWindowGeneration::isValid()` rule to the carried action fence; an invalid
epoch cannot become available client truth.

## Consequences

- Global Menu can match the active UUID to the exact X11 registrar id, or use
  the Wayland-announced service/path, then independently compare the
  registrar/provider bus peer's daemon-derived PID with the compositor PID.
- The compositor proof prevents a registrar claim from becoming menu
  authority merely because its numeric id matches. It does not make the
  registrar itself authenticated, stop a local process from inserting bogus
  registrations, or prove that an announced service name is currently owned by
  the window process. Composition must bind the name to an exact unique owner,
  resolve that peer's credentials, repeat the focus snapshot around credential
  lookup, and fail closed on every mismatch.
- A compromised application process can export a malicious menu for its own
  window; a compromised shell or a process capable of impersonating its
  committed panel remains outside this boundary's threat model. A legitimate
  out-of-process menu exporter with a different PID also fails closed until a
  separately authenticated delegation protocol exists.
- Focused tests require fake identity/credential sources, strict wire decoding,
  exact-owner private-bus refresh and late-old-owner reply rejection, and a
  private virtual-KWin row comparing the live object to its XML, a native
  Wayland PID and valid KDE AppMenu announcement, overlong and malformed
  service plus malformed path withdrawal/recovery, and an XWayland XRes
  PID/window id to real clients while rejecting an unbound caller.

## Revisit when

KWin or KDE standardizes an authenticated compositor-to-shell identity
protocol, native Wayland gains a registrar-compatible numeric identifier, or
QindaQt needs explicit authenticated delegation to an out-of-process menu
exporter.
