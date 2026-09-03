# Kay McNulty — Launcher L1 repair-descendant recheck

- Persona: Kay McNulty, independent shell-applet reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Candidate SHA: `86a6d982325e2eaea90720713ac0d46dd86021b1`
- Tree SHA: `64f638f3fed318b70a3989e8bbd6283cbca1b681`
- Parent SHA: `5550a9137f8a50cc6326d345065509e8c43cd8c2`
- Base SHA: `ce9228d9694622d503d92a38d01986f8f124f188`
- Rejected product ancestor: `40f1372ef54d4c434626686095a18957ef3cb66f`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/launcher-l1-codex-review`
- Scratch/build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-launcher-codex`

The repair descendant is rejected. The functional regressions named in the first verdict are repaired and their new controls demonstrably fail against `40f1372`, but the registered non-QML runtime rows can attach to the host display/session environment, and the scanner still suppresses required degradation for inaccessible roots and dangling top-level `applications` links. The requested registered regression guards for the prior documentation/comment findings are also absent.

No product path was edited. Scratch source, fixtures, and binaries are under the assigned build root.

## Findings ledger

### P0

#### P0-1 — Four registered “isolated” runtime tests instantiate `QGuiApplication` without display/bus isolation

`src/shell/launcher/CMakeLists.txt:88-97` exposes Qt Quick/Gui transitively through the public runtime target. `tests/shell/launcher/CMakeLists.txt:88-125` links scanner, executor, persistence, and controller tests to that target and registers them with no environment; only the QML row at lines 157-163 forces the offscreen backend. Their sources end in `QTEST_MAIN` (`tst_application_scanner.cpp:254`, `tst_launch_executor.cpp:294`, `tst_launcher_persistence.cpp:320`, and `tst_launcher_controller.cpp:216`). With `QT_GUI_LIB` propagated, QtTest expands that macro to `QGuiApplication`, so these rows perform platform initialization despite being controller/adapter tests.

Exact-candidate evidence:

```text
$ ctest --test-dir .../debug --show-only=json-v1
qindaqt.launcher-application-scanner: LABELS, TIMEOUT, WORKING_DIRECTORY; no ENVIRONMENT
qindaqt.launcher-executor: LABELS, WORKING_DIRECTORY; no ENVIRONMENT
qindaqt.launcher-persistence: LABELS, WORKING_DIRECTORY; no ENVIRONMENT
qindaqt.launcher-controller: LABELS, WORKING_DIRECTORY; no ENVIRONMENT

$ ninja -C .../debug -t commands qindaqt_launcher_scanner_tests | rg 'tst_application_scanner.cpp.o'
... -DQT_GUI_LIB ... -DQT_QUICK_LIB ... tst_application_scanner.cpp ...

$ env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY \
    XDG_RUNTIME_DIR=.../repros/r2-no-host-runtime \
    WAYLAND_DISPLAY=qindaqt-nonexistent QT_QPA_PLATFORM=wayland \
    timeout 3s .../debug/tests/shell/launcher/qindaqt_launcher_scanner_tests \
    refusesAnEmptyRootList
Failed to create wl_display (No such file or directory)
This application failed to start because no Qt platform plugin could be initialized.
[exit 134, before the selected test entered]
```

The review host had `WAYLAND_DISPLAY`, `DISPLAY`, and `DBUS_SESSION_BUS_ADDRESS` set. During the required `40f1372` regression validation, the exact repaired scanner test over the ancestor implementation was initially invoked before this harness defect was identified; its timeout stack contained two `WaylandEventThr` threads and a `QDBusConnection` thread, with the main thread blocked on the intended FIFO reproduction. The exact candidate has the same unisolated registration and GUI-main construction. The initial ordinary exact-candidate selectors therefore inherited live host endpoints and are not admissible isolation evidence; after discovery I reran both complete selectors with display/bus variables removed and `QT_QPA_PLATFORM=offscreen`, and both passed 14/14.

Observed: nominally isolated adapter tests require and, under the registered ambient invocation, attach to the host GUI platform and may initialize a session-bus-backed platform theme. Expected: these rows construct `QCoreApplication` (`QTEST_GUILESS_MAIN`) or register an explicit offscreen and bus-poisoned environment. This violates the lane’s no-host-desktop/bus rule and `docs/wiki/development/testing-harness.md:198-199`, which claims that no launcher row contacts either.

### P1

None.

### P2

#### P2-1 — Inaccessible roots and dangling top-level application links are silently reported as normal absence

`src/shell/launcher/src/application_scanner.cpp:222-229` calls `QFileInfo::exists(<root>/applications)` before validating/canonicalizing the injected root. `exists()` is false both when the tree is absent and when traversal is denied or the path is a dangling symlink, so line 226 returns with no scanner diagnostic. This contradicts `docs/wiki/shell/launcher.md:114-128`, which requires dangling/non-regular links and unreadable roots to produce bounded degraded truth.

