# Winifred Asprey — Launcher L1 inaccessible-ancestor repair handoff

- Candidate commit: `26f366a4a2ab14fc341407f015d21685b8c8415f`
- Candidate tree: `a7be82dee8349102e59d7f153338d5b82583a9da`
- Exact rejected repair base: `352cfd03db385fb497998e80ea2ad028bd744979`
- Candidate parent: `46828b1f2de98a565a60a3275f6f5ca104d2fa2f` (coordination-only handoff descendant of the rejected base)
- Branch: `worker/launcher-l1`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/launcher-l1`
- Requested next action: Kay McNulty independent exact review of `26f366a4a2ab14fc341407f015d21685b8c8415f`, then Program Manager integration.

## Outcome

`ApplicationScanner` now probes injected data roots and their `applications/`
trees with `lstat`. Only syscall-confirmed `ENOENT` is normal absence; denied
ancestor traversal and every other indeterminate status publish a fixed bounded
scanner diagnostic, which the controller projects as `degraded`. Final-component
symlinks remain visible to the existing dangling/canonical-containment checks.

The registered scanner and controller fixtures construct
`blocked/data-root/applications`, remove all permissions from `blocked`, assert
the bounded diagnostic and visible degraded phase, and restore permissions via
`qScopeGuard` on every return path. They skip only for effective UID 0. The
required runs used effective UID 1000 and did not skip. A paired positive
control proves a genuinely missing root and missing `applications/` tree remain
normal.

## Changed product paths

- `docs/wiki/adr/0056-bound-launcher-execution-behind-injected-seams.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/shell/launcher.md`
- `src/shell/launcher/src/application_scanner.cpp`
- `src/shell/launcher/src/application_scanner.h`
- `tests/shell/launcher/check_launcher_contract_text.cmake`
- `tests/shell/launcher/tst_application_scanner.cpp`
- `tests/shell/launcher/tst_launcher_controller.cpp`

## Verification evidence

Identity and scope:

```text
git show --format='%H%n%T%n%P%n%s' --no-patch 26f366a4a2ab14fc341407f015d21685b8c8415f
[exit 0: candidate, tree, and parent exactly match the values above]

git diff-tree --no-commit-id --name-only -r 26f366a4a2ab14fc341407f015d21685b8c8415f | sort
[exit 0: exactly the eight sorted product paths above]
```

Both mandated configure commands were run exactly with `<ROOT>` equal to
`/home/cabewse/work_SPaC3/builds/qindaqt/launcher-l1`:

```text
cmake -S . -B <ROOT>/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
[exit 0]

cmake -S . -B <ROOT>/release -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
[exit 0]
```

The configure output contained only the repository's existing mixed-prefix
runtime-search-path warnings. For each profile the exact candidate was built
with:

```text
cmake --build <ROOT>/<profile> --parallel 3 --target \
  qindaqt_shell_launcher qindaqt_shell_launcher_runtime \
  qindaqt_shell_launcher_qml qindaqt_shell_launcher_qmlplugin \
  qindaqt_desktop_entry_parser_tests qindaqt_application_catalog_tests \
  qindaqt_launcher_category_model_tests qindaqt_launcher_search_ranker_tests \
  qindaqt_launcher_pinned_recent_tests qindaqt_launcher_presentation_tests \
  qindaqt_launcher_scanner_tests qindaqt_launcher_execution_tests \
  qindaqt_launcher_executor_tests qindaqt_launcher_persistence_tests \
  qindaqt_launcher_controller_tests qindaqt_launcher_qml_tests \
  qindaqt_launcher_installed_probe qindaqt_applet_manifest_tests \
  qindaqt_applet_catalog_tests qindaqt_applet_instance_resolver_tests
[Debug exit 0; Release exit 0]
```

Final exact-candidate hostile-environment selectors:

```text
env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY \
  XDG_RUNTIME_DIR=<ROOT>/isolated-<profile> \
  WAYLAND_DISPLAY=qindaqt-nonexistent QT_QPA_PLATFORM=wayland \
  ctest --test-dir <ROOT>/<profile> -R '^qindaqt\.launcher-' \
  --output-on-failure --no-tests=error
[Debug exit 0, 15/15 passed; Release exit 0, 15/15 passed]

env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY \
  XDG_RUNTIME_DIR=<ROOT>/isolated-<profile> \
  WAYLAND_DISPLAY=qindaqt-nonexistent QT_QPA_PLATFORM=wayland \
  ctest --test-dir <ROOT>/<profile> \
  -R '^qindaqt\.applet-(manifest|catalog|runtime-resolution)$' \
  --output-on-failure --no-tests=error
[Debug exit 0, 3/3 passed; Release exit 0, 3/3 passed]
```

Focused fixtures were also invoked directly before the full matrix:

```text
<ROOT>/debug/tests/shell/launcher/qindaqt_launcher_scanner_tests \
  inaccessibleAncestorDegrades
[exit 0, 3 passed / 0 failed / 0 skipped]

<ROOT>/debug/tests/shell/launcher/qindaqt_launcher_controller_tests \
  inaccessibleAncestorReachesTheProjection
[exit 0, 3 passed / 0 failed / 0 skipped]
```

Mutation sensitivity against exact rejected `352cfd0`:

```text
cmake -DSOURCE_ROOT=<ROOT>/launcher-r3-mutation.xGka9q \
  -P "$PWD/tests/shell/launcher/check_launcher_contract_text.cmake"
[expected exit 1: missing ENOENT discriminator/test/wiki truth and retained
 ambiguous QFileInfo discriminator]

cmake -S <ROOT>/launcher-r3-mutation.xGka9q/tests/shell/launcher \
  -B <ROOT>/mutation-build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
[exit 0]

cmake --build <ROOT>/mutation-build --parallel 3 \
  --target qindaqt_launcher_scanner_tests
[exit 0, 46/46 actions]

env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY \
  XDG_RUNTIME_DIR=<ROOT>/mutation-runtime \
  WAYLAND_DISPLAY=qindaqt-nonexistent QT_QPA_PLATFORM=wayland \
  <ROOT>/mutation-build/qindaqt_launcher_scanner_tests \
  inaccessibleAncestorDegrades
[expected exit 1: 2 passed / 1 failed / 0 skipped; rejected scanner published
 0 diagnostics where the fixture requires 1]
```

Final static gates:

```text
./tools/validate-docs
[exit 0: 117 Markdown documents and mkdocs.yml navigation]

/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir <ROOT>/site
[exit 0]

./tools/check-source-shape
[exit 0: 1,784 source files checked, 0 allowlisted; two unrelated existing
 decomposition-review warnings]

git diff --check
[exit 0]
```

No JSON changed, so no `python3 -m json.tool` invocation applied. An optional
whole-file `clang-format --dry-run --Werror` exited 1 because these launcher
files already use the repository's established formatting rather than the
installed formatter's defaults; it made no changes and is not a required gate.

## Bounded caveats and non-claims

- The permission-denial rows skip only as effective UID 0, where the fixture
  cannot reproduce traversal denial. The reported runs were non-root and had
  zero skips.
- This candidate does not claim nested compositor/session, physical hardware,
  uinput, network, host D-Bus, or real-application evidence. None was invoked.
- No launcher execution, persistence, panel-hosting, or public module boundary
  changed; this repair is confined to injected-root filesystem truth.
- The coordination claim/midpoint/handoff records are committed after the
  immutable product candidate and do not alter its tree.

Requested next action: **Kay McNulty independent exact review of
`26f366a4a2ab14fc341407f015d21685b8c8415f`, then Program Manager integration.**
