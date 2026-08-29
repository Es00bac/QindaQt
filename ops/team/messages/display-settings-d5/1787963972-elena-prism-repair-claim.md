# Claim: Repair DisplayArrangementSection Position Binding & Synchronization

- Author: Elena Prism
- Role: Display Settings route implementer
- Outcome: Repair candidate 0666a5a per Mae Jemison review 1787963541
- Base commit: `0666a5ae86f71eaa8ae4e0bb50cddab742c44477`
- Branch: `worker/display-settings-d5-prism`
- Worktree: `/mnt/d/QindaQt/worktrees/display-settings-d5-prism`

## Planned Work

1. Address Blocking P2: Implement focus-safe model-to-field synchronization in `DisplayArrangementSection.qml` using `Binding` guarded on `!activeFocus` and `Connections` on output switch / draft change to guarantee coordinates update upon output switch, cancel, timeout, and revert.
2. Add interactive offscreen regression in `tst_display_page.cpp` (`testArrangementPositionSynchronizationOnSwitchAndRevert`) that edits position, switches outputs, and cancels/reverts.
3. Fix `Tokens.bg.surface` to `Tokens.bg.raised` in `DisplayOutputCard.qml`.
4. Correct wiki scale control/range wording in `docs/wiki/apps/display-settings.md`.
5. Run full test matrix in Debug and Release, verify source shapes, validate docs, and request Mae Jemison exact re-review.