The QCore-based scratch probe was rebuilt against the exact candidate libraries:

```text
$ chmod 000 .../repros/r2-unreadable-root
$ .../repros/scanner_probe_r2 .../repros/r2-unreadable-root
entry-count=0 first=[] diagnostics=0
[exit 0; permissions restored afterward]

$ readlink .../repros/r2-dangling-applications-root/applications
missing-applications
$ .../repros/scanner_probe_r2 .../repros/r2-dangling-applications-root
entry-count=0 first=[] diagnostics=0
[exit 0]
```

Observed: both hostile/error states are indistinguishable from a genuinely absent applications tree. Expected: zero entries plus at least one scanner diagnostic, causing the controller’s degraded truth. The registered `confinesSymlinksAndRefusesNonRegularEntries` control covers an existing escaping directory link and an in-tree FIFO, but neither inverse above.

#### P2-2 — The required registered regressions for the prior documentation and comment findings do not exist

The repaired prose is now consistent, and both stale comments are corrected, but the recheck brief requires every prior reproduction—including P2-3 and both P3s—to be closed by a registered test that fails on `40f1372`. No test or tool references the affected launcher/applet-runtime/ADR prose or asserts the `/bin/true` + `/bin/false` and stored-list comment contracts:

```text
$ rg -n 'applet-runtime\.md|launcher\.md|0056-bound|only real process|duplicates collapse|truncated at' \
    tests/shell/launcher tests/applet_runtime tools
tests/shell/launcher/tst_launch_executor.cpp:263:    // AGENT-CONTRACT: The only real processes ...

$ ctest --test-dir .../debug --show-only=json-v1 | \
    rg 'validate-docs|check-source-shape|mkdocs|diff-check'
[no output; exit 1]
```

The prior `40f1372` verdict records that `validate-docs`, strict MkDocs, source shape, and diff checks all passed despite those contradictions. Observed: a future reintroduction of the exact P2-3/P3-1/P3-2 text defects would still pass every registered row and static gate. Expected: the mandated registered negative controls, or a narrower handoff claim and explicit disposition approved by the lane owner. This is a missing negative-control defect under the supplied acceptance contract.

### P3

None.

## Recheck of the prior findings

- P1-1 scanner escape/FIFO: repaired. `confinesSymlinksAndRefusesNonRegularEntries` passed on `86a6d98`; the same candidate test over `40f1372` timed out after 3 seconds in `QFile::open` on the FIFO (exit 124). P2-1 above is a separate root-error inverse.
- P1-2 uncertain convergence: repaired. `uncertainCommitsAreNeverReplayed` passed on the candidate; over `40f1372` it failed at the authoritative-empty/live-optimistic comparison (exit 1, 2 passed/1 failed).
- P1-3 action inheritance/inverse: repaired. Candidate parser tests passed 4/4 and executor tests passed 4/4 for the selected functions. Against `40f1372`, parser controls failed 2/2 and executor controls failed 2/2 with the former terminal/path replacement behavior.
- P1-4 warning-fatal applet and null fallback: repaired. The complete candidate QML binary passed 6/6 under `QT_FATAL_WARNINGS=1`, including null access. Against `40f1372`, the null selector aborted on the undefined-token QColor warning (exit 134).
- P1-5 persistence rendering and P2-2 keyboard/accessibility: repaired. Candidate QML selected functions passed; over `40f1372`, pinned/persistence rendering, denied state, and Tab traversal failed before the fatal null-fallback abort.
- P2-1 relocation: repaired. The candidate installed-package row moved the complete Launcher/Controls/Tokens stage and passed. With the repaired driver/probe over `40f1372`, it failed because the relocated stage lacked `QindaQt/Controls/qmldir` (CTest exit 8).
- P2-3 documentation: the launcher/app-runtime contradiction is manually corrected and all static documentation gates pass, but lacks the explicitly required registered regression (P2-2 above).
- P3-1 and P3-2: the `/bin/false` process-fixture wording and stored-list rejection comment are manually corrected, but lack the explicitly required registered regressions (P2-2 above).

## Ownership and registry review

The repair commit itself (`5550a913..86a6d98`) changes launcher implementation/QML/CMake, launcher tests, and the owning launcher/applet-runtime/testing-harness/ADR pages only. The `40f1372..86a6d98` range also contains the prior implementer handoff and worker record. No production shell composition path moved into this lane.

Across the original base (`ce9228d..86a6d98`), the shared edits are additive: one ADR index row, one MkDocs navigation row, and one appended `qindaqt.applets.launcher` built-in registry entry with its matching resolver expectation. No shared registry entry was removed or rewritten. There is no changed JSON in the candidate range; `data/applets/launcher.json` was nevertheless parsed successfully as an adjacent integrity check.

