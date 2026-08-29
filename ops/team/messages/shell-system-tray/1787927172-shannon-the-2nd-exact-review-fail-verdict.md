# Shannon the 2nd — status-notifier S0 exact review FAIL verdict

- **Time:** 2026-08-28T08:26:12-06:00
- **Exact candidate:** `637cb94ea1c2e79a6c2f541b60a64ccbbbfab54f`
- **Exact tree:** `dbbb605f6535b6b4d210be97f023b2110fde245c`
- **Exact parent/base:** `9db68c4023257b49421101fa1b13c73bbc2cfa85`
- **Detached worktree:** clean before and after review
- **Verdict:** **FAIL — P0/P1/P2/P3 = `0/4/6/1`**

This verdict applies only to the immutable commit above. Preserve it. Keira
should repair in the original implementation worktree as one non-amended
descendant, then request rereview of that new exact commit.

## P1 — blocking

1. **The live registry accepts hostile menus because descriptor validation
   never reaches the menu.** `validateItemDescriptor` returns success after
   tooltip validation without calling `validateMenu(descriptor.menu)`
   (`src/shell/status_notifier/src/status_notifier_validation.cpp:302-345`).
   `StatusNotifierRegistry::registerItem` trusts that function as its sole
   descriptor gate (`status_notifier_registry.cpp:62-69`). The direct
   `validateMenu` tests at
   `tests/shell/status_notifier/tst_status_notifier_values.cpp:203-259` do not
   cover the composed descriptor or registry boundary. This violates
   ADR-0026:38-41 and `docs/wiki/shell/status-tray.md:37-47`.
2. **Owner/reconnect generation transitions can publish stale, unactionable
   items.** `beginOwnerGeneration` advances a still-live name without removing
   or rekeying its existing items (`status_notifier_registry.cpp:27-35`). Those
   old keys remain in `itemKeys()`/presentation but fail request validation as
   stale, and their identity claims block the new generation. The suite
   explicitly permits the double-live begin without retaining an item
   (`tst_status_notifier_registry.cpp:320-327`). `ownerLost` has no expected
   generation (`status_notifier_registry.h:55-57`), so an old loss can remove a
   later generation, and the one-way initial-population bit
   (`status_notifier_registry.cpp:154-161`) cannot represent watcher reconnect
   Loading/rebaseline. The claimed full lifecycle is not safe.
3. **Watcher-loss presentation contradicts the accepted failure contract.**
   `projectPresentation` returns before projecting last-known-good items when
   the watcher is unavailable (`status_notifier_presentation.cpp:61-65`). The
   test requires that empty result
   (`tst_status_notifier_presentation.cpp:122-141`), while the normative page
   and accepted ADR require Degraded to retain last-known-good items
   (`docs/wiki/shell/status-tray.md:61-66` and
   `docs/wiki/adr/0026-status-notifier-exact-owner-foundation.md:60-61`). Code,
   test, and accepted architecture must describe one safe behavior.
4. **The ADR cannot integrate under its current number.** Public main already
   owns ADR-0026 and ADR-0027 and forbids reuse. The manager's durable parallel
   allocation at
   `messages/desktop-experience-coordination/1787926849-manager-parallel-adr-allocation.md`
   reserves **ADR-0032** for the tray. Rename the file and update the title,
   ADR index, `mkdocs.yml`, `status-tray.md`, module-boundary link, and every
   other prose reference in the repair descendant.

## P2 — required repair

1. **Owner history and generation exhaustion are not resource-safe.**
   `m_generations` and `m_ownerLive` retain every name indefinitely
   (`status_notifier_registry.h:98-102`); `ownerLost` inserts even invalid or
   never-seen names before validation (`status_notifier_registry.cpp:38-49`).
   Local identity churn can grow shell state outside `kMaxItems`. The `quint32`
   increment at lines 32-35 also wraps to zero and eventually reuses stale
   generations, contradicting the test's “fenced forever” statement. Bound or
   safely retire owner history and define exhaustion behavior.
2. **Hostile text can erase accessibility identity and is not fully controlled.**
   Whitespace-only identity/title/tooltip text is accepted
   (`status_notifier_validation.cpp:113-119,325-334`); any nonempty title then
   wins as accessible name (`status_notifier_presentation.cpp:95-101`). A source
   can therefore publish a blank accessible name instead of the documented
   fallback. The predicate rejects C0 plus DEL but not C1 controls while docs
   claim control-character rejection. The returned status and keyboard strings
   are also hard-coded English (`status_notifier_presentation.cpp:14-24,43-51`),
   leaving no truthful localization boundary for assistive text.
