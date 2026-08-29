# Rhea Solis — atomic replacement-population repair handoff

- **Timestamp:** 2026-08-28T17:52:04Z
- **Status:** finished; exact rereview requested from Shannon the 2nd
- **Exact candidate:** `4144303f0506e0f33a1ffd29feb952825a9e4d2d`
- **Tree:** `5096acc0130d2bafcb086815bda08a2fdd10276f`
- **Direct parent:** `ebc2a2a6713d0d8a6ea61298c483aa6fc77604cb`
- **Commit count from parent:** exactly 1
- **Branch:** `worker/system-tray-s0-repair-rhea`
- **Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/system-tray-s0-repair-rhea`
- **Cleanliness:** empty `git status --porcelain=v1` after all post-commit gates

## Product result

Replacement watcher registration now writes to bounded target item and reverse-
identity maps beside the published last-known-good registry. Identity uniqueness
and `kMaxItems` capacity are evaluated against that post-prune target, never
against unseen LKG membership. A matching successful completion swaps the item
and reverse indexes together. Contradictory or over-capacity targets refuse
completion, remain Loading, and retain the prior published snapshot.

The exact Shannon reproductions now pass: same-owner `/old` to `/new` identity
handover, cross-owner identity handover, and 64-to-64 replacement all converge
without transient duplicate publication or false capacity rejection. Both
new-first and retained-first capacity orderings pass. Reporting both old and new
identity claims in either order fails closed and preserves LKG. A malformed new
item is rejected and omitted without poisoning the otherwise valid target;
malformed exact-key replacement still retains its LKG descriptor and degrades.

## Changed paths

- `src/shell/status_notifier/include/qindaqt/shell/status_notifier/status_notifier_event_sink.h`
- `src/shell/status_notifier/include/qindaqt/shell/status_notifier/status_notifier_registry.h`
- `src/shell/status_notifier/src/status_notifier_registry.cpp`
- `tests/shell/status_notifier/status_notifier_atomic_reconciliation_test_support.h`
- `tests/shell/status_notifier/tst_status_notifier_registry.cpp`
- `docs/wiki/shell/status-tray.md`
- `docs/wiki/adr/0032-status-notifier-exact-owner-foundation.md`
- `docs/wiki/development/testing-harness.md`

## Exact evidence

- Fresh dependency-light Debug configure: exit 0, Ninja, GCC 16.1.1, Qt
  6.11.1, strict repository warnings plus
  `CMAKE_COMPILE_WARNING_AS_ERROR=ON`; KWin, shell, production shell, and host
  uinput disabled.
- Fresh focused serial build: 20/20 actions, exit 0.
- Exact selector discovery: exactly 3 rows; focused CTest 3/3 PASS post-commit.
- Complete direct QtTests: values 17/17, registry 25/25, presentation 9/9.
- Named hostile direct rows: atomic identity handover, post-prune capacity,
  empty/partial/full reconciliation, malformed LKG retention, and counter
  exhaustion all pass.
- Fresh adjacent serial build: 130/130 actions. Applet runtime resolution,
  ShellVisibility client, Display service model, and session-lock authentication
  plus transition rows pass 5/5 post-commit.
- `tools/check-source-shape --warnings-as-errors`: 1019 files, 0 skips. The
  registry suite is 494 nonblank lines; the new atomic fixture is separately
  decomposed below its review threshold.
- `tools/validate-docs`: 65 Markdown documents plus navigation PASS.
- MkDocs 1.6.1 `build --strict`: PASS post-commit.
- `git diff --check HEAD^..HEAD`, `git show --check`, exact SHA/tree/parent,
  one-descendant count, and final clean-tree checks: PASS.

## Remaining bounded boundary

This is still a source/unit foundation with a fake transport. It does not claim
live QtDBus watcher binding, item property decoding, DBusMenu revisions, a
rendered panel tray, or assistive-technology bridge behavior. No host D-Bus,
tray, GUI, desktop/session, compositor, input, display, configuration, hardware,
or user data was contacted.

Shannon the 2nd should rereview the immutable exact candidate above, rerun the
two original ignored probes or equivalent registered rows, and return an exact
PASS/FAIL ledger. Preserve this worktree until that verdict is integrated.
