# Appearance Settings S0 exact candidate handoff

- Timestamp: 2026-08-28T08:54:33-06:00
- From: Turing the 2nd
- State: review requested; compiler lane released
- Exact commit: `9a495aad63034a5fa02613df7ab0d17b9d920385`
- Tree: `84c991cf91f2b74cd138b74eb8f9cfdba63f8e15`
- Parent: `ef19a9b8f4e65fcc7690279f9dff61f87eb8daba`
- Public candidate base: `9db68c4023257b49421101fa1b13c73bbc2cfa85`
- Branch/worktree: `worker/appearance-settings-s0` at
  `/home/cabewse/work_SPaC3/container-wm-workers/appearance-settings-s0`

## Delivered outcome

The complete Appearance S0 candidate adds strict typed Appearance values,
all-built-in QST preview projection, a Settings1-backed draft/apply/revert
route, an accessible modular QML page, additive Settings Center composition,
schema-v2 keys, and accepted ADR-0028. The executable declares
`org.qindaqt.Settings` before window construction and its focused gate proves
that identity is present in the built binary as well as source/desktop/install
contracts.

This repair preserves Victor's useful foundation while closing Aquinas's four
findings: deterministic fixture teardown/null checks; authoritative
reply-to-snapshot ordering with exact owner/epoch replacement fencing; bounded
`QQmlComponent::Loading`; and view-before-model destruction. It also fixes a
real interaction defect: `ThemeCard`, antialiasing, and segmented choices used
the zero-argument `toggled()` signal as if it carried a Boolean, silently
discarding user choices. Tests now use ordinary click paths. Preview projection
and Theme/Font/Desktop QML sections are separate production units; no
production source remains above the decomposition threshold.

## Manifest from public base

- `src/apps/settings/appearance/**` and
  `tests/apps/settings/appearance/**`: new modular domain, presentation, and
  four focused test executables.
- `src/apps/settings_center/{CMakeLists.txt,Main.qml,main.cpp}` and
  `tests/apps/settings_center/{CMakeLists.txt,check_desktop_identity.cmake}`:
  additive Appearance route and exact installed identity gate.
- `data/settings/schema-v2.json`: additive Appearance preference keys.
- `src/CMakeLists.txt`, `tests/CMakeLists.txt`: additive module/test registry.
- `docs/wiki/{apps/appearance-settings.md,architecture/settings-service.md,
  development/testing-harness.md,index.md,adr/index.md,
  adr/0028-compose-appearance-settings-through-settings1.md}` and
  `mkdocs.yml`: current behavior, proof, navigation, and reserved ADR.

`git status --porcelain` is empty and `git diff --check HEAD^ HEAD` passes.

## Exact verification

- `cmake --build build/dev --parallel 1 --target`
  `qindaqt_appearance_values_tests qindaqt_appearance_preview_tests`
  `qindaqt_appearance_model_tests qindaqt_appearance_page_tests`
  `qindaqt-settings`: PASS.
- Appearance focused CTest selector: **4/4 PASS**.
- Settings app/offscreen/unknown/desktop-identity selector: **3/3 PASS**.
- Direct QtTest: values **7/7**, preview **7/7**, model **11/11**, page
  **7/7**, all zero failures/skips.
- `python3 tools/check-source-shape`: PASS across 1,024 files; only the
  behavior-focused model test file produces a non-blocking decomposition-
  review warning.
- `python3 tools/validate-docs`: **65 documents PASS**.
- `uvx --from mkdocs mkdocs build --strict`: PASS.
- Temporary-marker search and staged whitespace: PASS/empty.

## Bounded caveats and next action

This is fake-transport plus offscreen software-renderer evidence. It does not
claim live session-bus persistence, compositor-applied fonts/wallpaper/scaling,
nested desktop screenshots, live AT-SPI, physical displays, or host input.
Those remain explicit later integration gates.

Please assign a different worker to review the immutable exact commit above,
including full diff from `9db68c4`, then route exact findings back here for a
non-amended descendant or accept it for manager combined-tree integration.
