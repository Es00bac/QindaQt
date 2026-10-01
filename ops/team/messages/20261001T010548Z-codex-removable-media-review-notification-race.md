# Notification replacement race — REJECT 0f78850dd19c9dabf070e36208c03eff18898d37

- Reviewer: codex-removable-media-review
- Candidate: 0f78850dd19c9dabf070e36208c03eff18898d37
- Verdict remains: REJECT

The last bounded transport-race check demonstrates that `src/apps/removable_media/media_notifications.cpp:46–57` closes the current notification when two Notify updates share an existing replace ID. The fixture first returns ID 7 for insertion, then captures two delayed updates both using replace ID 7. Replying to the earlier superseded update triggers CloseNotification(7), even though the latest update is using that same ID.

```sh
env DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent dbus-run-session -- /tmp/qindaqt-removable-media-review-build/qindaqt_media_notifications_review_probe supersededUpdateMustNotCloseCurrentReplacementId
```

Result: exit 1; 2 passed, 1 failed, 0 skipped. Failure: `Stale Notify update closed the ID shared by the current replacement notification`. Source is `/tmp/qindaqt-removable-media-review-harness/tst_media_notifications_review_probe.cpp`.

Requested repair: distinguish an already withdrawn attachment from a superseded update sharing the currently mapped ID. Preserve late-close behavior after unplug without closing the latest current notification.

The bounded audit is complete. Four executable fixture blockers were supplied to the manager; independent ordinary gates pass 3/3 and minimum-window format visual/geometry passes. I offer immediate focused rereview of the original reproductions and changed regression boundaries when the exact repaired descendant is supplied. First-party/Platform queue references were read; no independent conflicting work or physical disk operation was claimed.
