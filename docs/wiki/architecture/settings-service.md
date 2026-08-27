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
validated against v1, copied to a v2 candidate, and validated against v2. After
the service wins its D-Bus name, a migrated user document is atomically
replaced with v2; the immutable installed profile is composed from the
migrated candidate in memory. The new key remains absent so normal
system-default resolution supplies `false`. Corrupt, wrong-layer, missing,
stale/unsupported, or invalid migrated input fails startup without mutation.

Object values are recursively normalized at every schema/layer/migration
ingress to one restart-stable JSON domain. Null has one valid in-memory form,
integers have one signed 64-bit form, and integral doubles normalize
consistently. Invalid QVariant, wide unsigned integers, non-finite numbers,
embedded NUL, ill-formed UTF-16, empty object keys, and non-JSON Qt types
reject. Persistence uses an explicit canonical `QJsonValue` encoder rather
than QVariant's lossy unsigned/numeric conversions. Loaded
system/profile/user state must also fit Settings1 snapshot bounds before the
service registers.

The production composition currently selects the installed
`profile-defaults/qindaqt.json` document beside the active schemas. It is a
required, validated `profile-defaults` layer, applied before the optional user
document. Dynamic profile selection is not part of this slice.

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

After the service validates the request envelope and epoch, the repository
preflights every operation key before checking its base revision or revision
capacity. Any unknown key rejects the complete atomic transaction as
`UnknownKey`, with unchanged revisions, no publication or persistence, and
exactly empty authoritative value/source maps. Known-key outcomes retain one
current value/source per operated key. This distinction prevents a nonexistent
value from being confused with canonical JSON null or a mixed transaction from
returning misleading partial authority.

A committed file with a lost reply is possible at any IPC boundary. Clients
therefore classify timeout, owner change, and transport loss during a write as
uncertain, never replay the operation, and fetch a complete authoritative
snapshot.

## Wire and client lineage

The [Settings1 protocol](../reference/settings1-v1.md) is generic rather than
Do-Not-Disturb-specific. It supports scoped snapshots and one bounded
`CommitUserTransaction` over every JSON-native value shape accepted by active
schema keys, including nested display and panel objects. UTF-8 bytes, aggregate
bytes, nodes, depth, list entries, map entries, requested keys, operations,
fixed reply envelopes, messages, and changed keys are all bounded before schema
evaluation. Real QtDBus lazy arrays/maps are streamed through those shared
budgets: each temporarily demarshalled child is charged before retention,
append, or insert, rather than the complete tree being expanded by an unbounded
`qdbus_cast` first.

Canonical null crosses D-Bus only as the fixed reserved signature scalar
`g:"v"`, never as invalid QVariant or a variable-length byte array. Outbound
snapshot/commit values and transactions are recursively encoded before QtDBus;
the public Qt transport validates again so direct callers cannot reach libdbus
with raw null. Ordinary and opaque collection forms converge to the same
canonical metatypes.

The asynchronous client watches activation/owner change and local bus
disconnect, subscribes to `SettingsChanged` from the exact unique owner before
requesting its baseline, and targets that owner for every call. The service
epoch is fresh per process; revisions compare only inside `(unique owner,
epoch)`. Replacement and late old-owner traffic cannot update published state.
Activation is serialized with one in-flight request and configured bounded
backoff. Synchronous transport-start failure publishes Unavailable truth while
retaining a logical start, so explicit Retry can safely reattempt transport
startup; another identical failure retains that honest Retry state. A successful
activation call that yields no stable owner also releases its in-flight guard
and backs off. Stop/start is symmetric on the same connected bus.

Snapshot and commit envelopes must have their exact field sets. Commit replies
are accepted only when owner, initiating epoch, settings-schema version, base
revision, status-specific operated-key maps, changed-key set, and
status/revision relationship agree. UnknownKey alone requires both authority
maps to be empty; every known-key semantic outcome requires exact operated-key
entries. Any contradiction makes the write uncertain and triggers
authoritative resync without replay. `SettingsChanged` is only a bounded,
deduplicated refresh hint; because repository revisions are global, even an
unrelated-key change refreshes a scoped client's next commit base.

The transport publishes old-owner loss before attempting a replacement
subscription, and every pending request carries its initiating owner generation.
Failed replacement subscription or late old-owner replies therefore fail
closed. Within one unique-owner lifetime the epoch and settings-schema version
are immutable; equal-revision snapshots must also equal the last accepted
values and sources.

## Do Not Disturb consumers

The generic client has a DND-scoped controller with Loading, Ready, Saving,
Conflict, and Unavailable projections. A conflict refreshes authority and
requires explicit **Apply my choice**; an uncertain result exposes last
confirmed state and requires refresh, never an automatic resubmit.
An initial transport-start failure immediately projects Unavailable plus a
bounded diagnostic; Retry performs the safe transport/activation attempt
rather than leaving the surface stranded in Loading.
Owner replacement/loss always outranks Saving or Conflict and exposes
Unavailable/Retry while retaining the last confirmed value. Conflict intent
is private until a fresh authority baseline either resolves it or makes
**Apply my choice** valid again. Confirmed validation, persistence, and
revision-exhaustion diagnostics survive automatic authoritative refresh;
only a new explicit write dismisses them. None of these paths replays a write.

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

Private-bus reconstruction coverage saves through the ordinary controller,
destroys and reopens it, constructs and reconstructs the shell
client/controller/bridge while the service remains, then reconstructs both
service and shell from the same isolated file. Each shell policy starts
fail-quiet and accepts the restored
choice only after a fresh exact-owner baseline; service revisions prove none of
the reconstruction paths replay the commit.

Scheduling, per-application exceptions, inhibition, and the complete
multi-page/applet-based settings catalog remain later work. See
[ADR-0012](../adr/0012-persist-notification-quieting-through-settings1.md).
