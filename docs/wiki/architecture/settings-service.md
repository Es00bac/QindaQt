# Settings model and service boundary

`qindaqt-settings-service` is the sole authority for QindaQt user-settings
persistence, revision order, migration, and change publication. It owns the
session-bus name `org.qindaqt.Settings1` and is independently D-Bus activatable;
it is not an essential `qindaqt-session` child and has no compositor,
notification-presenter, or lock authority.

## Schema and resolution

The active persisted schema is v2. Immutable
`data/settings/schema-v1.json` remains accepted only as a migration input. V2
adds `services.doNotDisturb`, Boolean, default `false`; it does not repurpose
`services.notifications`. A valid v1 profile/user document is completely
validated against v1, copied to a v2 candidate, validated against v2, and saved
as v2. The new key remains absent so normal system-default resolution supplies
`false`. Corrupt, wrong-layer, stale/unsupported, or invalid migrated input
fails without mutation.

Settings resolve from highest to lowest precedence:

1. volatile session overrides;
2. persisted user overrides;
3. persisted profile defaults; and
4. schema system defaults.

Settings1 exposes ordinary same-user writes to user overrides only. Exact
service-owner binding is lineage fencing against stale replies and signals; it
does not attest a PID or executable and is not a security boundary against the
session user.

## Atomic commits

`SettingsRepository` owns one authoritative `LayeredSettings` value. For each
optimistic transaction it validates and applies operations to a clone, writes
the clone's user document with `QSaveFile`, then swaps the authoritative model
and publishes changed-key invalidation. A save failure leaves memory, file,
revision, and publication unchanged. A raw no-op does not write, increment, or
signal. Stale revisions conflict and an exhausted `quint64` revision returns a
typed terminal failure rather than wrapping.

A committed file with a lost reply is possible at any IPC boundary. Clients
therefore classify timeout, owner change, and transport loss during a write as
uncertain, never replay the operation, and fetch a complete authoritative
snapshot.

## Wire and client lineage

The [Settings1 protocol](../reference/settings1-v1.md) is generic rather than
Do-Not-Disturb-specific. It supports scoped snapshots and one bounded
`CommitUserTransaction` over every JSON-native value shape accepted by active
schema keys, including nested display and panel objects. UTF-8 bytes, aggregate
bytes, nodes, depth, list entries, map entries, requested keys, operations, and
changed keys are all bounded before schema evaluation.

The asynchronous client watches activation/owner change and local bus
disconnect, subscribes to `SettingsChanged` from the exact unique owner before
requesting its baseline, and targets that owner for every call. The service
epoch is fresh per process; revisions compare only inside `(unique owner,
epoch)`. Replacement and late old-owner traffic cannot update published state.

## Do Not Disturb consumers

The generic client has a DND-scoped controller with Loading, Ready, Saving,
Conflict, and Unavailable projections. A conflict refreshes authority and
requires explicit **Apply my choice**; an uncertain result exposes last
confirmed state and requires refresh, never an automatic resubmit.

Shell composition injects confirmed values into the persistence-neutral
notification interruption policy. Before its first authenticated Settings1
baseline the bridge enables DND to fail quiet. After a baseline it retains the
last confirmed value across service, owner, or bus loss. This affects popup
interruption only. The independent authenticated lock privacy gate always
outranks DND and suppresses even critical presentation when state is not
conclusively unlocked.

`qindaqt-settings --page notifications` is a normal Qt Quick application. It
links only the public settings client/controller, not shell or notification
presentation internals. The shell's quick toggle uses that same controller;
presentation DND is read-only to QML. The notification-center applet remains a
read-only indicator. A fixed **Notification settings…** action opens the
ordinary app without exposing arbitrary process-launch capability.

Scheduling, per-application exceptions, inhibition, and the complete
multi-page/applet-based settings catalog remain later work. See
[ADR-0012](../adr/0012-persist-notification-quieting-through-settings1.md).
