# ADR-0254: separate saved window preferences from session apply truth

- **Status:** Proposed
- **Date:** 2026-09-23
- **Owners:** Settings, Session, Compositor
- **Supersedes:** None
- **Superseded by:** None

## Context

Settings1 persistence and a live compositor effect are different facts. The
Windows & workspaces route receives a confirmed Settings1 snapshot after its
commit, while `qindaqt-session` separately writes the matching `kwinrc` values
and asks KWin to reload. A successful Settings1 commit cannot show that the
file was written or that KWin completed its reload. The existing bridge also
discarded the asynchronous reconfigure result.

The route must remain a Settings1 consumer. The session remains the only
`kwinrc` writer, and the compositor continues to apply its `[QindaQt]` values
on KWin's configuration-change signal. The status path extends the live
bridge described by [ADR-0209](0209-bridge-window-management-settings-into-kwinrc.md)
and the [compositor/session architecture](../architecture/compositor-session.md).

## Decision

1. Settings1-confirmed `windowManagement.*` values remain the saved preference
   source of truth. The Windows route presents that saved state separately
   from its draft and from the session apply status.
2. `qindaqt-session` reports `Applied` only after it has written the requested
   values, read them back from its owned `kwinrc` groups, received a successful
   `org.kde.KWin.reconfigure` reply from the exact compositor owner, and
   confirmed the same values still read back. Failed writes, mismatched
   readback, KWin call errors, stale Settings1 lineage, and owner loss do not
   produce `Applied`.
3. The session exports a same-user, session-bus `org.qindaqt.WindowManagement1`
   interface at `/org/qindaqt/WindowManagement1`. Version 1 provides
   `GetState() -> a{sv}`, `StateChanged(a{sv})`, and `RetryApply() -> ()`. The
   fixed state dictionary is:

   | Field | D-Bus value | Meaning |
   | --- | --- | --- |
   | `wireVersion` | `u`, exactly 1 | Fixed dictionary version |
   | `phase` | `s` | `unavailable`, `applying`, `applied`, or `failed` |
   | `settingsOwner` | `s` | Exact Settings1 unique owner, empty before a snapshot |
   | `settingsEpoch` | `s` | Settings1 snapshot epoch |
   | `settingsRevision` | `t` | Settings1 snapshot revision |
   | `preferences` | `a{sv}` | Complete five-key `windowManagement.*` snapshot when known; empty otherwise |
   | `kwinOwner` | `s` | Exact KWin unique owner only after acknowledged apply |
   | `message` | `s` | Bounded (512-character) diagnostic or empty |

   A caller may retry the current confirmed session apply; that operation
   does not change or recommit Settings1 values.
4. The Windows route watches the unique owner of this interface, reads state
   from that same owner, and treats service loss, owner replacement, and
   malformed state as unavailable. Every unique-owner transition synchronously
   revokes the previously published status before requesting the replacement's
   state; generation fencing discards late replies from the former owner. It
   says `Applied in this session` only when the reported applied preference
   equals the route's current Settings1-confirmed values. A failed matching
   apply exposes its diagnostic and an explicit retry action. KWin owner loss
   is reported until the replacement compositor acknowledges the reapply.
5. The initial confirmed session baseline and each replacement KWin owner
   receive one acknowledged reconfigure even when `kwinrc` already matches.
   After convergence, ordinary updates retain the existing debounced
   write-then-reconfigure behavior.

The interface is read-only except for the idempotent `RetryApply` operation.
All same-session clients share the user's existing Settings1 authority; no
credential or privileged operation crosses this boundary.

## Consequences

- A persisted preference can remain visible and editable even while the
  session writer or KWin is unavailable, without claiming that the running
  desktop has applied it.
- The session must keep its apply state coherent across Settings1 owner/epoch
  changes and compositor owner replacement. The route must continue observing
  and decoding the versioned fixed dictionary fail-closed.
- `Applied` means a matching `kwinrc` readback followed by a successful KWin
  reconfigure reply. Nested KWin interaction tests remain necessary to prove
  that the configuration changes the tested pointer and focus behavior.
- The new interface is additive to Settings1 and Session1. Incompatible wire
  changes require a new protocol version or endpoint.

## Revisit when

Revisit this boundary if another consumer needs the same apply-state contract,
if KWin exposes a supported readback for native focus and snap values, or if
the session writer moves out of `qindaqt-session`.
