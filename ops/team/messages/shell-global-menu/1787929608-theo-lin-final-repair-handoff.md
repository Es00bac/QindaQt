# Theo Lin — final rereview repair handoff for exact rereview

- **Timestamp:** 2026-08-28T15:16:48Z
- **Worker:** Theo Lin — provider Z.ai, exact model
  `zai-coding-plan/glm-5.3-flash`, reasoning `high` (same permanent employee).
- **Exact repaired commit:** `87cef246a690f5bdc2c860238a1feb37e10957de`
  (single, non-amended; branch `worker/global-menu-g0`).
- **Tree:** `48756678c022970f4e2604fa8dfb1efe0baa7b66`.
- **Parent:** `bdb27348cb2d899cec1f04d5a3fe2ffeed827630` — the exact failed
  final-rereview candidate, preserved untouched; ancestry
  `87cef24` → `bdb2734` → `79e7333` → `d168e95` → `782792e` → `9db68c4023257b49421101fa1b13c73bbc2cfa85`.
  No amend/reset/rebase/squash/clean; worktree clean.
- **Change size:** 12 files, +799/−405 (2 new test files: lineage suite,
  accessibility QML suite).

## Finding closure ledger (against the final rereview verdict)

- **P1-1 (contradictory keyboard test)** — closed: submenu delegates stay
  disabled (honest non-activating presentation); the registered test now
  asserts the Qt Item contract — disabled entries DECLINE active focus
  (`activeFocus` false after `forceActiveFocus`) and Space on them is inert,
  while enabled actions remain focusable and activating. No dead keyboard
  route: keyboard users navigate directly to working entries
  (`test_keyboardFocusSkipsDisabledSubmenuEntries`).
- **P2-1 (bus-name grammar/boundary)** — closed: hyphen admitted
  (`[A-Za-z0-9_-]` elements), exact 255-byte maximum set
  (`kMaxProviderUniqueNameUtf8Bytes` 256→255 with a spec-conformance note in
  menu_limits.h; narrowing documented in place). Regressions: hyphenated
  `:1.worker-2` accepted end-to-end; 255-byte name accepted; 256-byte name
  rejected `invalid-registration` with no proof. Wiki grammar corrected.
- **P2-2 (non-bounding heuristic)** — closed: measured geometry contract.
  `TextMetrics` bounding sizes plus a fixed safety margin give strict upper
  bounds for every label and the "+N" indicator; iterative fitting reserves
  the indicator inside the assigned width/height; entries are pinned to a
  deterministic 24 px; below the documented minimum the applet degrades to
  indicator-only and the indicator hides itself when even it cannot fit —
  no partial label is ever painted inside the clipped root. Tests assert
  the font-metric-independent fit invariant on retained delegates plus
  indicator (`verifyHorizontalFits`/`verifyVerticalFits`), a wide-glyph
  case, below-minimum horizontal and vertical hosts, and the unchanged
  count-cap and clamp behaviors.
- **P2-3 (bypassed accessibility paths)** — closed: the test now emits the
  REAL attached signal (`entries[i].Accessible.pressAction()`) so a broken
  `Accessible.onPressAction` connection fails; the checkable suite
  interactively activates both initially-checked and initially-unchecked
  fixtures (Space) and proves the button is non-toggleable, activation
  requests are emitted, and the provider-owned `checked` binding never
  locally inverts.
- **P3-1 (boundary disagreement)** — closed: module-boundaries row rewritten
  to name separate protocol/policy, export, adapter, and applet-presentation
  targets; Qt Quick confined to the applet-presentation target.
- **P3-2 (stale verification list)** — closed: the page names all eight
  focused gates including the split `...-ownership-lineage` and
  `...-applet-qml-accessibility-offscreen` entries.

All accepted ownership, lineage, destroyed-source, and hostile-input
repairs from prior rounds are preserved; ADR-0033 is byte-unchanged.

## Path manifest

`src/shell/global_menu/protocol/include/.../menu_limits.h`,
`src/shell/global_menu/ownership/src/provider_authenticator.cpp`,
`src/shell/global_menu/applet/qml/GlobalMenuApplet.qml`,
`tests/shell/global_menu/ownership/{tst_menu_ownership.cpp, NEW
tst_menu_lineage.cpp, CMakeLists.txt}`,
`tests/shell/global_menu/qml/{tst_GlobalMenuApplet.qml,
tst_GlobalMenuAppletOverflow.qml, NEW
tst_GlobalMenuAppletAccessibility.qml, CMakeLists.txt}`,
`docs/wiki/shell/global-menu.md`,
`docs/wiki/architecture/module-boundaries.md`.

## Gates

- `python3 tools/check-source-shape` — PASS, exit 0, ZERO warnings/errors
  (prior advisories cleared by the ownership/accessibility splits).
- `python3 tools/validate-docs` — PASS (65 docs + nav).
- `git diff --check` — PASS.
- `qmlformat` — PASS on all four QML files.
- `mkdocs build --strict` — unavailable on PATH (reported).
- Compiler/CTest/QML runtime/GUI/session/input/config: not used; the manager
  owns the lane. Registered-but-unexecuted gates now total nine
  `qindaqt.global-menu-*` CTest entries.

## Remaining bounded caveats

- The measured-fit invariant is proven against whatever fonts the offscreen
  platform supplies at runtime; the safety margin plus upper-bound design
  make it hold for any metrics, but the compiler/QML lane should confirm the
  two below-minimum cases on the lane's Qt build.
- `Accessible.pressAction` signal emission from test JS relies on standard
  QML signal semantics for the attached object; if a Qt version restricts
  it, the fallback is the documented `Accessible.onPressAction` handler
  invocation check — flagged for the runtime lane.

## Requested next action

Aquinas: please rereview exactly `87cef246a690f5bdc2c860238a1feb37e10957de`
(tree `48756678…`, parent `bdb2734…`) against the final verdict. Manager:
compiler/CTest/QML-runtime evidence remains gated on your lane; ADR-0033
stays Proposed until accepted integration.

— Theo Lin, 2026-08-28T15:16:48Z
