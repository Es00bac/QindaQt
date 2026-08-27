# ADR-0012: Persist notification quieting through Settings1

- **Status:** Accepted
- **Date:** 2026-08-26
- **Owners:** Settings, applications, shell, and notification presentation
- **Supersedes:** ADR-0010's session-volatile lifetime and direct writable-center clause
- **Superseded by:** None

## Context

ADR-0010 correctly placed popup interruption in an injected shell policy, but
its first slice reset DND each shell lifetime and let center QML write the
presentation controller. Persistence now requires one authority that can
migrate old documents, order concurrent writers, survive independent service
and shell restarts, and report failure without letting UI claim a save that did
not happen. Settings v1 is already shipped and must not silently change meaning.

The settings schema contains Object keys for nested display/panel data, so a
nominally generic Settings1 adapter cannot be limited to flat maps. The session
supervisor deliberately couples only the notification host and shell; making
settings a third essential child would defeat independent activation/restart.

## Decision

Keep schema v1 immutable, make schema v2 active, and add the dedicated Boolean
`services.doNotDisturb` defaulting to `false`. Explicitly migrate only valid v1
profile/user documents to validated v2 candidates. Production selects and
validates the installed QindaQt profile before user overrides; an installed v1
profile is migrated in memory, while a migrated user file is persisted only
after winning the service name.

Run `qindaqt-settings-service` as an independently session-bus-activatable
process. It owns user persistence and implements copy-on-write commits:
candidate mutation, atomic candidate save, then authoritative swap and
publication. No-op, conflict, persistence failure, malformed input, and
revision exhaustion are distinct outcomes.

Expose a generic Qt Core/DBus-only Settings1 protocol with recursive
JSON-native values and explicit byte/node/depth/container/transaction bounds.
Ordinary same-user clients may write user overrides. Bind clients to an exact
unique owner plus service epoch, subscribe before baseline, use asynchronous
calls/timeouts, and treat uncertain writes as resync-only. Owner identity fences
lineages; it does not attest a program.

Decode QtDBus lazy containers incrementally under shared aggregate budgets and
bound fixed reply envelopes before materialization. Serialize activation with
one in-flight request and configured backoff. Validate every commit result
against the initiating epoch, schema version, base revision, key maps, and
status/revision relation; treat signals only as bounded refresh hints.

Normalize Object settings recursively to a single persistable JSON form:
`Nullptr` null, signed 64-bit integers (including in-range unsigned inputs),
canonical integral doubles, finite non-integral doubles, strings, lists, and
string-keyed maps. Reject invalid QVariant, wider unsigned integers,
non-finite/non-JSON values, embedded NUL, and malformed Unicode before
mutation. Encode documents explicitly. Carry null over D-Bus only as the exact
reserved, fixed-size signature scalar `g:"v"`, and encode all request/reply
value maps before QtDBus marshalling. Loaded persistent layers must pass the
same wire-fit gate before service registration.

Add a DND-scoped controller above the generic client. Shell composition fails
quiet before its first baseline and retains the last confirmed value across
loss. It only changes the injected interruption policy. Authenticated lock
privacy remains independent and always wins.

Make `qindaqt-settings --page notifications` an ordinary application using the
public client. Keep the shell quick control only through that controller, make
presentation DND read-only to QML, retain the applet's read-only indicator, and
add one fixed notification-settings launch action. Do not add an applet,
layer-shell settings surface, global shortcut, or supervisor child.

## Consequences

- DND survives service, app, and shell reconstruction through one validated
  file and service authority.
- UI exposes loading, saving, conflict, and unavailable/last-confirmed truth;
  timeout never means confirmed failure or automatic commit retry. Explicit UI
  Retry can recover a synchronous transport-start failure.
- Authority loss overrides Saving/Conflict, while a confirmed save rejection
  remains visible across automatic refresh until a new explicit write.
- Every supported Object value has one exact metatype/value after commit,
  atomic save, service reconstruction, and client decode; unsupported values
  fail before authoritative mutation.
- Settings1 remains useful for later schema keys without importing settings
  model or shell types into the wire module.
- The shell may remain conservatively quiet while Settings1 is unavailable;
  this never discards Active/Recent host state or relaxes lock privacy.
- Offscreen/private-bus tests prove structure and process behavior, not live
  assistive technology, compositor focus, KGlobalAccel, or a real session bus.

## Revisit when

Add a new contract for schedules, inhibitors, per-application rules, trusted-
writer credentials, multi-user/system policy, or long-lived preview handles.
Do not broaden this same-user optimistic-transaction protocol implicitly.
