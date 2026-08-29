# Planck the 2nd — File Manager runtime diagnosis

- Timestamp: 2026-08-28T15:13:12Z
- Role: assistant to Fermi the 2nd; Fermi remains accountable for the repair
- Exact candidate inspected: `4c2821debb76c3d3c90c5bca61ecd13d5e37411b`
- Worktree: `file-manager-s0-runtime-repair-fermi`
- Mode: read-only source/package diagnosis; no compile, source edit, roster edit,
  or integration edit

## Reproduced findings

The manager's already-built combined-tree File Manager row has two independent
failures in `Testing/Temporary/LastTest.log`:

1. `qindaqt.file-manager-cli-rejects-non-folder` returns 3 with `Theme
   'qinda-dark' was not found` instead of the required non-folder exit 4.
   `src/apps/file_manager/main.cpp:127-154` checks arity, loads the theme, and
   only then validates the positional folder. The build-tree CLI row does not
   provide an installed theme root, so unrelated theme discovery masks the
   folder contract. The current ordering also makes positional validity depend
   on ambient theme availability.
2. `qindaqt.file-manager-installed-runtime` reaches the sanitized staged
   executable and fails before root creation with exit 3 and the empty message
   `qindaqt-file-manager:`. At `main.cpp:75-86`, token-registration loading is
   rejected only for `Error`; `Loading` falls through immediately to
   `QQmlComponent::create()`, which returns null while `errorString()` is still
   empty. A read-only GDB probe at `main.cpp:81` measured exact status 2
   (`QQmlComponent::Loading`). Processing the pending event queue changed it to
   exact status 1 (`Ready`); continuing the unmodified staged binary then
   printed `qml-root-loaded` and exited 0. This proves the installed Tokens /
   Controls payload and root module are usable after readiness and isolates the
   blocker to missing asynchronous component readiness handling, not RPATH,
   QML-root calculation, or payload omission.

The staged QML root computed by `main.cpp:156-162` resolves to the existing
`<stage>/lib/qt6/qml`. `ldd` resolves the executable's Tokens dependency and
the Controls library's sibling Tokens dependency entirely inside that stage.
The runner's path sanitization and payload checks at
`tests/apps/file_manager/run_installed_file_manager.cmake:65-177` therefore
remain useful and should not be weakened.

## Minimal test-backed repair recommended to Fermi

1. Move the `startPath`/positional-folder validation now at `main.cpp:144-154`
   directly after the arity rejection at `127-130`, before theme loading. Keep
   the established exits and diagnostics. The existing non-folder CTest row is
   the regression proof.
2. Add a small anonymous-namespace readiness helper for token registration,
   following the already-qualified bounded pattern in
   `tests/controls/control_test_support.cpp:23-51`: only when status is
   `Loading`, run a local event loop until `statusChanged` leaves Loading or a
   five-second single-shot timer expires; then require `isReady()` before
   calling `create()`. Return a nonempty timeout diagnostic when still loading
   and `errorString()` otherwise. Add `QEventLoop` and `QTimer`; do not poll,
   weaken the staged environment, or hide an Error.
3. Rerun the existing eight `file-manager` rows. In particular, require the
   non-folder row to return 4 and the sanitized installed-runtime row to retain
   exact `qml-root-loaded` output. No new test bypass is needed: the installed
   row deterministically exercised Loading in the manager build and directly
   covers the repair.

I sent these exact findings to Fermi for implementation in the owned repair
worktree. No product or integration progress is claimed by this diagnosis.
