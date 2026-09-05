# Andrea Ghez — bounded repair handoff (first-party menu export)

- Persona: **Andrea Ghez**, accountable implementer (same as candidate)
- Provider/model: Z.AI GLM `zai-coding-plan/glm-5.3`, reasoning high
- Exact candidate commit SHA: `ba88f0b153a762ff4ab604bc5c2157a44a3cc296`
- Tree SHA: `53c06fc095ab74af157912df1c9e71902c47b98d`
- Exact base SHA: `23b549db` (my prior handoff record on top of reviewed
  candidate `e8e5170b`; product base is `e8e5170b`)
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/first-party-menu-export`
- Branch: `worker/first-party-menu-export`
- Responds to: Elizabeth Blackburn verdict
  `/home/cabewse/work_SPaC3/builds/qindaqt/lanes/review-menuexport-codex/verdict.md`
  REJECT P1-01 (stale window identity on native-surface recreation)

## What this repairs

`ApplicationMenuExport::eventFilter()` handled only `QEvent::Close`. On
`QEvent::PlatformSurface`:

- `SurfaceAboutToBeDestroyed` withdraws synchronously — registrar
  `UnregisterWindow` for the registered id, platform identity withdrawal while
  the surface still exists, serial advanced so in-flight `RegisterWindow`
  replies for the dead identity cannot publish, status
  `WaitingForRegistrar` / failure code `window-surface-destroyed`.
- `SurfaceCreated` republishes on a queued turn with the freshly obtained
  identity against the current registrar owner (queueing is required:
  `SurfaceCreated` can arrive synchronously inside `publishIdentity` when an
  X11 `winId` read creates the surface; guards recheck
  started/surface/identity/owner at execution).
- `publishIdentity()` fails closed while the surface is dead, so a registrar
  owner change during the recreation window can never re-register the old
  identity. No polling, no identity guessing. The status enum is unchanged
  (smallest public boundary; choice recorded in
  `docs/wiki/shell/global-menu.md`).
- Close/quit teardown and dbusmenu revision behavior unchanged; the endpoint
  keeps serving the live tree across recreation. Terminal, Text Editor, and
  File Manager benefit through the shared exporter with no per-app edits.

## Changed paths (sorted)

- `docs/wiki/development/testing-harness.md`
- `docs/wiki/shell/global-menu.md`
- `src/app_shell/menu_export/src/application_menu_export.cpp`
- `tests/app_shell/CMakeLists.txt`
- `tests/app_shell/tst_application_menu_export_surface.cpp` (new)

## Evidence (all commands actually run; exit status and counts)

Negative control on the unrepaired exporter — product file temporarily
reverted to its `e8e5170b` content (`git stash push -- <file>`), test target
rebuilt, row run, repair restored (`git stash pop`):

- `env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir
  /home/cabewse/work_SPaC3/builds/qindaqt/first-party-menu-export/debug -R
  '^qindaqt\.app-shell-menu-export-surface-recreation' --output-on-failure`
  → ctest exit 8 (row FAILED): rows
  `recreatedSurfaceSwapsExactRegistrarIdentity` and
  `composedFirstPartyExportReRegistersRecreatedSurface` both FAIL on the
  unrepaired exporter;
  `surfaceRecreationWithoutRegistrarStaysFailClosedThenRebinds` passes there
  (the old tree's registrar-absence path was already correct — this row is
  the fail-closed control, not a defect detector). Repair restored and
  rebuilt afterward.

Focused selector, Debug and Release (build root
`/home/cabewse/work_SPaC3/builds/qindaqt/first-party-menu-export`, targets
`src/app_shell/menu_export/all src/apps/file_manager/all src/apps/terminal/all
src/apps/text_editor/all tests/app_shell/all tests/apps/file_manager/all
tests/apps/terminal/all tests/apps/text_editor/all tests/shell/global_menu/all
qindaqt_global_menu_qmlplugin qindaqt-shell`, both built exit 0):

- `env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir <debug|release>
  -R '^qindaqt\.(terminal|editor|text-editor|app-shell|global-menu-|file-manager)'
  --output-on-failure --no-tests=error`
  → Debug exit 0, **90/90 passed, 0 failed** (42.32 s);
  Release exit 0, **90/90 passed, 0 failed** (37.93 s).
  That is the reviewed candidate's 89 rows plus the one new registered row;
  File Manager, Terminal, and Text Editor rows stay green.

Static gates from the worktree root:

- `./tools/validate-docs` → exit 0 (144 Markdown documents + navigation).
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/first-party-menu-export/site`
  → exit 0.
- `./tools/check-source-shape` → exit 0.
- `git diff --check` → exit 0, no output.
- No JSON files changed, so no `python3 -m json.tool` invocation applies.

No nested compositor, host D-Bus service, hardware, uinput, network, or
windowed-host row was run; all D-Bus evidence is `dbus-run-session` private
buses with host display variables unset.

## Remaining bounded caveats

- Same nonclaims as the reviewed candidate: foreign-toolkit exporters and
  installed nested-session qualification remain unclaimed; only private-bus
  evidence is claimed.
- The recreation rows use the offscreen QPA (`QWindow::destroy()/create()`
  delivers the same `PlatformSurface` events the reviewer's standalone
  reproduction observed); a live native Wayland round trip stays part of the
  unclaimed nested-session boundary.
- The registrar-absent recreation row intentionally passes on the unrepaired
  tree (owner-loss handling was already correct); the two failing rows are the
  defect detectors for P1-01.

## Requested next action

Elizabeth Blackburn rechecks this exact candidate commit once (bounded repair
round), then manager integration.
