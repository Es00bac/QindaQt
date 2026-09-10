# Settings1 protocol version 1

## Endpoint and trust scope

- Well-known name/interface: `org.qindaqt.Settings1`
- Object: `/org/qindaqt/Settings1`
- Authority: ordinary same-user session service; writes affect user overrides
- Lineage: exact unique owner plus fresh service epoch; no PID/executable proof

The wire schema version is independent of persisted settings schema v2.

### Launcher scope

The launcher uses schema-v2 `panels.launcherPinned` and
`panels.launcherRecent`, each a string list with an empty default. Both keys
must be included in its client's snapshot scope, which may also contain other
shell settings. Desktop-entry identity validation, uniqueness and the bounds of
16 pins and 8 recent entries remain launcher-owned. Schema-normalized string
lists persist as JSON arrays and regain their string-list type when loaded.
The old unregistered `shell.launcher.*` names never had a supported persisted
value to migrate. See [ADR-0076](../adr/0076-register-launcher-persistence-in-panel-settings.md)
and the [Launcher contract](../shell/launcher.md).

### Terminal application scope

The Terminal S1 client uses three schema-v2 keys in the existing `services`
domain. `services.terminalProfiles` is a string containing the terminal-owned
canonical JSON profile array (default `[]`),
`services.terminalDefaultProfile` is a string naming the built-in or one listed
user profile (default `builtin-default`), and `services.terminalRestoreWindows` is
a Boolean policy flag (default `false`). The client
requests all three in every snapshot and rejects the logical value as a whole
when any field is absent, incorrectly typed, or internally inconsistent. It
persists no session content, scrollback, argv history, title, process identity,
or window inventory; the opt-in restore inventory enabled by the Boolean flag
is an application-owned state file, not a Settings1 value. Profile structure
and bounds are owned by the
[Terminal application contract](../apps/terminal.md#profiles-and-settings1-persistence).

## Methods and signal

`GetSnapshot(keys)` returns status, exact wire-schema version, active persisted
settings-schema version, epoch, revision, effective values, source layers, and
a bounded message. The request is rejected as a whole when any key is unknown,
duplicated, or malformed. A client rejects a missing or non-v1 wire version;
the reported settings-schema version is lineage metadata rather than the wire
compatibility version.

`CommitUserTransaction(epoch, baseRevision, operations)` accepts a bounded list
of unique-key `set`/`remove` operations. It returns a typed status, revision
before/after, changed keys, and a bounded message. Every known-key semantic
outcome includes the authoritative current value/source for each operated key.
`UnknownKey` instead has exactly empty value and source maps: an unknown key has
no authoritative value, invalid QVariant is not null, and a partial map for a
mixed transaction would be ambiguous. Statuses distinguish applied,
validation, conflict, read-only layer, persistence failure, revision
exhaustion, epoch mismatch, unknown key, and malformed request. Applied no-ops
keep revision and publish nothing.

Status precedence is deterministic. The service first validates the bounded
request shape, then fences the service epoch. Repository schema-key preflight
comes next and precedes base-revision conflict and revision-exhaustion checks.
Thus any well-formed transaction containing an unknown key returns
`UnknownKey`, unchanged before/after revisions, empty changed keys, empty
value/source maps, and no persistence or publication. `EpochMismatch` still
wins before repository evaluation.

Snapshot replies contain exactly eight fields and commit replies exactly ten.
A method reply contains exactly one top-level map argument.
A client accepts a commit result only for the initiating owner/epoch,
settings-schema version, base revision, and operated key; its status must agree
with revision-before/after, the exact known-key map or UnknownKey empty-map
shape, and its bounded, deduplicated changed keys. A contradiction is an
uncertain write followed by resync, never a replay.

`SettingsChanged(epoch, revision, changedKeys)` is an invalidation hint. A
client subscribed to the exact unique sender fetches a complete scoped
snapshot; it does not assemble authority from signals or chase a signal's
claimed target revision. Changed keys are bounded and duplicate-free. Revisions
are global to the repository, so even a valid unrelated-key invalidation
refreshes the scoped client's optimistic commit base.

## Value and resource bounds

Values are JSON-native null, Boolean, bounded integer/finite number, UTF-8
string, array, or string-keyed object. Arrays and objects may nest. In the
canonical in-process form, null is a valid `QMetaType::Nullptr` QVariant;
invalid QVariant means absent/malformed and is rejected. Integers use exact
signed 64-bit representation. Unsigned inputs through `INT64_MAX` normalize to
that representation and wider inputs reject before mutation. Finite integral
floating inputs inside the same range normalize to signed integers (`-0.0`
becomes integer zero); other finite doubles retain their exact value through
document and service restart. NaN and infinities reject.

Strings and object keys must contain well-formed UTF-16 with no embedded NUL;
object keys are non-empty. This prevents UTF-8/JSON/D-Bus replacement from
collapsing distinct values. The v1 limits are:

| Resource | Limit |
| --- | ---: |
| Requested keys / transaction operations / changed keys | 64 |
| Snapshot / commit top-level fields | 8 / 10 (exact) |
| Key UTF-8 bytes | 256 |
| Diagnostic message UTF-8 bytes | 2,048 |
| String UTF-8 bytes | 16,384 |
| Entries per list / map | 512 / 256 |
| Value depth / nodes | 16 / 4,096 |
| Aggregate nodes per snapshot / transaction | 16,384 / 16,384 |
| Aggregate bytes per value / snapshot / transaction | 262,144 / 1,048,576 / 1,048,576 |

QtDBus delivers real arrays/maps as lazy opaque arguments. The decoder streams
those containers under the same aggregate node/byte/depth/list/map/key/string
budgets as ordinary variants and charges each temporarily demarshalled child
before retaining, appending, or inserting it. Duplicate operations, non-finite
numbers, non-JSON Qt values,
malformed D-Bus containers, or any bound overflow reject the complete request
before it reaches the settings model.

Ordinary `QStringList`, opaque D-Bus `as`, and generic `av` string arrays all
normalize to the same canonical `QVariantList` shape. D-Bus variants have no
untyped null, so Settings1 represents null on the wire as the exact reserved
D-Bus signature scalar `g:"v"`. The signature type is outside the JSON-native
caller domain and D-Bus bounds its payload before Qt exposes it. Only that
exact marker decodes to null; all other and caller-supplied signatures reject.

Snapshot values, authoritative values in every commit outcome, and transaction
set values are encoded before QtDBus marshalling. The public Qt transport
repeats complete operation validation/encoding defensively, so a raw invalid or
null operation cannot reach libdbus. Persistent profile and user layers are
normalized to the same canonical JSON domain, and the resident service proves
each loaded layer fits Settings1 wire limits before object registration.

Activation and client restart are serialized. Only one
`StartServiceByName` call may be in flight; failures use the configured retry
backoff, and an explicit UI Retry may safely reattempt a synchronous transport
start. A stopped Qt transport removes its exact-owner and bus-local matches so
the same object can start again. Completion without a stable owner releases the
in-flight guard and backs off. Replacement publishes old-owner loss before
installing the next exact-owner subscription, and pending replies are fenced by
the generation that initiated them. An epoch cannot change within one unique
owner; an equal-revision baseline cannot contradict accepted values or sources.

## Clipboard-history consent key

`services.clipboardHistory` is Boolean and defaults to `false` in both shipped
settings schemas. Clipboard1 treats a resolved `true` as consent only when the
same snapshot's `sourceLayers` map names `user-overrides` for that key. A
`SystemDefaults` or `ProfileDefaults` value is ordinary resolved configuration,
not user consent, even if it is Boolean `true`. Missing, malformed, unavailable,
or differently sourced truth keeps capture disabled and purges prior history.

## Known first-party keys

The schema data files (`data/settings/schema-v*.json`) remain the exhaustive
key registry. One key is owned by the Display Color lane and has semantics
beyond the generic value bounds:

- `displays.colorAssignments` (object, default `{}`) — per-output ICC profile
  assignment intents persisted by the Display Color C1 discovery/assignment
  provider. Each entry maps a display stable ID to exactly
  `{ "profile": <identifier>, "lineage": <64 lowercase hex SHA-256 or empty> }`;
  the value domain above bounds the object, and the stricter record grammar,
  document bounds, and draft/apply/conflict truth are specified in the
  [Display color model](../architecture/display-color-model.md) page. Settings1
  enforces only the generic JSON/resource and top-level object constraints;
  the Display Color consumer performs the stricter assignment-record decode
  and refuses drafts while confirmed persisted content is unusable.
