# Midpoint: Appearance Settings S0 source checkpoint committed

- **Timestamp:** 2026-08-28T13:58:00Z
- **From:** Victor Shaw, Appearance Settings S0 implementer
- **Candidate:** `ab8f92e` on `worker/appearance-settings-s0` (clean tree;
  exact base `9db68c4` preserved as parent)

## What exists now

- `src/apps/settings/appearance` — appearance values + validation, pure QST-1
  preview projection, Settings1-backed route model, QML module
  (`AppearancePage`, `SegmentedChoiceRow`), route-composition helper.
- Additive route seam in `src/apps/settings_center` (`main.cpp`, `Main.qml`,
  CMake) — `--page appearance`; notifications route behavior unchanged.
- `data/settings/schema-v2.json` — three additive keys with defaults; schema
  version stays 2. Flagged per my claim; nobody objected so far.
- Four focused test executables registered under
  `tests/apps/settings/appearance` (values, preview, settings-model, page).
- Owning wiki `docs/wiki/apps/appearance-settings.md`, ADR-0026, mkdocs nav,
  ADR index, wiki index, and a truthful additive note in the settings-service
  page's schema section.

## Design decision worth reviewers' attention

Apply sequences **per-key single-key commits** (public client has no batch)
and, after every Applied reply, waits for the client's fresh authoritative
snapshot before issuing the next key — writing immediately from the commit
reply would use the pre-commit base revision and fail as Conflict. Documented
as AGENT-GUARDs in `appearance_settings_model.cpp` and in ADR-0026; recovery
rules (no replay, private conflict intent, retained diagnostics) match the
accepted Settings1 controller contract.

## Gate status

- Source/static review: done (Settings1 reply-validator status/authority-map
  rules checked against `settings_reply_validation.cpp`; QST publication
  contract checked; schema JSON and mkdocs YAML validated; wiki links
  resolve).
- NOT executed: compile, focused tests, broad suites, strict mkdocs (mkdocs
  not installed in this worktree environment). Compiler lane belongs to
  Anika Rao then Devika Shah per my assignment, so this checkpoint is
  source/static only and claims no runtime evidence.

## Request

Compiler lane: when released, please run
`ctest -R '^qindaqt\.appearance-' --output-on-failure` plus the settings-app
offscreen gates in this worktree, or hand the release back to me and I will
run them myself. Next midpoint will follow any compile findings.
