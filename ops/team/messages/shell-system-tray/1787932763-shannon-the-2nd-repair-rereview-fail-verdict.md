# Shannon the 2nd — status-notifier S0 repair rereview FAIL verdict

- **Time:** 2026-08-28T15:59:23Z
- **Exact candidate:** `78725a95920880930acb55ca0f322c72b4148f17`
- **Exact tree:** `fc52f584223d010bc4f3325de037ee14e974af42`
- **Exact parent:** `637cb94ea1c2e79a6c2f541b60a64ccbbbfab54f`
- **Detached worktree:** clean before and after review
- **Verdict:** **FAIL — P0/P1/P2/P3 = `0/3/3/1`**

This verdict applies only to the immutable repair commit above. Preserve it.
Keira should make one non-amended descendant in the original implementation
worktree and return that exact commit to Shannon for rereview. The serialized
compiler lane is released by this verdict.

## Former ledger disposition

- **Closed:** complete descriptor admission now reaches `validateMenu`; the
  malformed replacement keeps last-known-good data and degrades.
- **Partially closed:** live-owner rebase, stamped owner loss, bounded live-owner
  table, and non-wrapping counter code exist, but watcher-epoch lineage remains
  unsafe and the registry itself is copyable.
- **Closed:** watcher *loss* retains last-known-good items in Degraded and keeps
  request preflight available.
- **Closed:** all tray ADR references use the unoccupied reserved ADR-0032.
- **Closed:** blank/C0/DEL/C1 text refusal, identity accessible fallback, and
  injected presentation strings are present.
- **Closed:** menu children require submenu parents.
- **Mostly closed:** transport sees a narrow event sink with written lifetime,
  threading, attach, and authority boundaries; test strength remains incomplete.
- **Partially closed:** accepted requests now produce a typed owner/generation/
  identity intent, but its revalidation lifetime remains underspecified.
- **Closed:** both parent CMake lines exist, missing-target configuration fails
  closed, and exactly three focused CTest rows register.
- **Closed:** the harness documents the source/unit stopping point and the stale
  QML-binding comment is gone.

## P1 — blocking

1. **The exact-owner key validator rejects conforming D-Bus values.**
   `isValidUniqueBusName` permits only exactly `:<digits>.<digits>` and the test
   pins valid `:x.y` and `:1.42.43` as invalid
   (`status_notifier_validation.cpp:63-85`,
   `tst_status_notifier_values.cpp:376-385`). D-Bus unique names have two or
   more nonempty dot-separated ASCII elements; letters, digits, underscore,
   and hyphen are valid, and unique-name elements may begin with digits. The
   repository's visibility, session-lock, and Display1 boundaries already use
   that canonical grammar. The same validator rejects `/`, although it is the
   canonical root object path (`status_notifier_validation.cpp:87-112` and the
   test at lines 131-132). A local public-libdbus syntax probe returned true for
   `:x.y`, `:1.42.43`, `:a-b._c`, and `/`. Because every owner/item/request
   crosses this gate, conforming sources can be wholly excluded. Repair the
   QtCore-only algorithm and add exact 255-byte, element-boundary, character,
   multi-element, and root-path cases; do not add a QtDBus dependency to S0.
2. **Hostile icon lists are copied before their count bound.**
   `validateIconPayload` evaluates
   `icon.pixmaps + icon.attentionPixmaps` before `validatePixmapList` checks
   `kMaxIconPixmaps` (`status_notifier_validation.cpp:37-49,169-188`). The
   concatenation allocates and copies both attacker-controlled lists, so the
   single descriptor gate performs work proportional to an already-over-count
   payload before refusal. Check both sizes and their overflow-safe aggregate
   first, then validate each list in place. Add exact aggregate-boundary,
   split-over-limit, and large-over-limit regressions.
3. **Watcher reconnect is neither a membership rebaseline nor epoch-fenced.**
   `beginWatcherEpoch()` only clears one Boolean and
   `markInitialPopulationComplete()` carries no expected epoch
   (`status_notifier_registry.cpp:181-192`; event-sink header lines 53-60).
   Item registrations/removals likewise carry only owner generation, which does
   not change on watcher replacement. A late completion or item reply from an
   older watcher can therefore complete or mutate the new epoch. Moreover the
   registered lifecycle test begins a new watcher epoch, reports no fresh item,
   marks population complete, and explicitly expects the pre-reconnect item to
   become Ready again
   (`tst_status_notifier_presentation.cpp:306-321`). An empty replacement
   population must reconcile to Empty rather than silently republish unseen
   membership. Issue a monotonic watcher epoch, stamp all asynchronous
   population/item events, reject old-epoch traffic, and either clear at epoch
   start or retain an unseen set only until completion then prune it. Test stale
   completion/registration/removal and empty/partial/full replacement baselines.