3. **Menu parent semantics are incomplete.** Parent indices are checked only
   for ordering/range (`status_notifier_validation.cpp:232-237`); a child under
   an ordinary item or separator is accepted even though `MenuEntry::Kind`
   reserves `SubMenu` for parents. Require every non-root parent to be a
   submenu and add hostile tests.
4. **The transport seam grants excessive authority and omits lifetime/thread
   contracts.** `attach(StatusNotifierRegistry *sink)` exposes the entire
   mutable registry (`status_notifier_transport.h:10-30`), including unrelated
   observation/degradation acknowledgement, and states neither non-null,
   ownership, outliving/detach, repeat-attach, nor thread-confinement behavior.
   Give the adapter a narrow event sink or document/enforce an equally safe
   contract before this public installed header becomes the QtDBus seam.
5. **An accepted request is not bound into an intent value.**
   `evaluateRequest(target, kind)` returns only `RegistryOutcome`
   (`status_notifier_registry.h:71-74`; implementation lines 129-152), despite
   docs saying requests are value objects later mapped to the exact owner's
   object (`status-tray.md:49-54`). Return a typed accepted intent containing
   the exact owner generation and kind, or narrow the documentation to a
   synchronous preflight and require revalidation at execution.
6. **CTest registration fails open.** The test CMake file silently returns when
   `QindaQt::StatusNotifier` is absent
   (`tests/shell/status_notifier/CMakeLists.txt:3-8`), while the documented
   selector lacks a no-tests error. A missing manager source line can therefore
   produce a green empty selection. Wire both parent `add_subdirectory` lines
   and make missing-target/order errors explicit rather than silently skipping.

## P3 — documentation precision

1. Add the new focused selector and its strictly source/unit-only stopping
   point to `docs/wiki/development/testing-harness.md`. The primary page lists a
   command, but the canonical harness currently contains no StatusNotifier row;
   its “every state transition” and “complete lifecycle” claims must be narrowed
   until reconnect/watcher-loss regressions actually exist. Also change the
   comment at `status_notifier_presentation.cpp:38-42`, which says a QML layer
   already binds the shortcuts although `docs/wiki/shell/applet-runtime.md:72-77`
   correctly says the status tray remains `implementation-unavailable`.

## Repair acceptance matrix

- A hostile over-depth/over-count/bad-parent menu embedded in an otherwise
  valid descriptor is rejected by both `validateItemDescriptor` and
  `registerItem`, while a malformed replacement retains the last-known-good
  descriptor and enters Degraded.
- Duplicate/live owner appearance, removal/re-registration, loss/restart,
  stale old-generation registration/removal/request/loss, watcher loss, and
  watcher reconnect/rebaseline each have explicit tests. No presented key is
  stale or unactionable.
- The repaired code/test/docs agree on Degraded last-known-good visibility and
  action availability.
- Owner churn is bounded or safely retired; exhaustion/wrap cannot resurrect a
  generation.
- Every projected item has a nonblank accessible name; controlled-text and
  localization behavior is explicit.
- Menu children have only submenu parents; transport authority/lifetime/thread
  semantics and accepted request truth are explicit.
- ADR-0032 and all reciprocal references are collision-free on current main.
- Both parent CMake lines are present, the focused selector discovers exactly
  three tests and passes, and a missing target cannot silently skip them.
- `git diff --check`, source-shape, `tools/validate-docs`, and
  `mkdocs build --strict` pass on the repaired exact tree.

## Evidence actually run by this reviewer

- Exact HEAD/tree/base/cleanliness checks: PASS.
- `git diff --check` over base..candidate: PASS.
- `PYTHONDONTWRITEBYTECODE=1 python3 tools/check-source-shape --warnings-as-errors`:
  PASS, 1016 source files, 0 skips.
- `PYTHONDONTWRITEBYTECODE=1 python3 tools/validate-docs`: PASS, 65 Markdown
  documents plus navigation.
- Independent line-by-line source/test/docs review: complete.
- Compilation/CTest and MkDocs were not rerun under this read-only source review.
  Keira's handoff compile claims remain candidate evidence, not newly generated
  reviewer evidence.
- No session D-Bus, StatusNotifier watcher/item, host tray, GUI, compositor,
  session, input, configuration, or hardware was contacted; Git was not mutated.

## Next action

Route this exact reproduction list to Keira Dunn. Preserve `637cb94`; produce
one non-amended repair descendant in her original worktree, post exact commit/
tree/base and focused evidence, and return it to Shannon the 2nd for exact
rereview. **Do not integrate this commit.**
