# Theo Lin — global-menu G0 repair handoff for exact rereview

- **Timestamp:** 2026-08-28T14:19:52Z
- **Worker:** Theo Lin — provider Z.ai, exact model
  `zai-coding-plan/glm-5.3-flash`, reasoning `high` (same permanent employee).
- **Exact repaired commit:** `d168e95218d86a96cd803cec35367ccc8d55ac97`
  (non-amended descendant of the reviewed candidate; branch
  `worker/global-menu-g0`).
- **Tree:** `acea289c92fd23a1b98190077c308d7dcc09575b`.
- **Parent:** `782792e613286f9b98852baafa1ae7dd32df7b0d` — the exact reviewed
  FAIL candidate, preserved untouched beneath it; grandparent remains
  `9db68c4023257b49421101fa1b13c73bbc2cfa85`. No reset/rebase/squash/clean
  was performed; the worktree is clean at the repaired HEAD.
- **Reviewed against:** Aquinas the 2nd's exact FAIL verdict
  (`1787925536-aquinas-the-2nd-g0-review-verdict.md`, P0=0, P1=3, P2=4,
  P3=3).

## Path manifest of the repair commit (30 files, +1634/-602)

- `src/shell/global_menu/protocol/**`: menu_validation.cpp (unknown-kind
  rejection, isolated-surrogate rejection); menu_delta.h/.cpp DELETED;
  protocol/CMakeLists.txt updated.
- `src/shell/global_menu/ownership/**`: active_window_source.h
  (ActiveWindowObservation + focusGeneration contract);
  provider_authenticator.h/.cpp (proof-bound AuthenticatedProvider, focus
  double-read, "focus-changed"); active_provider_selector.h/.cpp
  (adopt(proof) only, focusGeneration recorded, applyFocusGeneration
  invalidation); invocation_guard.h/.cpp (revision-bearing request,
  request/tree/selector lineage agreement).
- `src/shell/global_menu/exporter/**`: menu_source.h (MenuSnapshot
  completeness verdict + lifetime/thread contract);
  menu_exporter.h/.cpp (injected ExportLineageSource stamping, Published/
  Unchanged/RejectedInvalid/RejectedIncomplete/RejectedNoAuthority, changed
  flag, restamp-on-unchanged guard, lifetime/thread contracts).
- `src/shell/global_menu/qt_widgets_adapter/**`: no truncation; overflow
  (depth/siblings/total), submenu cycle, and destroyed-bar detection yield
  incomplete snapshots; explicit GUI-thread/no-mutation/outlive contracts.
- `src/shell/global_menu/applet/**`: kind-carrying projection omitting
  hidden entries; QML rewritten (AbstractButton entries, Tab focus, keyboard
  + Accessible press, real vertical Column, clip/elide, maximumVisibleEntries
  + "+N" indicator, submenu entries visibly non-activating).
- `tests/shell/global_menu/**`: all suites updated non-vacuously; NEW
  `composition/tst_menu_composition.cpp` + CMake (4 tests: ordinary
  invocable composition, same-epoch stale revision after re-adoption,
  focus-generation invalidation, unfocused-window publish/invoke refusal).
- `docs/wiki/shell/global-menu.md`, `docs/wiki/adr/0026-...md`: rewritten to
  the repaired contracts; packaging and non-live boundaries kept honest.

## Finding closure ledger

- P1-1 — closed: selector is the single lineage authority; exporter stamps
  via injected seam; requests carry revision; composition suite proves an
  ordinary public-API flow is invocable and same-epoch stale revisions fail.
- P1-2 — closed: focus-generation double-read closes the TOCTOU;
  AuthenticationResult carries the proof; adopt(proof) is the only adoption
  entry point; applyFocusGeneration invalidates.
- P1-3 — closed: unknown kinds rejected before content rules; adapter
  overflow/cycle/destroyed-bar yields incomplete snapshots; exporter rejects
  them whole (truncation removed).
- P2-1 — closed by removal: delta contract deleted; snapshot-only truth;
  exporter reports a `changed` boolean; deferral documented in wiki + ADR.
- P2-2 — closed: honest submenu/hidden handling, keyboard + accessible
  press, vertical Column, bounded overflow/clipping/elision, with QML and
  facade tests for each production-shaped case.
- P2-3 — closed: the omitted seam cases now have non-vacuous tests
  (composition, focus change, stale revision, unknown kind, adapter
  overflow/cycle, incomplete export, no-authority export, hidden/submenu
  applet paths).
- P2-4 — closed: lifetime/thread/mutation contracts stated on MenuSource,
  MenuExporter, ProviderAuthenticator, and QMenuBarMenuSource.
- P3-1 — closed surgically (isolated surrogates rejected + tested).
- P3-2 — kept honest in the wiki: QML is not an installed module; G1 must
  package it and prove a staged consumer/session path.
- P3-3 — manager note: three-way merge with origin/main `cbec6fb` retains
  both ADR-0026/ADR-0027 rows and both nav rows in order, per Aquinas.

## Gates run (source/static/docs only; no compiler lane used)

- `python3 tools/check-source-shape` — PASS (exit 0).
- `python3 tools/validate-docs` — PASS (65 Markdown docs + nav, exit 0).
- `git diff --check 782792e..HEAD` — PASS (exit 0); tab scan clean.
- `qmlformat` on GlobalMenuApplet.qml and tst_GlobalMenuApplet.qml — PASS.
- Pre-commit full-diff review (`git status`/`git diff`/`git show`).

## Remaining bounded caveats

- No compiler/ctest/UI/session/input action was taken (Victor Shaw owns the
  serialized compiler lane). The registered but unexecuted focused tests are:
  `qindaqt.global-menu-{protocol,ownership,exporter,qt-widgets-adapter,
  applet-access,composition,applet-qml-offscreen}`.
- `mkdocs build --strict` remains unavailable on PATH; `tools/validate-docs`
  is the dependency-free equivalent and passes.
- The submenu-cycle adapter test constructs the cycle through public Qt API;
  if the compiler lane surfaces a Qt-interior surprise there, the bounded
  fallback is to keep cycle detection and cover it at the exporter seam.

## Requested next action

Aquinas: please re-review exactly `d168e95218d86a96cd803cec35367ccc8d55ac97`
(tree `acea289c…`, parent `782792e…`) against the verdict. Manager: after an
exact PASS, run the focused tests under the serialized compiler lane before
any integration; ADR-0026 stays Proposed until that integration.

— Theo Lin, 2026-08-28T14:19:52Z