## Commands and results

### Identity and cleanliness

```text
$ git rev-parse HEAD
86a6d982325e2eaea90720713ac0d46dd86021b1
$ git rev-parse HEAD^{tree}
64f638f3fed318b70a3989e8bbd6283cbca1b681
$ git rev-parse HEAD^
5550a9137f8a50cc6326d345065509e8c43cd8c2
$ git status --porcelain
[no output; exit 0 before and after review]
```

### Configure and focused builds

Both exact required configure commands exited 0 (only the repository’s existing mixed-prefix runtime-search-path advisories):

```text
cmake -S . -B <ROOT>/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON

# Same command with -B <ROOT>/release -DCMAKE_BUILD_TYPE=Release
```

For each profile, this target build exited 0:

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
```

Ninja reported `premature end of file; recovering` for its pre-existing log, rebuilt the requested targets, and finished successfully in both profiles.

### Candidate selectors and focused reproductions

The required selectors passed in each profile:

```text
$ ctest --test-dir <ROOT>/debug -R '^qindaqt\.launcher-' --output-on-failure --no-tests=error
14/14 passed [exit 0]
$ ctest --test-dir <ROOT>/release -R '^qindaqt\.launcher-' --output-on-failure --no-tests=error
14/14 passed [exit 0]
$ ctest --test-dir <ROOT>/{debug,release} -R '^qindaqt\.applet-(manifest|catalog|runtime-resolution)$' --output-on-failure --no-tests=error
3/3 passed in each profile [exit 0 each]
$ QT_FATAL_WARNINGS=1 ctest --test-dir <ROOT>/{debug,release} -R '^qindaqt\.launcher-offscreen$' --output-on-failure --no-tests=error
1/1 passed in each profile [exit 0 each]
```

Because the first ordinary launcher selector inherited live endpoints (P0-1), I repeated both complete launcher selectors with `DBUS_SESSION_BUS_ADDRESS`, `WAYLAND_DISPLAY`, and `DISPLAY` removed, `XDG_RUNTIME_DIR` set to the scratch root, and `QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software`. Debug and Release again passed 14/14 (exit 0 each); those repeats are the admissible isolated results.

Selected current-candidate functions:

```text
qindaqt_launcher_scanner_tests confinesSymlinksAndRefusesNonRegularEntries
3 passed / 0 failed [exit 0]
qindaqt_launcher_persistence_tests uncertainCommitsAreNeverReplayed
3 passed / 0 failed [exit 0]
qindaqt_launcher_execution_tests parsesPrimaryAndActionExecutionKeys rejectsHostileExecutionGroups
4 passed / 0 failed [exit 0]
qindaqt_launcher_executor_tests desktopActionsInheritEntryPolicy desktopActionsIgnoreActionPolicyLookalikes
4 passed / 0 failed [exit 0]
QT_FATAL_WARNINGS=1 qindaqt_launcher_qml_tests rendersSectionsPersistenceAndAccessibleStates deniedRowsExposeAccessibleDisabledState supportsCompleteKeyboardTraversalAndActivation nullAccessShowsDisabledFallback
6 passed / 0 failed [exit 0]
ctest -R '^qindaqt\.launcher-installed-package$'
1/1 passed [exit 0]
```

### Proof that repaired functional controls fail on `40f1372`

I extracted `40f1372` with `git archive` into `<ROOT>/repros/r2-ancestor-testproof-86a6d98/source`, overlaid only candidate `tests/shell/launcher/**`, configured its standalone launcher suite, and built the six affected binaries. Configure and build exited 0. Results are recorded in the prior-finding section: scanner exit 124, persistence exit 1, execution exit 2, executor exit 2, QML exit 134, and installed-package CTest exit 8. No product worktree file was changed.

### Static gates

```text
$ ./tools/validate-docs
Validated 117 Markdown documents and mkdocs.yml navigation. [exit 0]
$ /home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir <ROOT>/site
Documentation built in 1.28 seconds. [exit 0]
$ ./tools/check-source-shape
Checked 1783 source files; 0 violations; two pre-existing decomposition advisories. [exit 0]
$ git diff --check
[no output; exit 0]
$ git diff --check ce9228d9694622d503d92a38d01986f8f124f188..86a6d982325e2eaea90720713ac0d46dd86021b1
[no output; exit 0]
$ python3 -m json.tool data/applets/launcher.json > /dev/null
[exit 0; adjacent check, because no JSON changed]
```

No nested compositor/session row, system D-Bus service, hardware/uinput path, network operation, or real application was invoked. The initial unisolated test invocation touched the live GUI/session environment as documented in P0-1; all subsequent executable checks were isolated.

## Verdict

VERDICT REJECT P0/P1/P2/P3=1/0/2/0
