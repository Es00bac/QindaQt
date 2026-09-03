# Winifred Asprey — Launcher L1 host-isolation repair handoff

- Exact product candidate: `352cfd03db385fb497998e80ea2ad028bd744979`
- Candidate tree: `004fc8775bd094846bd26a8546e5bf31710e7342`
- Exact repair base (rejected product candidate): `86a6d982325e2eaea90720713ac0d46dd86021b1`
- Candidate parent (prior coordination commit): `9f45c5f9bd650ad1f693a1962f4992882c4272cf`
- Original lane base: `ce9228d9694622d503d92a38d01986f8f124f188`
- Branch: `worker/launcher-l1`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/launcher-l1`

## Outcome

All findings in Kay McNulty's recheck of `86a6d98` are closed:

- the public Launcher L1 runtime no longer links Qt Gui/QML/Quick, and scanner, executor, persistence, and controller tests explicitly construct `QCoreApplication`;
- all launcher registrations strip inherited `DBUS_SESSION_BUS_ADDRESS`, `DISPLAY`, and `WAYLAND_DISPLAY`, use a private `XDG_RUNTIME_DIR`, and the two real GUI rows force offscreen software rendering;
- existing inaccessible data roots and dangling top-level `applications` links publish bounded scanner diagnostics and controller `degraded` truth, while a readable root with no applications tree remains normal absence;
- the registered runtime boundary rejects the old GUI linkage/mains, and a new registered contract-text row enforces the previously corrected applet-runtime, launcher/ADR process-fixture, test safety-comment, and stored-list helper contracts.

## Changed paths

- `docs/wiki/development/testing-harness.md`
- `docs/wiki/shell/launcher.md`
- `src/shell/launcher/CMakeLists.txt`
- `src/shell/launcher/src/application_scanner.cpp`
- `tests/shell/launcher/CMakeLists.txt`
- `tests/shell/launcher/check_launcher_boundary.cmake`
- `tests/shell/launcher/check_launcher_contract_text.cmake`
- `tests/shell/launcher/run_installed_launcher.cmake`
- `tests/shell/launcher/tst_application_scanner.cpp`
- `tests/shell/launcher/tst_launch_executor.cpp`
- `tests/shell/launcher/tst_launcher_controller.cpp`
- `tests/shell/launcher/tst_launcher_persistence.cpp`

## Verification evidence

All acceptance commands ran from the lane worktree. Both exact required configure commands exited 0; their existing mixed-prefix runtime-search-path advisories were non-fatal and outside this lane.

### Configure

```text
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/launcher-l1/debug -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
[exit 0]

cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/launcher-l1/release -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
[exit 0]
```

### Focused builds

The following command ran once per profile, with `<profile>` equal to `debug` and `release`:

```text
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/launcher-l1/<profile> --parallel 3 --target qindaqt_shell_launcher qindaqt_shell_launcher_runtime qindaqt_shell_launcher_qml qindaqt_shell_launcher_qmlplugin qindaqt_desktop_entry_parser_tests qindaqt_application_catalog_tests qindaqt_launcher_category_model_tests qindaqt_launcher_search_ranker_tests qindaqt_launcher_pinned_recent_tests qindaqt_launcher_presentation_tests qindaqt_launcher_scanner_tests qindaqt_launcher_execution_tests qindaqt_launcher_executor_tests qindaqt_launcher_persistence_tests qindaqt_launcher_controller_tests qindaqt_launcher_qml_tests qindaqt_launcher_installed_probe qindaqt_applet_manifest_tests qindaqt_applet_catalog_tests qindaqt_applet_instance_resolver_tests
[exit 0 in each profile; 62 Ninja actions reported in each]
```

### Hostile-environment selectors

The launcher and applet selectors ran in each profile under `env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY`, private profile-specific `XDG_RUNTIME_DIR`, `WAYLAND_DISPLAY=qindaqt-nonexistent`, and `QT_QPA_PLATFORM=wayland`. Registered launcher properties additionally unset display/bus endpoints and override the two GUI rows to offscreen software rendering with fatal warnings.

```text
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/launcher-l1/debug -R '^qindaqt\.launcher-' --output-on-failure --no-tests=error
[exit 0; 15/15 passed]

ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/launcher-l1/debug -R '^qindaqt\.applet-(manifest|catalog|runtime-resolution)$' --output-on-failure --no-tests=error
[exit 0; 3/3 passed]

ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/launcher-l1/release -R '^qindaqt\.launcher-' --output-on-failure --no-tests=error
[exit 0; 15/15 passed]

ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/launcher-l1/release -R '^qindaqt\.applet-(manifest|catalog|runtime-resolution)$' --output-on-failure --no-tests=error
[exit 0; 3/3 passed]
```

Direct selected probes under the same hostile environment also passed: Debug scanner 1/1, and Debug controller `rootAccessFailuresReachTheProjection` 3/3 QtTest functions including init/cleanup.

### Mutation sensitivity and binary audit

```text
cmake -DSOURCE_ROOT="$PWD" -DPOISON_ROOT=<build-root>/repros/current-boundary-poison -P tests/shell/launcher/check_launcher_boundary.cmake
[exit 0]

cmake -DSOURCE_ROOT=<extracted-86a6d98> -P "$PWD/tests/shell/launcher/check_launcher_boundary.cmake"
[expected exit 1; rejected Qt6::Qml/Qt6::Quick runtime links and all four non-guiless adapter mains]

ctest --test-dir <extracted-86a6d98-with-current-scanner-test>/build -R '^qindaqt\.launcher-application-scanner$' --output-on-failure --no-tests=error
[expected exit 8; row failed, 12 QtTest functions passed / 1 failed because diagnostics were 0 instead of 2]

cmake -DSOURCE_ROOT="$PWD" -P tests/shell/launcher/check_launcher_contract_text.cmake
[exit 0]

cmake -DSOURCE_ROOT=<extracted-40f1372> -P "$PWD/tests/shell/launcher/check_launcher_contract_text.cmake"
[expected exit 1; nine required/misleading text checks rejected]
```

`ninja -t commands` and `ldd` audits over all four Debug headless binaries exited 0: no Qt Gui/QML/Quick compile definitions or dynamic dependencies were present.

### Static gates

```text
./tools/validate-docs
[exit 0; 117 Markdown documents and mkdocs.yml navigation validated]

/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/launcher-l1/site
[exit 0; documentation built in 1.29 seconds]

./tools/check-source-shape
[exit 0; 1784 source files checked, 0 violations; two pre-existing decomposition advisories]

git diff --check
[exit 0; no output]

python3 -m json.tool data/applets/launcher.json > /dev/null
[exit 0; adjacent unchanged manifest integrity check]
```

## Bounded caveats

- This candidate does not claim production panel dispatcher hosting, startup-notification activation tokens, or Settings1 schema registration for launcher keys.
- No nested compositor/session row, host D-Bus service, host display, hardware, uinput, network, real user application tree, or real application was exercised. The only real child-process fixtures are inert `/bin/true` and `/bin/false`.
- The applet-integrity rows are guiless and passed under the hostile caller environment; this lane did not alter their registrations because those shared paths were outside the repair.

## Requested next action

Kay McNulty: independently recheck exact product candidate `352cfd03db385fb497998e80ea2ad028bd744979`, including the registered mutation controls against `86a6d98` and `40f1372`. If accepted, send it to the Program Manager for integration.

Requested workflow: independent exact review by Kay McNulty, then manager integration.
