# REJECT — 0f78850dd19c9dabf070e36208c03eff18898d37

- Reviewer: codex-removable-media-review
- Candidate: 0f78850dd19c9dabf070e36208c03eff18898d37
- Base: 6f5c132d40251b059a5f67ea1a13ffafbd1b8b36
- Verdict: REJECT

## Blocking reproduction

`src/apps/removable_media/udisks_operations.cpp:149–167` handles a delayed Mount result without validating its attachment token before the read-only rollback. If the original attachment disappears and UDisks reuses its object path for a mounted replacement, an unverifiable read-only result dispatches Unmount to the replacement.

The independent fixture-only probe captures a delayed Mount, emits InterfacesRemoved for the original, publishes a different UUID at the same DataPath and waits for its new token, then delivers the old reply naming writable `/tmp`. Its assertion that the replacement must receive no Unmount fails.

```sh
env DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent dbus-run-session -- /tmp/qindaqt-removable-media-review-build/qindaqt_media_udisks_review_probe lateReadOnlyReplyMustNotUnmountReplacement
```

Result: exit 1; 2 passed, 1 failed, 0 skipped. Failure: `Late read-only reply dispatched Unmount to a replacement attachment at the reused path`. Probe source is `/tmp/qindaqt-removable-media-review-harness/tst_media_udisks_review_probe.cpp`, added test `lateReadOnlyReplyMustNotUnmountReplacement`. No host disk was operated on.

Requested repair: fence delayed mutation results and any rollback against current attachment identity; invalidated results must not send follow-up mutations or save successful choices. Review the repaired exact descendant with this same reproduction.

## Passing independent evidence

The review-only CMake harness points exclusively at the exact detached candidate worktree. A separate Debug build produced policy, UDisks, QML and application binaries with exit 0. `ctest --test-dir /tmp/qindaqt-removable-media-review-build -R '^qindaqt.removable-media-' --output-on-failure` passed 3/3, exit 0. These passing gates do not cover the reproduced delayed-rollback hazard. Session gates remain with the manager's affected combined-tree run.
