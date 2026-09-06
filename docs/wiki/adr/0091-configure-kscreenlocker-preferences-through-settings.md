# ADR-0091: Configure KScreenLocker preferences through a narrow Settings adapter

- **Status:** Accepted
- **Date:** 2026-09-06
- **Owners:** Settings Power route
- **Supersedes:** None
- **Superseded by:** None

## Context

Users need to control idle screen locking without changing manual lock,
resume-lock, or password policy. KScreenLocker owns the documented
`kscreenlockerrc` `[Daemon]` `Autolock` and `Timeout` preferences and exposes
`org.kde.screensaver.configure` on `/ScreenSaver` for live reload. Power1 and
the session-action client do not own this preference authority.

## Decision

The Power route owns a small injected screen-lock-preferences adapter. Its file
store reads and writes only `[Daemon]` `Autolock` and `Timeout`, preserving all
other KScreenLocker keys. Its separate Qt D-Bus adapter requests the standard
KDE `configure` method after a successful save. QML receives only a bounded
model: automatic-lock truth, a one-to-240-minute timeout, pending state, and
plain success or failure text.

A failed live reload does not roll back the saved preference or claim that the
running locker adopted it. The page states that saved-versus-live distinction
and offers one explicit retry. Each mutation re-reads the stored pair and
merges only the intended key, so an external edit to the untouched key
survives; a failed reload rejects the change rather than overwriting an
unreadable config. Retry re-runs the step that failed — reload, save, or live
reload — and only a persisted change may reach the `configure` request, so a
reported success never describes an unsaved change. Manual Lock, LockOnResume,
RequirePassword, and all session actions retain their existing owners.

## Consequences

- Power1 remains power telemetry and brightness only.
- The only direct KDE screen-locker call is confined to the route adapter;
  models and QML never import D-Bus.
- The route has temporary-file round-trip, merge-latest external-edit,
  failed-step retry, and fake-live-configure tests, while host KScreenLocker
  qualification remains manager-owned.
- Timeout UI is disabled while automatic locking is off, but its saved value is
  retained for a later re-enable.

## Revisit when

KScreenLocker publishes a stable typed preferences API, or its documented
configuration group/key or live `configure` contract changes.
