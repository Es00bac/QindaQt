# Source/static handoff: Appearance Settings S0 awaiting compiler release

- **Timestamp:** 2026-08-28T13:45:00Z
- **From:** Victor Shaw, Appearance Settings S0 implementer
- **To:** Manager (integration/queue owner); Anika Rao then Devika Shah
  (compiler lane); any reviewer
- **Branch/commits:** `worker/appearance-settings-s0` — candidate tip
  **`ef19a9b`** (`ab8f92e` foundation + `ef19a9b` typing-tighten repair),
  parented exactly on public base `9db68c4023257b49421101fa1b13c73bbc2cfa85`.
  Worktree clean.

## Changed paths

Owned (new):

- `src/apps/settings/appearance/**` — values/validation, pure QST-1 preview
  projection, Settings1-backed route model, `QindaQt.SettingsApp.Appearance`
  QML module, route-composition helper.
- `tests/apps/settings/appearance/**` — four focused test executables.
- `docs/wiki/apps/appearance-settings.md`, `docs/wiki/adr/0026-*.md`.

Shared registries, minimal additive edits (per my claim record):

- `src/apps/settings_center/{main.cpp,Main.qml,CMakeLists.txt}` — additive
  `appearance` route seam; notifications route behavior unchanged.
- `data/settings/schema-v2.json` — additive `appearance.colorScheme`,
  `appearance.wallpaperMode`, `appearance.uiScale` with defaults; version
  stays 2; existing partial layers validate per key.
- `src/CMakeLists.txt`, `tests/CMakeLists.txt`, `mkdocs.yml`,
  `docs/wiki/adr/index.md`, `docs/wiki/index.md`,
  `docs/wiki/architecture/settings-service.md` (one additive schema note).

No edits to `src/services/settings_{protocol,service,client}`, `src/settings`,
`src/controls`, `src/design_tokens`, `src/themes`, shell, or AppShell paths.

## Tests

Written and registered, **not yet executed** (blocked: compiler lane is
reserved to Anika then Devika; no runtime evidence is claimed):

- `qindaqt.appearance-values` — token round trips, decode rejections, draft
  validation, scoped-key contract.
- `qindaqt.appearance-preview` — theme precedence, scheme/system fallbacks,
  complete preview maps for all five built-ins, high-contrast QST input.
- `qindaqt.appearance-settings-model` — fake-transport sequencing: per-key
  commits with fresh-base snapshots, conflict stop + explicit re-apply,
  uncertain no-replay, owner-loss abort, diagnostic retention, fail-closed
  decode.
- `qindaqt.appearance-page` — offscreen Controls scene: card selection/gating,
  action-row wiring, accessible roles/status truth, focus/tab chain.
- Inherited: `qindaqt.settings-app-offscreen`,
  `qindaqt.settings-app-rejects-unknown-route`, Settings1 suites.

Executed static gates: schema JSON parse + default/constraint consistency,
mkdocs YAML parse, wiki link resolution across touched pages, line-count
guardrail review (largest file 465 non-blank lines, decomposition reviewed:
the model is one cohesive state machine; the page was factored into
`SegmentedChoiceRow`).

## Bounded caveats

1. Compile/test results are the missing gate; findings will be repaired in
   this worktree on top of `ef19a9b` without amending.
2. `mkdocs build --strict` is not runnable in this environment (mkdocs
   absent); links and nav were checked manually.
3. Multi-key Apply is intentionally a per-key sequence, not an atomic
   transaction (public client has single-key writes only); documented in
   ADR-0026 and the wiki truth table.
4. Appearance route exits 3 with the catalog diagnostic when no installed
   themes exist, rather than rendering token-less controls.

## Requested action

Compiler lane (Anika/Devika) or manager: release compile in this worktree
(or run) `cmake` configure + focused
`ctest -R '^qindaqt\.appearance-' --output-on-failure`, the two settings-app
offscreen gates, and ideally strict mkdocs. I will repair any findings and
post the executable-verification handoff for exact-commit review.
