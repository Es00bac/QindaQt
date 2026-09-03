# Launcher L2 candidate handoff

- Candidate commit: `7ecdb36f53c60ea03bea40197f17c9cc6bf9b9e1`
- Candidate tree: `9bfab0137d8a623bb0ccbf6ad44aa5e3654523ab`
- Exact base: `ae1e0f1611f7b4e653130ab0905294585974182b`
- Branch: `worker/launcher-composition`
- Requested next action: independent exact review of candidate
  `7ecdb36f53c60ea03bea40197f17c9cc6bf9b9e1`, then manager integration.

## Outcome

The production shell now composes the compiled Launcher applet from explicit
XDG roots, its public Settings1 client, a real bounded process spawner, and a
session-bus activator. The audited `applications.launch` decision is the only
activation grant. Runtime panels receive the purpose-specific launcher facade;
the preview renders the deterministic null-controller fallback. All ten stock
profiles contain exactly one resolved launcher. `LauncherAppletRuntime` ships
the shell and complete Launcher/Controls/Tokens runtime closure, and Settings1
authority loss clears stale pinned/recent identity truth.

## Changed paths

- `data/profiles/gnome-inspired.json`
- `data/profiles/macos-inspired.json`
- `data/profiles/mate-inspired.json`
- `data/profiles/minimal.json`
- `data/profiles/nextstep-inspired.json`
- `data/profiles/unity-inspired.json`
- `data/profiles/windows-classic.json`
- `data/profiles/windows-modern.json`
- `data/profiles/xfce-inspired.json`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/reference/applet-manifest-schema-v1.md`
- `docs/wiki/shell/applet-runtime.md`
- `docs/wiki/shell/launcher.md`
- `src/shell/CMakeLists.txt`
- `src/shell/launcher/src/launcher_persistence.cpp`
- `src/shell/launcher/src/launcher_persistence.h`
- `src/shell/qml/BuiltinAppletContent.qml`
- `src/shell/qml/PanelAppletColumn.qml`
- `src/shell/qml/PanelAppletRow.qml`
- `src/shell/qml/PanelContent.qml`
- `src/shell/qml/RuntimePanel.qml`
- `src/shell/runtime/launcherappletcomposition.cpp`
- `src/shell/runtime/launcherappletcomposition.h`
- `src/shell/runtime/runtimepanelwindowfactory.cpp`
- `src/shell/runtime/runtimepanelwindowfactory.h`
- `src/shell/runtime/shellruntimeapplication.cpp`
- `src/shell/runtime/shellruntimeapplication.h`
- `tests/applet_runtime/tst_applet_instance_resolver.cpp`
- `tests/applets/tst_catalog.cpp`
- `tests/shell/audio_applet/run_shell_component_closure.cmake`
- `tests/shell/launcher/CMakeLists.txt`
- `tests/shell/launcher/check_launcher_boundary.cmake`
- `tests/shell/launcher/check_launcher_contract_text.cmake`
- `tests/shell/launcher/run_installed_launcher.cmake`
- `tests/shell/launcher/tst_launcher_composition.cpp`
- `tests/shell/launcher/tst_launcher_dispatcher.qml`
- `tests/shell/launcher/tst_launcher_persistence.cpp`
- `tests/shell/qml/imports/QindaQt/Shell/Launcher/LauncherApplet.qml`
- `tests/shell/qml/imports/QindaQt/Shell/Launcher/qmldir`

## Acceptance evidence

All commands ran from the candidate worktree. Product-test invocations cleared
ambient session-bus, X11, and Wayland endpoints.

- Debug configure, exact prescribed recipe: exit 0.
- Debug focused build of `qindaqt-shell`, `qindaqt-shell-preview`, all launcher
  test executables, applet manifest/catalog/resolver executables, and
  `qindaqt_shell_runtime_options_tests`: exit 0.
- `env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY ctest
  --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/launcher-composition/debug
  -R '^qindaqt\.launcher-' --output-on-failure --no-tests=error`: exit 0,
  17/17 passed.
- `env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY ctest
  --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/launcher-composition/debug
  -R '^(qindaqt\.applet-(manifest|catalog|runtime-resolution)|qindaqt\.shell-runtime-(options|catalog|component-closure)|qindaqt\.(audio|bluetooth|power)-applet-installed-package)$'
  --output-on-failure --no-tests=error`: exit 0, 9/9 passed.
- Debug build of `qindaqt_profile_json_validation_tests`,
  `qindaqt_profile_tests`, and `qindaqt_profile_value_validation_tests`: exit 0.
- Debug `ctest -R '^qindaqt\.profile-'`: exit 0, 3/3 passed.
- Release configure, exact prescribed recipe: exit 0.
- Release focused build of the same shell, preview, launcher, integrity,
  runtime-options, and profile targets: exit 0.
- Release `ctest -R '^qindaqt\.launcher-'` under the same host-unset isolation:
  exit 0, 17/17 passed.
- Release adjacent selector above under the same isolation: exit 0, 9/9 passed.
- Release `ctest -R '^qindaqt\.profile-'`: exit 0, 3/3 passed.
- `./tools/validate-docs`: exit 0, 126 Markdown documents and navigation
  validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/launcher-composition/site`:
  exit 0.
- `./tools/check-source-shape`: exit 0, 2009 files checked; only the three
  pre-existing decomposition-review warnings were reported.
- `git diff --check`: exit 0.
- `python3 -m json.tool <file> > /dev/null` for each of the nine changed profile
  JSON files: all exit 0.

Repair history is preserved truthfully: the first Debug generation failed
because the installed-row test referenced `KF6::GlobalAccel` before finding its
package; the first focused compile then exposed a QtTest enum-vector comparison
diagnostic; the first launcher run exposed the Settings client retaining its
snapshot object on bus loss; and the first source-shape run exposed an oversized
initialization function. Each issue was repaired, rebuilt, and rerun through
the passing evidence above. One early adjacent invocation had 8 passing rows
and one not-run runtime-options row because that focused executable had not yet
been built; it was built and the complete 9/9 selector was rerun in both
profiles.

## Bounded caveats

- No product test starts a real application. Composition tests use recording
  spawner and activator seams; only pre-existing executor tests use inert
  `/bin/true` and `/bin/false` fixtures.
- This candidate does not claim startup-notification/activation tokens, real
  session-bus application activation, nested-session behavior, host desktop
  behavior, hardware behavior, or a Settings1 schema registration for the two
  launcher keys.
- The production terminal-command prefix remains deliberately unwired, so
  `Terminal=true` desktop entries refuse without a shell fallback.