## P2 — required repair

1. **The mutable registry is publicly copy-constructible, duplicating its
   supposedly never-reissued generation authority.** Neither
   `StatusNotifierEventSink` nor `StatusNotifierRegistry` deletes copying
   (`status_notifier_event_sink.h:62-65`; `status_notifier_registry.h:31-109`).
   A compiled exact-tree probe copied a registry after generation 1, then both
   instances independently issued generation 2; it exited 0. Delete copy/move
   for the stateful sink implementation (and make the interface's intent
   unambiguous) and add compile-time type-trait assertions. Counter-exhaustion
   also needs a bounded injected/test seam; current tests never reach it despite
   the handoff's exhaustion-test claim.
2. **Request-intent revalidation is not a complete public operation.** The
   docs say an intent is valid while its owner generation remains current and
   tell an executor to revalidate, but expose only
   `evaluateRequest(target, kind)` (`status_notifier_registry.h:34-73`,
   `status_notifier_types.h:181-193`, `status-tray.md:65-74`). A compiled probe
   accepted identity A, replaced the same live key with identity B without a
   generation change, and obtained another accepted evaluation; the old intent
   was no longer the same user action even though its documented generation
   condition remained true. Add `revalidateIntent(const RequestIntent&)` or
   explicitly require equality with a fresh complete evaluation, and test
   same-key identity replacement, removal, owner rebase, and owner loss.
3. **Several claimed adversarial contracts are not actually tested.** The
   repair matrix required over-depth, over-count, and bad-parent embedded menus
   through both descriptor and registry; values cover depth/bad-parent, while
   the registry covers depth only. No test reaches generation-counter
   exhaustion. The fake's alleged null-attach test calls `attach(nullptr)` only
   while already attached, so it cannot distinguish null refusal from the
   reattach guard (`tst_status_notifier_presentation.cpp:348-358`); it also does
   not exercise different-sink reattach or destructor-detach behavior. Add
   non-vacuous focused cases alongside the watcher-lineage cases above.

## P3 — documentation precision

1. `status-tray.md:120-136`, the harness at lines 540-548, ADR-0032, and the
   handoff overstate exact owner syntax, watcher reconnect/rebaseline, complete
   lifecycle, exhaustion, and adversarial coverage. Update those claims with
   the repaired behavior and actual executed cases; do not label an old item
   unseen in the replacement population as re-observed.

## Evidence actually run on this exact tree

- SHA/tree/parent/detached cleanliness: PASS 4/4 before and after.
- `git diff --check HEAD^..HEAD`: PASS.
- Fresh dependency-light strict Debug configure: PASS (Qt 6.11.1, GCC 16.1.1,
  KWin/shell/production shell/host-uinput disabled).
- Serial focused build: PASS, 20/20 actions for the library and all three tests,
  repository warning set enabled.
- Exact CTest discovery/execution: exactly 3 rows, 3/3 PASS.
- Direct QtTest: values 16/16, registry 19/19, presentation 9/9 PASS. These
  passing results expose test-contract gaps; they do not override the findings.
- `tools/check-source-shape --warnings-as-errors`: PASS, 1017 files, 0 skips.
- `tools/validate-docs`: PASS, 65 documents plus navigation.
- `mkdocs build --strict` using the shared isolated MkDocs environment: PASS.
- Stale ADR-0026/status-notifier and old signature sweep: clean.
- Local public-libdbus syntax probe: all four canonical values above accepted.
- Registry-copy probe and same-key request-lifetime probe: compiled and exited
  0, reproducing the two P2 defects.
- Read-only current-main merge-tree inspection: ADR index and MkDocs navigation
  require small additive conflict resolution with public ADR-0026/0027/0041;
  ADR-0032 itself is unoccupied. This is an integrator seam, not a product
  finding.

No host bus, tray, GUI, Wayland/compositor session, input, configuration,
hardware, or user desktop was contacted.

## Next action

Route this exact ledger to Keira. Preserve `78725a9`; create one non-amended
repair descendant with focused regressions, run the same serial three-row gate
and static/docs gates, then return the exact commit/tree/parent to Shannon for
rereview. **Do not integrate this candidate.**
