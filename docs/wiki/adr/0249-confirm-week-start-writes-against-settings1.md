# ADR-0249: Confirm week-start writes against Settings1 before showing success

- **Status:** Proposed
- **Date:** 2026-09-23
- **Owners:** Settings and SettingsClient
- **Supersedes:** None
- **Superseded by:** None

## Context

The Date & time route already edits `services.calendarWeekStart`, but its
adapter discarded `setUserValue()` admission and did not observe completion.
A retained snapshot was treated as editable whenever the client was not
Unavailable, including Authenticating and Degraded. Its picker could therefore
invite a write that SettingsClient would reject and give no explanation. A
successful commit reply can also precede a fresh snapshot, so moving the picker
on that reply would invent confirmed state.

## Decision

`WeekStartPreference` separates the last confirmed value from the mutation
state: idle, pending, refused, conflict, or uncertain. The Settings1 adapter is
purpose-scoped to exactly one key and owns its transport/client for its QObject
lifetime on the constructing thread. It accepts a write only when the client
is admissible; a failed admission is immediately visible. A confirmed rejection
has its own result. An Applied reply starts a bounded wait for a snapshot from
the same exact owner and epoch at least as new as `revisionAfter`; only that
snapshot may clear pending. A stale readback cannot claim success. A newer
snapshot with a different value reports conflict. Owner replacement, lost
reply, or expired readback produces uncertainty, never a replay. Retry asks for
a snapshot only. Published values follow confirmed snapshots, including
external edits; operation diagnostics survive an unchanged refresh.

The public SettingsClient adds `canSetUserValue(key)` and
`writeAdmissionChanged()`. The preview shares the state, scope, request, write,
and token guard with `setUserValue()`, for clients whose controls need accurate
availability. It is a same-thread observation, not a reservation: the actual
mutation still validates the value and returns its own admission result. The
client retains ownership of the transport reference for its lifetime and does
not change the Settings1 wire or persistence schema.

## Consequences

The Date & time page shows separate calendar availability, pending, and error
cards, plus a keyboard-accessible read-only Retry action. System timedate1
behavior stays under [ADR-0211](0211-the-clock-and-region-page-acts-on-the-platforms-own-services.md).
The focused production-adapter test injects a fake transport with nonexistent
bus addresses; no real Settings1 or system clock is touched. A live installed
session remains to be qualified separately.
