# Additional blocking findings — 0f78850dd19c9dabf070e36208c03eff18898d37

- Reviewer: codex-removable-media-review
- Candidate: 0f78850dd19c9dabf070e36208c03eff18898d37
- Verdict remains: REJECT

## Safe removal can power off replacement media

`src/apps/removable_media/udisks_operations.cpp:133–139` fences a Remove continuation only by drive-path presence. The independent probe delays the first sibling Unmount, removes the original drive and volume, then publishes a different drive Id/TimeDetected and filesystem UUID at the same object paths. After the old Unmount reply arrives, the pending sequence still dispatches PowerOff to the replacement.

```sh
env DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent dbus-run-session -- /tmp/qindaqt-removable-media-review-build/qindaqt_media_udisks_review_probe delayedSafeRemovalMustNotPowerOffReplacement
```

Result: exit 1; 2 passed, 1 failed, 0 skipped. Failure: `Safe-removal continuation powered off a replacement drive at the reused path`. The test is in `/tmp/qindaqt-removable-media-review-harness/tst_media_udisks_review_probe.cpp`. Guarding only non-Remove callbacks will not repair this. Remove must preserve the captured drive/media generation across its expected encrypted-cleartext disappearance and refuse replacement continuations.

## Notification-owner loss silently loses insertion prompt

`src/apps/removable_media/media_notifications.cpp:20–23,45–48` clears all in-flight state when the notification owner disappears, then ignores a pending Notify result at the old epoch. No fallback window appears and the controller has already marked the attachment seen, so its insertion prompt is lost until the user manually launches the media app.

```sh
env DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent dbus-run-session -- /tmp/qindaqt-removable-media-review-build/qindaqt_media_notifications_review_probe ownerLossDuringInsertionNotifyShowsWindow
```

Result: exit 1; 2 passed, 1 failed, 0 skipped. Failure: `Insertion prompt silently vanished when the notification owner disappeared while Notify was pending`. Source: `/tmp/qindaqt-removable-media-review-harness/tst_media_notifications_review_probe.cpp`. Requested repair: safely surface still-attached pending prompts through a window or resend after owner recovery while fencing withdrawn attachments.

## Additional passing graphical evidence

`env QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent /tmp/qindaqt-removable-media-review-build/qindaqt_media_qml_review_probe formatDialogFitsSmallWindow` passes (exit 0; 3 passed, 0 failed). At the supported 560×540 minimum window, format actions occupy y=437–477. The rendered `/tmp/qindaqt-removable-review-small-format.png` was visually inspected: target, erase consequence, unmount control, filesystem, exact-device field, Cancel and disabled Erase are fully visible.

All reproductions use injected fixtures/private buses. No physical storage operation was performed.
