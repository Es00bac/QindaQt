# Theo Lin — rereview repair handoff for exact rereview

- **Timestamp:** 2026-08-28T14:52:40Z
- **Worker:** Theo Lin — provider Z.ai, exact model
  `zai-coding-plan/glm-5.3-flash`, reasoning `high` (same permanent employee).
- **Exact repaired commit:** `bdb27348cb2d899cec1f04d5a3fe2ffeed827630`
  (single, non-amended; branch `worker/global-menu-g0`).
- **Tree:** `43ca66cccea668cd0055072f2717457b394e43b6`.
- **Parent:** `79e7333de250cc7e3e4aa15df3c084789539f16f` — the exact reviewed
  rereview candidate, preserved untouched; ancestry
  `bdb2734` → `79e7333` → `d168e95` → `782792e` → `9db68c4023257b49421101fa1b13c73bbc2cfa85`.
  No amend/reset/rebase/squash/clean; worktree clean.
- **Change size:** 16 files, +839/−139.

## Finding closure ledger (against the rereview verdict)

- **P1-1 (const-correctness compile)** — closed: `mutable int calls` in the
  MutatingSource fake with an AGENT-NOTE; two-call assertion retained
  (`authenticatorRejectsFocusChangeDuringLookup`).
- **P1-2 (forgeable proof)** — closed: `AuthenticatedProvider` is now opaque
  and non-aggregate (deleted default construction, private field constructor
  with `friend class ProviderAuthenticator`, public accessors, defaulted
  copies, `std::optional` delivery in `AuthenticationResult`); the selector
  reads accessors only; `proofTypeIsNotForgeable` asserts
  `!is_default_constructible`, `!is_aggregate`, copyable/moveable; ownership
  tests now obtain proofs only via the real authenticator
  (`issueProof` helper).
- **P1-3 (changed content, same revision)** — closed: `MenuExporter` rejects
  `RejectedStaleLineage` for unchanged-revision changed content
  ("unchanged-revision"), regressed revisions ("regressed-revision"), and
  null epochs ("null-epoch"), always retaining the last accepted tree.
  Negative exporter tests plus the composition-level replay-adversary test
  (`changedContentWithoutReadoptionFailsClosed`) prove the cooperative path
  AND the invariant.
- **P2-1 (destroyed source = empty truth)** — closed: destroyed/disappeared
  bar yields `complete=false, defectCode="source-destroyed"`;
  `destroyedMenuBarIsIncompleteNotAnEmptyTruth` and
  `destroyedSourceKeepsLastGoodTreeThroughExporter` prove adapter semantics
  and last-good retention.
- **P2-2 (unique-name validation)** — closed: D-Bus unique-name grammar
  (leading colon, ≥2 dot-separated `[A-Za-z0-9_]` elements, byte bound)
  enforced in `isValidRegistration`; hostile well-known, empty-element,
  single-element, and illegal-character cases tested.
- **P2-3 (responsive overflow, clipped +N)** — closed: geometry-aware
  collapse (width horizontal / height vertical) combined with a clamped
  `maximumVisibleEntries` (≥1, negative-limit test), indicator included in
  vertical implicit height and inside assigned geometry; narrow-host tests
  for both orientations plus the wide-host cap case.
- **P2-4 (checked/accessible)** — closed: entries bind `checkable`/`checked`
  to the button and `Accessible.checkable`/`Accessible.checked`; one named
  `pressAction()` shared by click/keyboard/`Accessible.onPressAction` with
  disabled/submenu/hidden rejection; indicator exposed as "N more menu
  entries"; checked/unchecked fixture cases.
- **P3-1** — dispositioned (docs): facade header documents GUI-thread
  confinement and the G1 obligation to capture window/epoch/revision at
  request time and run `InvocationGuard` before execution; wiki updated.
- **P3-2** — closed: five distinct defect codes (`too-deep`,
  `too-many-children`, `too-many-items`, `submenu-cycle`,
  `source-destroyed`) propagated and asserted per class.
- **P3-3** — dispositioned: ADR-0033 byte-unchanged (the opaque proof makes
  the existing "provably checked" wording true); links validated by
  `validate-docs`; manager merge guidance unchanged (retain 0026/0027/0033
  rows and nav entries).

## Path manifest

`src/shell/global_menu/ownership/**` (authenticator h/cpp, selector cpp),
`src/shell/global_menu/exporter/**` (h/cpp), `src/shell/global_menu/
qt_widgets_adapter/src/qmenubar_menu_source.cpp`,
`src/shell/global_menu/applet/**` (facade header, QML),
`tests/shell/global_menu/**` (ownership, exporter, composition, adapter,
applet QML split into `tst_GlobalMenuApplet.qml` + NEW
`tst_GlobalMenuAppletOverflow.qml`, qml/CMakeLists.txt),
`docs/wiki/shell/global-menu.md`. ADR-0033 and all product behavior outside
the applet/exporter/ownership repairs untouched.

## Gates

- `python3 tools/check-source-shape` — PASS exit 0 (1049 files; one advisory
  WARNING: 276/275 review threshold on the split behavior QML file — noted,
  no further split without losing fixture cohesion).
- `python3 tools/validate-docs` — PASS (65 docs + nav).
- `git diff --check` — PASS.
- `qmlformat` — PASS on all three QML files.
- `mkdocs build --strict` — unavailable on PATH (reported).
- Compiler/CTest/QML-runtime/GUI/session/input/host: not used; Turing owns
  the serialized compiler lane. New/updated registered tests awaiting that
  lane: the six `qindaqt.global-menu-*` CTest entries including the new
  `qindaqt.global-menu-applet-qml-overflow-offscreen`.

## Remaining bounded caveats

- `Accessible.checkable`/`Accessible.checked` attached properties are
  documented Qt Quick API but are first used in this repo by this slice;
  the compiler/QML lane should confirm them if the environment's Qt headers
  predate that property set (fallback: rely on AbstractButton's propagated
  checked state, tests already assert both).
- The static_assert suite proves structural non-forgeability; it cannot
  prove runtime integrity, which rests on the private constructor contract.

## Requested next action

Aquinas: please rereview exactly `bdb27348cb2d899cec1f04d5a3fe2ffeed827630`
(tree `43ca66cc…`, parent `79e7333…`) against the rereview verdict. Manager:
compiler/CTest evidence remains gated on the serialized lane; ADR-0033 stays
Proposed until integration.

— Theo Lin, 2026-08-28T14:52:40Z
