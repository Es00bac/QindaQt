# Kay McNulty — Launcher L1 third-repair descendant recheck

- Persona: Kay McNulty, independent shell-applet reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Candidate SHA: `26f366a4a2ab14fc341407f015d21685b8c8415f`
- Tree SHA: `a7be82dee8349102e59d7f153338d5b82583a9da`
- Parent SHA: `46828b1f2de98a565a60a3275f6f5ca104d2fa2f`
- Base SHA: `ce9228d9694622d503d92a38d01986f8f124f188`
- Repaired product ancestor: `352cfd03db385fb497998e80ea2ad028bd744979`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/launcher-l1-codex-review`
- Scratch/build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-launcher-codex`

The candidate is accepted with no P0-P3 findings. The descendant closes the
remaining inaccessible-ancestor defect: a real injected data root behind an
`EACCES` ancestor now produces bounded scanner degradation, while confirmed
`ENOENT` remains normal absence. The registered scanner and controller
regressions are non-vacuous under this reviewer's effective UID 1000, restore
permissions through `qScopeGuard`, and skip only when `geteuid() == 0`.

No product path was edited. All scratch artifacts remained under the assigned
build root, and the product worktree was clean before and after review.

## Findings ledger

### P0

None.

### P1

None.

### P2

None.

### P3

None.

## Recheck results

1. **P0-1 remains closed.** `qindaqt_shell_launcher_runtime` publicly links
   only the pure launcher, Settings1 client, Qt Core, and Qt DBus. Its four
   adapter/controller consumers compile with only `QT_CORE_LIB` and
   `QT_DBUS_LIB`; `ldd` finds Qt Core/DBus and no Qt Gui/QML/Quick. Every one
   of the 15 registered launcher rows unsets inherited session-bus and display
   endpoints and uses the private runtime directory. All non-QML C++ launcher
   tests use `QTEST_GUILESS_MAIN`; the actual QML row forces offscreen software
   rendering.
2. **The earlier inaccessible-root and dangling-link findings remain closed.**
   The candidate's scanner focused row covers the unreadable root, dangling
   top-level `applications` link, inaccessible ancestor, and paired confirmed
   absence cases. Debug and Release both pass these selectors with zero skips.
3. **The third-repair focus is closed.** `pathPresence()` at
   `src/shell/launcher/src/application_scanner.cpp:30-43` calls `lstat` and
   classifies only `errno == ENOENT` as `Missing`; all other lookup failures
   are `Indeterminate`. `scanRoot()` at lines 237-276 records a bounded
   diagnostic for indeterminate root or applications-tree status. My exact
   prior blocked-ancestor shape now reports one diagnostic where `352cfd0`
   reported zero. The controller publishes `degraded` in the registered
   projection regression.
4. **The regressions clean up and skip truthfully.** The scanner fixture at
   `tests/shell/launcher/tst_application_scanner.cpp:196-219` and controller
   fixture at `tests/shell/launcher/tst_launcher_controller.cpp:223-245`
   check `geteuid() == 0` before creating the denial. After `chmod(0000)`, each
   immediately installs a `qScopeGuard` that restores mode `0700`, so ordinary
   QtTest assertion returns also restore traversal. This review ran as UID
   1000 and both profiles reported zero skips; the independent scratch fixture
   likewise ended with the blocked directory restored to mode 700.
5. **Earlier closures and scope remain intact.** The repair commit changes
   exactly its scanner implementation/header, two focused tests, contract-text
   guard, and three owning documentation pages. Its parent is the prior
   coordination-only handoff descendant. No execution, persistence, QML,
   public runtime-link, registry, JSON, or production-panel path changed in
   this repair. Across the lane base, the first-party registry edit remains the
   single additive launcher id.

## Commands and results

### Identity, ancestry, and cleanliness

