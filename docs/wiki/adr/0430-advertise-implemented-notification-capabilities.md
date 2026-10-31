# ADR-0430: Advertise the notification capabilities this host implements

- **Status:** Accepted
- **Date:** 2026-10-31
- **Owners:** Shell
- **Supersedes:** The capability-default clause of [ADR-0008](0008-lean-notification-service.md)
- **Superseded by:** None

## Context

[ADR-0008](0008-lean-notification-service.md) chose a default
`GetCapabilities` advertisement of only `body`, on the explicit condition that
a composed host "may add `actions` only when an accessible action UI is
actually connected." That condition is now met and has been for some time:
`freedesktop_notification_codec.cpp` decodes `Notify` actions,
`NotificationService::invokeAction` dispatches them and emits
`ActionInvoked`, and `NotificationCard.qml` renders them as real buttons (plus
an overflow menu past the primary two). The domain also already honours the
`resident` hint: `NotificationService::invokeAction` only auto-closes a
non-resident notification after an action
(`notification_service.cpp`), and the resident host keeps a notification
history in the center. None of this was reflected in `GetCapabilities`.

That gap was not cosmetic. `CriticalNotificationAdapter::show()`
(`src/services/power_service/src/adapters/critical_notification_adapter.cpp`)
calls `GetCapabilities` before showing the critical-battery countdown and
*refuses to show the notification at all* unless `"actions"` is present,
specifically so it never shows a Cancel action the server cannot invoke. With
the production default at only `body`, this check always failed:
`qindafirewall-monitor`-style producers and the critical-battery countdown
alike had no way to discover that the host could, in fact, run their actions.
The test fixture `tests/services/power_service/support/critical_runtime_fixture.h`
had to hand-construct an identity with `{"body", "actions"}` to exercise the
adapter at all, instead of relying on the shipped default — a sign the
default no longer matched reality.

## Decision

The default `FreedesktopServerIdentity::capabilities`
(`freedesktop_notification_server.h`) is now `body`, `actions`,
`persistence`:

- `actions` — already decoded, dispatched, and rendered as buttons.
- `persistence` — a resident notification is not auto-closed, and the
  notification center retains history; this is what the specification's
  `persistence` capability promises callers.

`GetCapabilities` continues to omit `body-markup`, `body-hyperlinks`,
`body-images`, `icon-multi`, `icon-static`, `sound`, and `action-icons`:
nothing in this codebase decodes markup, renders inline images, or plays a
sound yet, so advertising those would be a real lie rather than a stale one.
An embedder that genuinely adds one of those still constructs its own
`FreedesktopServerIdentity` and extends `capabilities` explicitly, exactly as
before.

The codec already treats an unrecognized hint key as inert: it only probes
the specific keys it knows (`urgency`, `category`, `desktop-entry`,
`image-path`, `sound-file`, `sound-name`, `action-icons`, `resident`,
`transient`, `suppress-sound`, the image struct) and never rejects a request
merely for carrying an extra key, so this change required no validation
changes to keep "unknown hint" behaviour non-fatal.

## Consequences

- `CriticalNotificationAdapter::show()` can now actually show the
  critical-battery Cancel countdown against the production host, where it
  previously always rejected the attempt before this change shipped.
- Any other sender that gates on `GetCapabilities` before attaching actions
  gets the same correction for free.
- `tests/services/notifications/tst_freedesktop_notification_dbus.cpp` now
  asserts the real default instead of constructing a widened one, and adds a
  `resident` hint plus one unrecognized hint key to its existing `Notify`
  call to pin that both are accepted.
- `docs/wiki/architecture/notifications-service.md` no longer describes the
  omission as intentional; it is corrected to the implemented set.

See [Notification service](../architecture/notifications-service.md).
