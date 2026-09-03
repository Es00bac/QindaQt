# Winifred Asprey — Launcher L1 repair handoff

- Exact product candidate: `86a6d982325e2eaea90720713ac0d46dd86021b1`
- Candidate tree: `64f638f3fed318b70a3989e8bbd6283cbca1b681`
- Exact repair base (rejected product candidate): `40f1372ef54d4c434626686095a18957ef3cb66f`
- Candidate parent (prior handoff-only commit): `5550a9137f8a50cc6326d345065509e8c43cd8c2`
- Original lane base: `ce9228d9694622d503d92a38d01986f8f124f188`
- Branch: `worker/launcher-l1`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/launcher-l1`

## Outcome

All P1/P2/P3 findings in Kay McNulty's verdict against `40f1372` are closed:

- scanner traversal is canonical-root confined, rejects non-regular files, caps reads, and reports hostile links/FIFOs;
- unchanged authoritative Settings1 resyncs converge optimistic uncertain state without replay;
- desktop actions contribute only `Exec` and inherit entry-level `Terminal`, `Path`, and `DBusActivatable`;
- compiled QML is fatal-warning clean, null-safe before access/tokens, renders bounded accessible persistence truth, and has exercised keyboard/accessibility paths;
- the installed-package row relocates the whole staged prefix and loads Launcher/Controls/Tokens only from the moved closure;
- launcher, applet-runtime, testing-harness, ADR, process-fixture, and stored-list contracts now match the implementation.

## Changed paths

- `docs/wiki/adr/0056-bound-launcher-execution-behind-injected-seams.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/shell/applet-runtime.md`
- `docs/wiki/shell/launcher.md`
- `src/shell/launcher/CMakeLists.txt`
- `src/shell/launcher/qml/LauncherApplet.qml`
- `src/shell/launcher/qml/LauncherSection.qml`
- `src/shell/launcher/src/application_scanner.cpp`
- `src/shell/launcher/src/application_scanner.h`
- `src/shell/launcher/src/launch_execution.cpp`
- `src/shell/launcher/src/launch_execution.h`
- `src/shell/launcher/src/launch_executor.cpp`
- `src/shell/launcher/src/launcher_persistence.cpp`
- `src/shell/launcher/src/launcher_persistence.h`
- `tests/shell/launcher/CMakeLists.txt`
- `tests/shell/launcher/run_installed_launcher.cmake`
- `tests/shell/launcher/tst_application_scanner.cpp`
- `tests/shell/launcher/tst_launch_execution.cpp`
- `tests/shell/launcher/tst_launch_executor.cpp`
- `tests/shell/launcher/tst_launcher_installed_probe.cpp`
- `tests/shell/launcher/tst_launcher_persistence.cpp`
- `tests/shell/launcher/tst_launcher_qml.cpp`

## Verification evidence

All commands ran from the lane worktree. The preconfigured assigned build roots were reused as directed; the Release build automatically reran CMake after build-graph edits and completed successfully. Its existing mixed-prefix CMake search-path advisories were non-fatal and did not involve this lane.

### Debug

```text
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/launcher-l1/debug --parallel 3 --target qindaqt_shell_launcher qindaqt_shell_launcher_runtime qindaqt_shell_launcher_qml qindaqt_shell_launcher_qmlplugin qindaqt_desktop_entry_parser_tests qindaqt_application_catalog_tests qindaqt_launcher_category_model_tests qindaqt_launcher_search_ranker_tests qindaqt_launcher_pinned_recent_tests qindaqt_launcher_presentation_tests qindaqt_launcher_scanner_tests qindaqt_launcher_execution_tests qindaqt_launcher_executor_tests qindaqt_launcher_persistence_tests qindaqt_launcher_controller_tests qindaqt_launcher_qml_tests qindaqt_launcher_installed_probe qindaqt_applet_manifest_tests qindaqt_applet_catalog_tests qindaqt_applet_instance_resolver_tests
[exit 0]

ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/launcher-l1/debug -R '^qindaqt\.launcher-' --output-on-failure --no-tests=error
[exit 0; 14/14 passed]

ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/launcher-l1/debug -R '^qindaqt\.applet-(manifest|catalog|runtime-resolution)$' --output-on-failure --no-tests=error
[exit 0; 3/3 passed]

QT_FATAL_WARNINGS=1 ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/launcher-l1/debug -R '^qindaqt\.launcher-offscreen$' --output-on-failure --no-tests=error
[exit 0; 1/1 passed]
```

### Release

```text
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/launcher-l1/release --parallel 3 --target qindaqt_shell_launcher qindaqt_shell_launcher_runtime qindaqt_shell_launcher_qml qindaqt_shell_launcher_qmlplugin qindaqt_desktop_entry_parser_tests qindaqt_application_catalog_tests qindaqt_launcher_category_model_tests qindaqt_launcher_search_ranker_tests qindaqt_launcher_pinned_recent_tests qindaqt_launcher_presentation_tests qindaqt_launcher_scanner_tests qindaqt_launcher_execution_tests qindaqt_launcher_executor_tests qindaqt_launcher_persistence_tests qindaqt_launcher_controller_tests qindaqt_launcher_qml_tests qindaqt_launcher_installed_probe qindaqt_applet_manifest_tests qindaqt_applet_catalog_tests qindaqt_applet_instance_resolver_tests
[exit 0]

ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/launcher-l1/release -R '^qindaqt\.launcher-' --output-on-failure --no-tests=error
[exit 0; 14/14 passed]

ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/launcher-l1/release -R '^qindaqt\.applet-(manifest|catalog|runtime-resolution)$' --output-on-failure --no-tests=error
[exit 0; 3/3 passed]

QT_FATAL_WARNINGS=1 ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/launcher-l1/release -R '^qindaqt\.launcher-offscreen$' --output-on-failure --no-tests=error
[exit 0; 1/1 passed]
```

### Static gates

```text
./tools/validate-docs
[exit 0; 117 Markdown documents plus mkdocs.yml navigation validated]

/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/launcher-l1/site
[exit 0; documentation built in 1.27 seconds]

./tools/check-source-shape
[exit 0; 1783 source files checked, 0 violations; only pre-existing advisory threshold warnings]

git diff --check
[exit 0; no output]

python3 -m json.tool data/applets/launcher.json > /dev/null
[exit 0]
```

During repair iteration, the new fatal-warning QML controls first exposed token-readiness, null-access, and transient section-rebuild faults and failed before each fix; only the final passing commands above are acceptance evidence. The scanner, uncertainty, action-policy, relocation, persistence-visibility, and keyboard/accessibility regressions are all registered rows that fail on the rejected `40f1372` behavior.

## Bounded caveats

- This candidate does not claim production panel dispatcher hosting; the audio/runtime composition lane owns that path.
- Startup-notification activation tokens and Settings1 schema registration for launcher keys remain later boundaries.
- No nested compositor/session, host D-Bus, hardware, uinput, network, real user application tree, or real application was exercised. The only real child-process fixtures are inert `/bin/true` and `/bin/false`.

## Requested next action

Kay McNulty: independently review exact product candidate `86a6d982325e2eaea90720713ac0d46dd86021b1`, including the registered descendants of each verdict reproduction. If accepted, hand it to the Program Manager for integration.

Requested workflow: independent exact review by Kay McNulty, then manager integration.
