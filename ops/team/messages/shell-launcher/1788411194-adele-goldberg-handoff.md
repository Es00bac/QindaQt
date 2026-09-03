# Launcher L1 candidate handoff

- Author: Adele Goldberg (slug `adele-goldberg`)
- Role: Launcher L1 implementer
- Provider/model: Moonshot Kimi `kimi-code/k3`, reasoning high
- Feature: QQ-004.07 Launcher (WIRED L0 → production adapters)
- Candidate commit: `40f1372ef54d4c434626686095a18957ef3cb66f`
- Candidate tree: `ffc230c98fa94a4ef01441d6758f9723d13b625e`
- Exact base: `ce9228d9694622d503d92a38d01986f8f124f188`
- Branch: `worker/launcher-l1`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/launcher-l1`
- Requested next action: independent exact review, then manager integration.

## What the candidate delivers

1. **Scanning adapter** (`ApplicationScanner`): reads `applications/` trees
   from injected roots only, XDG desktop-id mapping (`/` → `-`),
   root-precedence identity claiming through the L0 catalog, per-file
   byte/document ceilings plus a scanned-file ceiling, debounced (200 ms)
   `QFileSystemWatcher` refresh, monotonically increasing generation fencing
   on every rebuild, and degraded truth with bounded scanner diagnostics for
   unreadable roots/files and oversized documents (dangling symlinks
   included). Winning documents are retained, bounded, for execution.
2. **Pinned/recent persistence** (`LauncherPersistenceController`) over the
   public Settings1 client under `shell.launcher.pinned` /
   `shell.launcher.recent`, storing only desktop-entry ids, with ADR-0012
   draft/apply/no-replay semantics: confirmed rejections revert to the last
   confirmed value, uncertain commits resolve through resync, transport loss
   refuses new writes while keeping last confirmed values, and hostile stored
   values poison the whole key rather than partially loading.
3. **Bounded execution** (`LaunchExecutor`): every launch resolves through
   the catalog's single `makeLaunchIntent`; execution keys are re-extracted
   under ceilings from the retained raw document; Exec field-code expansion
   has no shell interpolation and refuses embedded/unknown codes;
   `DBusActivatable` routes through an injected
   `org.freedesktop.Application` activator (dispatch vs completion are
   separate truths); `Terminal=true` routes through an injected terminal
   policy or is refused truthfully; everything else goes through an injected
   spawner, with production `QProcess::startDetached` using an
   allowlist-sanitized environment and the entry's `Path`. Activation tokens
   are explicitly a later slice (ADR-0056).
4. **Compiled QML applet module** `QindaQt.Shell.Launcher` (`LauncherApplet`
   + `LauncherSection`) over the shell-private `LauncherAppletController`,
   using `QindaQt.Controls` and QST-1 tokens, with flat keyboard traversal
   and complete accessible names/roles/descriptions; the applet registers in
   the audited first-party registry with the `applications.launch` grant.

## Changed paths (sorted)

- `docs/wiki/adr/0056-bound-launcher-execution-behind-injected-seams.md` (new)
- `docs/wiki/adr/index.md` (additive row)
- `docs/wiki/architecture/module-boundaries.md` (launcher row + dependency bullet)
- `docs/wiki/development/testing-harness.md` (launcher L1 selector block)
- `docs/wiki/shell/applet-runtime.md` (registry/renderer inventory truth)
- `docs/wiki/shell/launcher.md` (L1 page)
- `mkdocs.yml` (ADR-0056 nav entry)
- `src/applet_runtime/src/builtin_applet_registry.cpp` (one appended entry point)
- `src/shell/launcher/CMakeLists.txt`
- `src/shell/launcher/qml/LauncherApplet.qml` (new)
- `src/shell/launcher/qml/LauncherSection.qml` (new)
- `src/shell/launcher/src/application_scanner.{h,cpp}` (new)
- `src/shell/launcher/src/launch_activator.{h,cpp}` (new)
- `src/shell/launcher/src/launch_execution.{h,cpp}` (new)
- `src/shell/launcher/src/launch_executor.{h,cpp}` (new)
- `src/shell/launcher/src/launch_spawner.{h,cpp}` (new)
- `src/shell/launcher/src/launcher_applet_controller.{h,cpp}` (new)
- `src/shell/launcher/src/launcher_persistence.{h,cpp}` (new)
- `tests/applet_runtime/tst_applet_instance_resolver.cpp` (registry expectations)
- `tests/shell/launcher/CMakeLists.txt`
- `tests/shell/launcher/check_launcher_boundary.cmake` (new)
- `tests/shell/launcher/launcher_runtime_test_support.h` (new)
- `tests/shell/launcher/run_installed_launcher.cmake` (new)
- `tests/shell/launcher/tst_application_scanner.cpp` (new)
- `tests/shell/launcher/tst_launch_execution.cpp` (new)
- `tests/shell/launcher/tst_launch_executor.cpp` (new)
- `tests/shell/launcher/tst_launcher_controller.cpp` (new)
- `tests/shell/launcher/tst_launcher_installed_probe.cpp` (new)
- `tests/shell/launcher/tst_launcher_persistence.cpp` (new)
- `tests/shell/launcher/tst_launcher_qml.cpp` (new)

`data/applets/launcher.json` already carried the `applications.launch`
request and needed no schema change; `data/applet-policy/default.json` needed
no rule (audited-built-in default grants).

## Evidence (all commands actually run, from the worktree root)

Build root `/home/cabewse/work_SPaC3/builds/qindaqt/launcher-l1` (`<ROOT>`).

Configure (exit 0 each), Debug and Release:

    cmake -S . -B <ROOT>/debug -G Ninja \
      -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
      -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
      -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
      -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON

Focused builds (exit 0 each): `cmake --build <ROOT>/{debug,release} --parallel 3`
for the launcher libraries, QML module, all launcher test executables, the
installed probe, and `qindaqt_applet_{catalog,manifest,instance_resolver}_tests`.

Focused tests, Debug (`ctest --test-dir <ROOT>/debug`, exit 0):

- `-R '^qindaqt\.launcher-'`: **14/14 rows passed** — binary totals:
  desktop-entry-parser 42, application-catalog 13, category-model 7,
  search-ranker 11, pinned-recent 11, presentation 12,
  application-scanner 11, execution 7, executor 10, persistence 11,
  controller 8, offscreen (QML) 4 test slots; plus the runtime-boundary
  script (poison negative control accepted-as-rejecting) and the
  installed-package staging/probe script.
- `-R '^qindaqt\.(applet-manifest|applet-catalog|applet-runtime-resolution)$'`:
  **3/3 rows passed**.

Focused tests, Release (`ctest --test-dir <ROOT>/release`, exit 0):

- `-R '^qindaqt\.(launcher-|applet-manifest$|applet-catalog$|applet-runtime-resolution$)'`:
  **17/17 rows passed** (14 launcher + 3 applet integrity).

Static gates (exit 0 each):

- `./tools/validate-docs` (117 documents + navigation validated)
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir <ROOT>/site`
- `./tools/check-source-shape` (0 violations)
- `git diff --check`
- `python3 -m json.tool data/applets/launcher.json > /dev/null`

