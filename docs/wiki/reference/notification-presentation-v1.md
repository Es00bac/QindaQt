# Notification presentation protocol 1

`org.qindaqt.NotificationPresentation1` is the private resident-host-to-shell
boundary. It is not an application notification submission API and is never a
replacement for `org.freedesktop.Notifications`.

## Address and availability

Production uses the standard notification service destination with:

- object path `/org/qindaqt/NotificationPresentation1`;
- interface `org.qindaqt.NotificationPresentation1`; and
- schema version `1` in every complete snapshot.

The object exists only when the host receives a valid presentation access token
at construction. `qindaqt-notification-host` supplies none when invoked without
an inherited token descriptor, so ordinary standalone startup does not expose
notification content to arbitrary session-bus peers. Registration failure
removes the
private object if needed, releases the standard object and well-known name, and
returns a typed startup failure.

## Presenter authentication

A token is exactly 32 random bytes represented by 64 lowercase hexadecimal
characters. Generation uses the operating-system random source. Comparison
examines every stored byte without an early exit. Tokens must not appear in
process arguments, environment variables, persistent files, logs, diagnostics,
or signals in a production session. `qindaqt-session` sends the same generated
value to host and shell through separate one-shot inherited pipes. Child
arguments contain only the inherited descriptor number, and each child closes
its descriptor after one exact bounded record. A silent writer times out after
two seconds, so a malformed inherited descriptor cannot block startup forever.

`RegisterPresenter(token)` binds the caller's unique D-Bus name and atomically
returns the current complete snapshot. A wrong token, non-unique sender, or
second sender receives `org.freedesktop.DBus.Error.AccessDenied`. Repeating the
registration from the already-bound sender resynchronizes safely. Explicit
`ReleasePresenter()` and unique-name disappearance clear the binding.

All other methods require that exact bound sender. `SnapshotChanged(epoch,
revision)` is a targeted signal, not a bus broadcast, and carries no
notification content.

## Methods

| Method | Result |
| --- | --- |
| `RegisterPresenter(string token)` | Complete snapshot map and presenter binding |
| `GetSnapshot()` | Complete current snapshot map |
| `Dismiss(uint id)` | Applied-operation map after trusted user dismissal |
| `InvokeAction(uint id, string actionKey, string activationToken)` | Applied-operation map after validated action invocation |
| `ReleasePresenter()` | Clears the caller's presenter binding |

Successful operation maps contain `status`, `revisionBefore`, `revisionAfter`,
and `notificationId`. For a shell client, `revisionBefore` must be at least the
accepted snapshot revision from which the operation was initiated, and
`revisionAfter` must be at least `revisionBefore`. Dismiss and non-resident
action results advance the revision. A resident action may return equal before
and after revisions because invoking it does not remove or otherwise mutate the
notification. Equal revisions are not a general relaxation: the client permits
them only for an action whose target was resident in its initiating snapshot.

Invalid arguments/actions use the standard invalid-args error, missing IDs use
`org.qindaqt.NotificationPresentation1.Error.NotFound`, capacity/reentrancy uses
limits-exceeded, and internal model failures use the standard failed error.
Access errors never reveal the current presenter or token.

The shell serializes operations. A valid reply is followed by a complete
snapshot fetch. Timeout, malformed reply, and non-authorization remote failure
also trigger a complete fetch because the client cannot infer whether the host
mutated state before the failure became visible. If another snapshot request is
already outstanding, one more fetch is forced afterward. Authorization failure
discards the binding and requires a new `RegisterPresenter` exchange. Owner
replacement changes the lineage before the interrupted request is reported,
and late replies from the old unique owner or token have no effect.

## Snapshot map

The envelope has exactly `schemaVersion`, `epoch`, `revision`, and
`notifications`. `epoch` is a canonical lowercase UUID generated for one
private-server lifetime. A client lineage is `(well-known unique owner, epoch,
revision)`; an owner or epoch change is a restart and permits revision reset.

Notifications are in strictly ascending nonzero ID order. Each item has exactly:

- `id`, `applicationName`, `applicationIcon`, `summary`, and `body`;
- numeric `urgency` in the range 0–2;
- `desktopEntry` and `imagePath` metadata;
- `resident` and `transient` booleans;
- `createdAtMs`, plus `updatedAtMs` and `expiresAtMs` using `-1` for absent; and
- `actions`, an ordered list of exact `{key, label}` maps with unique keys.

The wire allows at most 256 notifications, 32 actions per item, and 64 MiB of
aggregate exposed UTF-8 text. Per-field limits match or tighten the admitted
notification model: application name 1 KiB, icon/metadata 4 KiB, summary 8 KiB,
body 256 KiB, action key 256 bytes, and action label 2 KiB. Invalid UTF-16,
embedded NUL, duplicate fields/actions, unknown fields, bad timestamps, wrong
types, oversized lists, and over-budget text reject the complete snapshot.
QtDBus array/map arguments are parsed with collection limits before materializing
their nested elements.

Producer unique names and inline image bytes are intentionally absent. The
shell does not need source ownership, and moving admitted image buffers into a
resident UI feed would violate its memory target. Image-path trust and icon
loading policy remain a client/presentation milestone.

Do Not Disturb is intentionally absent from this wire contract. The shell
applies its injected interruption policy after accepting a complete snapshot;
the host still retains and reports every admitted notification. Enabling the
policy therefore requires no method, signal, snapshot field, schema increment,
or presenter reauthentication. See
[ADR-0010](../adr/0010-inject-shell-notification-interruption-policy.md).

## Current boundary

The shared values/decoder, resident-host server, owner-bound asynchronous shell
client, Qt transport, descriptor token channel, and essential-process
supervisor are implemented. The production shell now projects accepted client
snapshots through a bounded active/popup/in-memory-history model into plain-text
QML popup and center surfaces. This did not change the protocol schema or
methods. Operations are serialized, results are checked against their initiating
revision, uncertain outcomes force authoritative recovery, and owner changes
reject the old operation without accepting its late reply. The presentation
model keeps the originating popup until success and exposes bounded, plain-text
busy/error feedback for eight seconds after rejection. Actions are rendered and
forwarded with an empty activation token, so focus transfer is not promised and
the host continues to advertise only `body`. The shell now provides
session-volatile Do Not Disturb without changing this protocol: low/normal
popups are suppressed, critical popups bypass the filter, Active/Recent are
preserved, and disabling the policy does not replay suppressed entries.
Settings persistence, scheduling/inhibition, lock-screen redaction, persistent
history, safe image loading, activation-token acquisition, complete
keyboard/accessibility proof, and child restart policy remain. The private
object stays disabled in the installed standalone host.
