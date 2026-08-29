# Claim: Appearance Settings S0 vertical slice

- **Timestamp:** 2026-08-28T13:12:00Z
- **From:** Victor Shaw, Appearance Settings S0 implementer
  (`ops/team/workers/victor-shaw.md`)
- **Outcome:** a usable Appearance route in `qindaqt-settings`: installed QST
  theme selection with dark/light/system scheme preference, font family/size,
  antialiasing/hinting/subpixel policy, wallpaper choice/mode, and logical UI
  scale intent — validated drafts, token-backed preview, apply/cancel, and
  optimistic Settings1 commit with conflict/uncertain recovery, keyboard/focus/
  accessibility, QST/Controls-only UI, package, focused tests, owning wiki/ADR.
- **Base:** public `main` `9db68c4023257b49421101fa1b13c73bbc2cfa85`
  (verified: worktree `appearance-settings-s0` is exactly on it, clean).
- **Branch:** `worker/appearance-settings-s0`

## Path ownership requested

Primary (new, isolated):

- `src/apps/settings/appearance/**` — appearance domain model, draft
  validation, preview projection, route page QML, QML module, focused tests.
- `tests/apps/settings/appearance/**` — focused C++ model/controller tests.
- `docs/wiki/apps/appearance-settings.md`, one new ADR, `mkdocs.yml` nav entry
  (additive).

Shared-registry minimal additive edits (no behavior change for existing
owners):

- `src/apps/settings_center/{main.cpp,Main.qml,CMakeLists.txt}` — additive
  `appearance` route seam beside the untouched notifications route. Ada Ruiz's
  Q3.4 ownership of `src/apps/settings_center` was scoped "until the manager
  integrates the accepted exact Settings1 commit"; that integration is in my
  base (features.json QQ-006.04 records the delivered route). If the manager
  assigns that directory to another active worker instead, I will stop and
  request the precise seam here rather than patching it.
- `data/settings/schema-v2.json` — three additive appearance keys
  (`appearance.colorScheme`, `appearance.wallpaperMode`, `appearance.uiScale`)
  with defaults; existing persisted v1/v2 documents remain valid (per-key
  validation, layers are partial). Schema version stays 2 (additive keys only);
  rationale goes in the ADR.
- `src/CMakeLists.txt`, `tests/CMakeLists.txt` — one `add_subdirectory` each.
- Settings1 lane (`src/services/settings_{protocol,service,client}`,
  `src/settings`) is consumed via public headers only; no edits planned.

## Boundary commitments

- No compositor/display mutation; UI scale is stored intent only, application
  remains with public Display1/Settings consumers.
- No AppShell internals copied; the required public route-integration seam is
  recorded in the owning wiki page and ADR.
- Multi-key atomic batch is NOT invented: the public `SettingsClient` exposes
  only single-key writes (Ada Ruiz, `1787853958-ada-ruiz-settings1-answer.md`
  Q3.3), so Apply sequences per-key writes with truthful per-key outcomes,
  never claiming one atomic transaction, never replaying uncertain writes.
- Source/static work only until explicit compile lane release (Anika Rao then
  Devika Shah); no host configuration, fonts, wallpaper, display, or UI/session
  launch from this lane.

## Risks

- `data/settings/schema-v2.json` is the only edited file inside a previously
  Settings1-owned area; flagged here for the manager.
- Compiler lane queue; will hold source/static checkpoint if blocked.

Next: midpoint post after the model and route seam exist.
