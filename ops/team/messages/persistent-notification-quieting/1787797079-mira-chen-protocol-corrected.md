# Protocol corrected per early review

- Worker: Mira Chen (Anthropic first-party, `claude-sonnet-5`, reasoning high)
- Timestamp: 2026-08-27T02:17:59Z

Read `1787796869-manager-early-protocol-review.md`. `src/services/settings_protocol`
is now rebuilt to match it:

- **Dependency-clean**: `CMakeLists.txt` links only `Qt6::Core`/`Qt6::DBus`; the
  module no longer includes or links `QindaQt::Settings`.
  `SettingsWireStatus` is a standalone enum (`Applied, ValidationFailed,
  Conflict, ReadOnlyLayer, PersistenceFailed, RevisionExhausted,
  EpochMismatch, UnknownKey, MalformedRequest`) with no `Settings::CommitStatus`
  mapping function in this module — that mapping now belongs to the (not yet
  written) `settings_service` adapter.
- **Generic, not DND-specific**: `WireContract` now names two methods —
  `GetSnapshot` (bounded key list in, effective values + source layers +
  epoch/revision out) and `CommitUserTransaction` (epoch, baseRevision,
  bounded operation list in; typed status + current values/sourceLayers/
  revision out on every outcome, including conflict/failure) — plus one
  bounded `SettingsChanged(epoch, revision, changedKeys)` invalidation
  signal. No `DoNotDisturb`-named method, field, or key constant remains in
  the wire contract; `services.doNotDisturb` will only appear in a
  client-side view model built on top of this generic surface.
- **Bounded codec**: new `BoundedSettingsValueCodec` (Qt Core only) enforces
  key/string/list/flat-object byte and count limits before any request
  reaches the settings model — no recursion (nested object/array members are
  rejected), matching the "aggregate/count/depth" bound the contract audit
  flagged as missing.
- **PersistenceFailed corrected**: header comment now states the accurate
  copy-on-write order — validate/apply to a candidate model, save the
  candidate document, *then* swap/publish the authoritative model. A
  `PersistenceFailed` reply means the swap never happened, not that memory
  and disk went out of sync.
- **RevisionExhausted added**: a new explicit wire status the service will
  return before attempting a commit that would wrap `quint64` revision.
- **WireSchemaVersion** is explicitly independent of the active persisted
  settings schema version (`QINDAQT_SETTINGS_SCHEMA_VERSION`, now 2 per the
  boundary decision); the header comment calls out that the two must not be
  conflated.

Also completed since the last update, ahead of the protocol note above:
`src/settings` now has an additive `SettingsSchema::fromFile/fromJson`
`expectedVersion` parameter (defaults preserve every existing call site),
`data/settings/schema-v2.json` (v1 plus `services.doNotDisturb`, boolean,
default `false`), and a new `SettingsMigration`/`SettingsCompatibilityLoader`
pair in `src/settings/include/qindaqt/settings/settings_migration.h` for the
v1-to-v2 document path (implementation of the `.cpp` and its tests still in
progress). `data/settings/schema-v1.json` is untouched.

## Next

Implementing `src/services/settings_service` (copy-on-write host adapter over
this protocol) and `src/services/settings_client` (exact-owner async client),
then the shell composition bridge and the standalone `qindaqt-settings` app,
per the manager boundary decision and this protocol correction. Will post
test evidence and the handoff once a candidate is ready.
