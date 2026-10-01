# Independent repaired native notification privacy review — ACCEPT

- Reviewer: pf-privacy-review-sol-20261001
- Time: 2026-10-01T10:14:11+00:00
- Immutable candidate: a8b20b06cd944ca3e8d6f66c89306f3bd0222c10
- Product base: ab7640944d67b49f5fcbd198a96ac6c70cad193c
- Rejected predecessor: 1e2ab3ef600a800a49e94280107dd560ddcfd879
- Preserved blocker review: 0e4be1e5150654c82d4711868b0f5c0c32f3c55c on review/pf-privacy-sol-20261001
- Verdict: ACCEPT exact a8b20b06cd944ca3e8d6f66c89306f3bd0222c10 for bounded native notification disclosure and denial-transition source outcome.
- Requested next action: Program Manager integrates the exact accepted candidate and reruns affected combined-tree gates; actual window/native mapping qualification remains the manager's separate combined-producer gate.
- Reviewer changes: employee record and new timestamped messages only. No implementation/main/task/feature/queue edits.

## Reproduction and repair recheck

The original real-owner production-consumer blocker and the subsequent popup-only denial notification blocker are resolved in this exact descendant. Same reviewer checked the bounded repair diff, the original external reproduction and affected focused regression boundary. Production callback wiring, model ownership and live read guards remain as independently reviewed in predecessor review0e4be1e5; no legacy signal-only caller was treated as the production path.

The unchanged external probe source was rebuilt in a separate ignored scratch directory against the exact candidate's public libraries and observer object. It joins a real private D-Bus fixture to a real ordinary kernel socket peer, reaches authenticated native Unlocked, creates the production observer→live-policy→controller/models chain, populates Active/Popup/Recent, replaces the actual selected Session1 owner, and reads retained roles first without event-loop dispatch or a prior observer getter:

```text
immediate active_role=0 popup_role=0 history_role=0 entries=0 policy=0 observer=0 popup_count=0 dismiss=0 invoke=0 operations=0 resets=0
after_watchers popup_count=0 popup_count_changed=1 privacy_changed=1 popup_reset=1 center=0 busy=0 error_empty=1
```

Zero read-triggered resets preserves the non-reentrant getter contract. The normal queued denial now emits exactly one popup-count retirement. The unchanged blocker probe exits1 deliberately when the original `popupChanged.isEmpty()` criterion is false; its exit1 is expected repair evidence, not a failing assertion or crash. Output confirms exact1 count notification. Build/link exit0,2/2 actions. All admission/runtime setup failure codes remain10–21.

`clearPrivatePresentation()` now derives pre-clear popup truth from owned retained `m_popupEntries`, so an already-hidden model read cannot suppress the retirement signal. `NotificationWindowController` independently subscribes to controller `privatePresentationAllowedChanged`, routes it to existing `updateVisibility()`, and disconnects it alongside its existing connections before reset/destruction. The context engine and controller outlive the window owner in shell teardown. Visibility reconciliation reads the denied popup/busy/error/center getters and selects `hide()` for both windows; no new event-dispatching getter or consumer mutation was introduced. The retained-consumer regression asserts zero popup notifications during immediate read-through and exactly one on normal denial. ADR0320/wiki document this behavior.

## Verification evidence

qinda production source directly checked HEADa8b20b06 and clean. Existing candidate build identity remains Debug/shared ON/plugin OFF and its CMake home points at that source. The successful final sibling `pf-shell-lock-20261001-popup-repair-build-final.log` shows the privacy regression and production shell rebuilt/linked (16-action plan); an earlier invocation with a nonexistent target is not buildpass evidence. Reviewer independently reran immutable exact-candidate focused binaries, rebuilt only the external2-action probe and performed local exact-source documentation/static checks.

```sh
QT_FATAL_WARNINGS=1 QT_QPA_PLATFORM=offscreen ctest --test-dir /home/cabewse/work_SPaC3/container-wm.worktrees/pf-shell-lock-20261001/build/focused --output-on-failure --no-tests=error -R '^qindaqt[.](native-lock-monitor|native-lock-qt-transport|notification-privacy-policy|notification-presentation-model|notification-presentation-dnd|notification-presentation-privacy|notification-presentation-app-policy|shell-native-notification-lock|compositor-attachment)$'
python3 tools/validate-docs
mkdocs build --strict --site-dir .cache/rereview-docs
git diff --check 1e2ab3ef a8b20b06
python3 tools/check-source-shape --largest 0 --json
ninja -C /home/cabewse/.cache/pf-privacy-rereview-sol-20261001 -j24 -l24
QT_FATAL_WARNINGS=1 /home/cabewse/.cache/pf-privacy-rereview-sol-20261001/probe
```

- Independent nine focused CTests PASS9/9 exit0, zero skips,6.82s. qinda log `/home/cabewse/.cache/pf-privacy-review-sol-20261001-rereview-tests.log`.
- `tools/validate-docs`:476 Markdown documents and navigation PASS exit0. Strict MkDocs and repair diff whitespace checks PASS exit0. This repository's link checker is validate-docs, not a nonexistent check-doc-links command.
- Exact-descendant source-shape exit1:5329 checked files,57 existing errors, no new issue keys versus reviewed predecessor. Prior exact-base comparison also had57 errors and no new issue keys. Existing oversized shell composition/CMake files remain inherited debt, not a new repair blocker.
- External probe compile/link PASS2/2 exit0; unchanged blocker probe exit1 as expected with exactly1 retirement notification, zero read-triggered resets. qinda artifacts `/home/cabewse/.cache/pf-privacy-rereview-sol-20261001/{probe.cpp,prepare_probe.py,build.ninja,build.log,probe.log}`. Original blocker artifacts remain preserved separately.

Repair changed paths relative to rejected predecessor:

```text
docs/wiki/adr/0320-native-notification-lock-observation.md
docs/wiki/shell/notification-presentation.md
src/services/notification_presentation_model/src/notification_presentation_privacy.cpp
src/shell/runtime/notificationwindowcontroller.cpp
src/shell/runtime/notificationwindowcontroller.h
tests/services/notification_presentation_model/tst_notification_presentation_privacy.cpp
```

## Bounded caveats and help

Already copied Qt values/painted frames cannot be recalled by these guards. This review verifies actual owner-loss admission, retained getters and denial notifications; it does not claim an executed native window/compositor mapping, physical installed-session privacy, or lock/suspend/install/service action. The window hide conclusion is source-backed; the root's later combined native mapping gate remains separate.

Compiler tiny-build exception and private-runtime slot released to root. Read current manager Shell queue/thread before offering compatible successor help: this reviewer is available for read-only verification of the combined-tree mapping gate/logs or an exact later regression reproduction once assigned. No product milestone or integration completion claim.
