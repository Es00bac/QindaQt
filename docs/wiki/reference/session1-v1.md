# Session1 version 1

Session1 is the narrow session-supervisor logout boundary. It is owned by
`qindaqt-session` on the ordinary user session bus and exists only for the
currently supervised QindaQt shell.

## Fixed surface

| Field | Value |
| --- | --- |
| Bus name | `org.qindaqt.Session1` |
| Object path | `/org/qindaqt/Session1` |
| Interface | `org.qindaqt.Session1` |
| `CanLogout` | no input, one D-Bus `b` output |
| `Logout` | no input or output |

The installed source descriptor is
`src/session_supervisor/org.qindaqt.Session1.xml`. Changing a name, path,
method, or signature requires a compatibility decision and normally a new
interface version.

## Authentication and failure

Both methods resolve the caller's session-bus credential PID for every call
and compare it with the supervisor's live shell PID. No unique name or PID is
cached across the supervisor's one allowed shell replacement. An unrelated,
retired, or non-D-Bus caller receives
`org.qindaqt.Session1.Error.Unauthorized`. `Logout` additionally returns
`org.qindaqt.Session1.Error.Unavailable` after shutdown has begun or either
essential child is absent. Callers must treat bus loss as an unknown outcome
and must not replay automatically.

`CanLogout` is an admission check, not a reservation. An authenticated caller
must still handle `Logout` becoming unavailable. The session-actions client
rechecks it immediately before dispatch and bounds every pending call.

## Accepted logout order

The supervisor sends the empty success reply before it begins teardown. It
then stops the shell, notification host, and optional network secret agent in
that order, clears their authority, and exits successfully. Because KWin starts
the supervisor through `--exit-with-session`, that exit ends the compositor.
The agent is non-essential and never affects `CanLogout`.

## Scope

Session1 does not expose lock, suspend, reboot, power-off, enumeration,
arbitrary process control, or a general shell capability token. Lock uses the
standard session-bus ScreenSaver interface; machine power actions use login1
behind the separate injected
[`session_actions`](../architecture/module-boundaries.md) client. Power1 stays
unchanged.