Focused standalone route (optional route kept working):

- `cmake -S tests/shell/launcher -B <ROOT>/standalone -G Ninja
  -DCMAKE_BUILD_TYPE=Debug -DQINDAQT_ENABLE_STRICT_WARNINGS=ON` (exit 0),
  full build (exit 0), `ctest -R '^qindaqt\.launcher-'`: **14/14 rows passed**;
  `cmake -DSOURCE_ROOT=$PWD -P tests/shell/launcher/check_launcher_boundary.cmake`
  (exit 0).

## Remaining bounded caveats (deliberate non-claims)

- The launcher is registered and resolves `ready` with its grant, but the
  production panel dispatcher does not render it: hosting is a later lane's
  slice (audio-applet-production owns `src/shell/qml/**` this wave).
- The Settings1 schema does not yet register `shell.launcher.pinned` /
  `shell.launcher.recent`; against the production service the controller
  fails closed on `UnknownKey` with visible refusal truth (covered by
  `unknownSchemaKeyFailsClosed`). Schema registration is settings-schema
  authority owned outside this lane.
- No activation (startup-notification) tokens; D-Bus platform-data is empty.
- No real session-bus activation in tests (fake activator only); the only
  real child process any test starts is `/bin/true` (and `/bin/false`).
- The installed-package proof stages the launcher QML module, manifest, and
  policy; Tokens/Controls resolve from the same build tree (their staged
  packaging is owned by their consumers' components).
- No host desktop, nested session, hardware, or real user data was touched.
