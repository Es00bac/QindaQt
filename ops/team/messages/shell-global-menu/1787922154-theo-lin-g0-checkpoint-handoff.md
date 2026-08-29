# Theo Lin — global-menu G0 source/static checkpoint handoff

- **Timestamp:** 2026-08-28T13:42:34Z
- **Worker:** Theo Lin — provider Z.ai, exact model
  `zai-coding-plan/glm-5.3-flash`, reasoning `high`.
- **Exact candidate commit:** `782792e613286f9b98852baafa1ae7dd32df7b0d`
  (single, non-amended checkpoint; branch `worker/global-menu-g0`).
- **Tree:** `263d86061585b2b097d9d453d34c2b7ad889f3d9`.
- **Parent:** `9db68c4023257b49421101fa1b13c73bbc2cfa85` — exactly the
  assigned public `main` base; nothing else sat on the branch.
- **Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/global-menu-g0`
  — clean (`git status` empty after the commit).

## What landed (54 files, +3161 lines)

Preserved and completed Celia Hart's interrupted slice; every preserved file
was audited, and only additive repairs/hardening were made on top:

- `protocol` (`QindaQt::GlobalMenuProtocol`): bounded canonical
  `MenuItem`/`MenuTree` values (mnemonic offset instead of toolkit escapes,
  radio groups, UTF-8 ceilings), whole-tree fail-closed hostile-input
  validation, deterministic Removed→Inserted→Updated id-keyed delta, and
  id-based lookup. Celia's files, unchanged.
- `ownership` (`QindaQt::GlobalMenuOwnership`): PID-authenticated
  active-window provider policy via injected compositor-window and
  bus-credential seams, narrower than `com.canonical.AppMenu.Registrar`;
  lineage-assigning `ActiveProviderSelector`; `InvocationGuard` stale-owner
  rejection. Takeover hardening: the guard now also rejects when the
  presented tree's lineage does not match the current selection
  (`invocationRejectsStaleTree` test added).
- `exporter` (`QindaQt::GlobalMenuExporter`): toolkit-neutral `MenuSource`
  pull with exporter-owned epoch/revision and fail-closed retention of the
  last accepted tree. Celia's files, unchanged.
- `qt_widgets_adapter`: `QMenuBarMenuSource` walking the ADR-0022
  `QMenuBar`/`QMenu`/`QAction` shape (object-name ids, mnemonic split,
  exclusive groups as radio groups); only Widgets-linked target. Celia's
  files, unchanged.
- `applet`: `GlobalMenuAppletAccess` facade + `GlobalMenuApplet.qml`.
  Takeover hardening: `publishTree` is now fail-closed (invalid trees
  publish unavailable, never partial content) with two new hostile tests.
- Build: Celia's additive `add_subdirectory` lines preserved; I added the
  six missing test-directory CMake files her interrupted session had not
  written (protocol/ownership/exporter/qt_widgets_adapter/applet/qml),
  including the imported `Qt6::qmltestrunner` runner and offscreen
  environments.
- Docs: new primary page `docs/wiki/shell/global-menu.md` (explicitly
  labels the live feature as NOT shipped), ADR-0026 (Proposed) for the
  canonical-model + authenticated-ownership decision, ADR index entry,
  wiki index link, `mkdocs.yml` nav, and one additive
  `src/shell/global_menu` row in the module-boundaries table. No other
  worker's page or path touched.

## Manifest of owned paths in the commit

`src/shell/global_menu/**`, `tests/shell/global_menu/**`,
`src/CMakeLists.txt`, `tests/CMakeLists.txt`,
`docs/wiki/shell/global-menu.md`,
`docs/wiki/adr/0026-canonical-menu-model-and-authenticated-menu-ownership.md`,
`docs/wiki/adr/index.md`, `docs/wiki/architecture/module-boundaries.md`,
`docs/wiki/index.md`, `mkdocs.yml`.

## Verification evidence

- `python3 tools/check-source-shape` — PASS (1048 files; no file near the
  500-line guardrail in this slice; exit 0).
- `python3 tools/validate-docs` — PASS (65 Markdown documents + mkdocs nav;
  exit 0).
- Trailing-whitespace and tab scan over all new/changed files — clean.
- Full change-set review before commit (`git status`/`git diff`/`git show`).

## Unavailable coverage (bounded caveats)

- No compiler lane is held this milestone: the C++/QML tests
  (`qindaqt.global-menu-protocol`, `-ownership`, `-exporter`,
  `-qt-widgets-adapter`, `-applet-access`, `-applet-qml-offscreen`) are
  registered but NOT executed here; they must run under the compiler lane.
- `mkdocs build --strict` and `ctest -R 'docs|links'` unavailable in this
  environment (`mkdocs` command not found; no `build/dev` configured);
  `tools/validate-docs` is the repo's dependency-free equivalent and passes.
- No D-Bus, session, GUI, compositor, or exporter-runtime interaction was
  attempted, per the lane restriction. The global menu is explicitly not a
  live feature; the manifest still resolves `implementation-unavailable`.

## Requested next action

Independent exact-commit review of `782792e` (not prose) by a second worker,
focusing on: preservation of Celia's contracts, the two takeover hardenings
(InvocationGuard tree-lineage check; fail-closed facade publish), and the
test CMake wiring. After review, the manager should run the focused tests
under a compiler lane; ADR-0026 can move to Accepted on integration.

— Theo Lin, 2026-08-28T13:42:34Z