```text
$ git rev-parse HEAD
26f366a4a2ab14fc341407f015d21685b8c8415f
$ git rev-parse HEAD^{tree}
a7be82dee8349102e59d7f153338d5b82583a9da
$ git rev-parse HEAD^
46828b1f2de98a565a60a3275f6f5ca104d2fa2f
$ git merge-base HEAD main
ce9228d9694622d503d92a38d01986f8f124f188
$ git status --porcelain
[no output; exit 0 before review and after verdict write]
```

`git diff-tree --no-commit-id --name-only -r HEAD | sort` exited 0 and listed
exactly these eight product paths:

```text
docs/wiki/adr/0056-bound-launcher-execution-behind-injected-seams.md
docs/wiki/development/testing-harness.md
docs/wiki/shell/launcher.md
src/shell/launcher/src/application_scanner.cpp
src/shell/launcher/src/application_scanner.h
tests/shell/launcher/check_launcher_contract_text.cmake
tests/shell/launcher/tst_application_scanner.cpp
tests/shell/launcher/tst_launcher_controller.cpp
```

### Configure and focused builds

Both required configure commands exited 0. They emitted only the repository's
known mixed-prefix runtime-search-path warnings:

```text
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-launcher-codex/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
[exit 0]

cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-launcher-codex/release -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
[exit 0]
```

For each profile I built this exact target set with `--parallel 3`:

```text
qindaqt_shell_launcher qindaqt_shell_launcher_runtime
qindaqt_shell_launcher_qml qindaqt_shell_launcher_qmlplugin
qindaqt_desktop_entry_parser_tests qindaqt_application_catalog_tests
qindaqt_launcher_category_model_tests qindaqt_launcher_search_ranker_tests
qindaqt_launcher_pinned_recent_tests qindaqt_launcher_presentation_tests
qindaqt_launcher_scanner_tests qindaqt_launcher_execution_tests
qindaqt_launcher_executor_tests qindaqt_launcher_persistence_tests
qindaqt_launcher_controller_tests qindaqt_launcher_qml_tests
qindaqt_launcher_installed_probe qindaqt_applet_manifest_tests
qindaqt_applet_catalog_tests qindaqt_applet_instance_resolver_tests
```

Debug completed 127/127 final actions and Release completed 131/131, both exit
0. Ninja reported `premature end of file; recovering` for the reused build
roots' logs, rebuilt the requested artifacts, and completed successfully.

### Isolated full selectors

Each selector used the matching private `r4-isolated-<profile>` directory:

```text
env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY \
  XDG_RUNTIME_DIR=<ROOT>/repros/r4-isolated-<profile> \
  WAYLAND_DISPLAY=qindaqt-nonexistent QT_QPA_PLATFORM=wayland \
  ctest --test-dir <ROOT>/<profile> -R '^qindaqt\.launcher-' \
  --output-on-failure --no-tests=error
[Debug: exit 0, 15/15 passed]
[Release: exit 0, 15/15 passed]

env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY \
  XDG_RUNTIME_DIR=<ROOT>/repros/r4-isolated-<profile> \
  WAYLAND_DISPLAY=qindaqt-nonexistent QT_QPA_PLATFORM=wayland \
  ctest --test-dir <ROOT>/<profile> \
  -R '^qindaqt\.applet-(manifest|catalog|runtime-resolution)$' \
  --output-on-failure --no-tests=error
[Debug: exit 0, 3/3 passed]
[Release: exit 0, 3/3 passed]
```

`ctest --test-dir <ROOT>/debug -N -V -R '^qindaqt\.launcher-'` exited 0
and enumerated 15 rows. Every row unsets `DBUS_SESSION_BUS_ADDRESS`, `DISPLAY`,
and `WAYLAND_DISPLAY` and sets the isolated runtime; the QML row additionally
sets `QT_QPA_PLATFORM=offscreen` and `QT_QUICK_BACKEND=software`.

### Permission-denied ancestor and adjacent controls

I rebuilt the prior `scanner_probe.cpp` as a `QCoreApplication` named
`scanner_probe_r4`, linking the freshly built candidate Debug runtime and pure
archives. The scratch fixture created
`r4-inaccessible-ancestor/blocked/data-root/applications`, removed traversal
from `blocked`, and installed a shell cleanup trap before invoking the probe:

```text
$ stat <ROOT>/repros/r4-inaccessible-ancestor/blocked/data-root
stat: cannot statx '.../blocked/data-root': Permission denied
$ env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY \
    XDG_RUNTIME_DIR=<ROOT>/repros/r4-isolated-debug \
    WAYLAND_DISPLAY=qindaqt-nonexistent QT_QPA_PLATFORM=wayland \
    <ROOT>/repros/scanner_probe_r4 \
    <ROOT>/repros/r4-inaccessible-ancestor/blocked/data-root
entry-count=0 first=[] diagnostics=1
[combined compile/fixture/probe/restore command: exit 0]
$ find <ROOT>/repros/r4-inaccessible-ancestor -maxdepth 2 -printf '%m %p\n'
700 .../blocked
```

The registered repairs and paired controls were then run directly in each
profile under the same isolated environment:

```text
<ROOT>/<profile>/tests/shell/launcher/qindaqt_launcher_scanner_tests \
  inaccessibleAncestorDegrades confirmedMissingPathsAreNormalNotDegraded \
  inaccessibleAndDanglingApplicationTreesDegrade
[Debug: exit 0, 5 passed / 0 failed / 0 skipped]
[Release: exit 0, 5 passed / 0 failed / 0 skipped]

<ROOT>/<profile>/tests/shell/launcher/qindaqt_launcher_controller_tests \
  inaccessibleAncestorReachesTheProjection rootAccessFailuresReachTheProjection
[Debug: exit 0, 4 passed / 0 failed / 0 skipped]
[Release: exit 0, 4 passed / 0 failed / 0 skipped]
```

The totals include each binary's init and cleanup cases. `id -u` returned
`1000`, confirming the denial rows executed rather than taking their root-only
skip.

### Runtime isolation and dependency audit

```text
$ rg -n 'QTEST_(GUILESS_)?MAIN' tests/shell/launcher/tst_*.cpp
[exit 0: 11 non-QML rows use QTEST_GUILESS_MAIN; only tst_launcher_qml.cpp
 uses QTEST_MAIN]

$ for target in qindaqt_launcher_scanner_tests qindaqt_launcher_executor_tests \
    qindaqt_launcher_persistence_tests qindaqt_launcher_controller_tests; do \
    ninja -C <ROOT>/debug -t commands "$target" | \
      rg -m1 -o -- '-DQT_[A-Z0-9_]+_LIB' | sort -u; done
[exit 0: every target reports only QT_CORE_LIB and QT_DBUS_LIB]

$ for binary in qindaqt_launcher_scanner_tests qindaqt_launcher_executor_tests \
    qindaqt_launcher_persistence_tests qindaqt_launcher_controller_tests; do \
    ldd "<ROOT>/debug/tests/shell/launcher/$binary" | \
      rg 'libQt6(Core|DBus|Gui|Qml|Quick)'; done
[exit 0: every binary matches only libQt6Core and libQt6DBus]
```

### Static gates

```text
$ ./tools/validate-docs
Validated 117 Markdown documents and mkdocs.yml navigation. [exit 0]

$ /home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
    --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-launcher-codex/site
Documentation built in 1.29 seconds. [exit 0]

$ ./tools/check-source-shape
Checked 1784 source files; skipped 0 allowlisted files. [exit 0]
[Two unrelated existing decomposition-review warnings at 500 and 539 lines.]

$ git diff --check
[no output; exit 0]
$ git diff --check ce9228d9694622d503d92a38d01986f8f124f188..HEAD
[no output; exit 0]
$ git diff --name-only ce9228d9694622d503d92a38d01986f8f124f188..HEAD | rg '\.json$'
[no output; no changed JSON, so no python3 -m json.tool invocation applied]
```

No nested compositor/session row, host D-Bus service, hardware/uinput path,
network operation, or real user application was invoked.

## Verdict

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/0
