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
at construction. The standalone `qindaqt-notification-host` currently supplies
none, so installing or starting that executable does not expose notification
content to arbitrary session-bus peers. Registration failure removes the
private object if needed, releases the standard object and well-known name, and
returns a typed startup failure.

## Presenter authentication

A token is exactly 32 random bytes represented by 64 lowercase hexadecimal
characters. Generation uses the operating-system random source. Comparison
examines every stored byte without an early exit. Tokens must not appear in
process arguments, environment variables, persistent files, logs, diagnostics,
or signals in a production session; session-supervisor descriptor provisioning
is the remaining integration boundary.

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
and `notificationId`. Invalid arguments/actions use the standard invalid-args
error, missing IDs use `org.qindaqt.NotificationPresentation1.Error.NotFound`,
capacity/reentrancy uses limits-exceeded, and internal model failures use the
standard failed error. Access errors never reveal the current presenter or
token.

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

## Current boundary

The shared values/decoder and resident-host server are implemented. The
production supervisor token channel, owner-bound asynchronous shell client,
timeouts/backoff, popup/history QML, accessibility, focus/activation-token
acquisition, do-not-disturb, and lock-screen redaction are not. Until those
pieces land, the private object stays disabled in the installed standalone host
and QindaQt does not advertise notification actions.
