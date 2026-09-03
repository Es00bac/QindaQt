# Kay McNulty — Launcher L1 exact-candidate review

- Persona: Kay McNulty, independent shell-applet reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Candidate SHA: `40f1372ef54d4c434626686095a18957ef3cb66f`
- Tree SHA: `ffc230c98fa94a4ef01441d6758f9723d13b625e`
- Parent SHA: `ce9228d9694622d503d92a38d01986f8f124f188`
- Base SHA: `ce9228d9694622d503d92a38d01986f8f124f188`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/launcher-l1-codex-review`
- Scratch/build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-launcher-codex`

The candidate is rejected. The ordinary focused rows pass, but five P1 contract failures and three P2 coverage/documentation defects remain. No product path was edited. All adversarial sources, fixtures, and binaries are under the assigned build root.

## Findings ledger

### P0

None.

### P1

#### P1-1 — The scanner escapes an injected root through its `applications` symlink and can block forever on a FIFO

`src/shell/launcher/src/application_scanner.cpp:96-157` accepts every non-directory name ending in `.desktop`, trusts a prior `QFileInfo::size()`, opens it as a generic `QIODevice`, and calls unbounded `readAll()`. `scanRoot()` at lines 177-186 constructs `<root>/applications` but does not reject that top-level directory when it is a symlink. The inner recursion's symlink check therefore does not confine the scan to the injected root. This violates the injected-root and hostile-input-bound contracts.

Reproduction 1 (the fixture's `symlink-root/applications` points to the sibling `outside-applications` directory):

```text
$ readlink -f /home/cabewse/work_SPaC3/builds/qindaqt/review-launcher-codex/repros/symlink-root/applications
/home/cabewse/work_SPaC3/builds/qindaqt/review-launcher-codex/repros/outside-applications
$ /home/cabewse/work_SPaC3/builds/qindaqt/review-launcher-codex/repros/scanner_probe /home/cabewse/work_SPaC3/builds/qindaqt/review-launcher-codex/repros/symlink-root
entry-count=1 first=[escaped] diagnostics=0
[exit 0]
```

Observed: a document outside the injected root is accepted with no diagnostic. Expected: the root symlink is refused/degraded and no out-of-root document is read.

Reproduction 2 (`fifo-root/applications/hang.desktop` is a FIFO):

```text
$ timeout 2s /home/cabewse/work_SPaC3/builds/qindaqt/review-launcher-codex/repros/scanner_probe /home/cabewse/work_SPaC3/builds/qindaqt/review-launcher-codex/repros/fifo-root
[exit 124; no output]
```

Observed: the synchronous scan blocks in `QFile::open`/`readAll`. Expected: non-regular filesystem nodes are refused with a bounded diagnostic. The positive ceiling control did work: a 4,200-file fixture returned `entry-count=4096 first=[app0] diagnostics=1` with exit 0.

#### P1-2 — An uncertain Settings1 commit cannot converge the optimistic pinned/recent model to an unchanged authoritative snapshot

`src/shell/launcher/src/launcher_persistence.cpp:141-150` clears the pending key on uncertainty and relies on the resync snapshot to converge. But `handleSnapshot()` at lines 97-113 decides whether to rebuild the live models by comparing the snapshot only with `m_confirmedPinned`/`m_confirmedRecent`. When the server still holds the old confirmed value, that comparison is equal even though the live model contains the optimistic mutation, so the mutation survives as false local truth.

The scratch probe drives an empty confirmed baseline, adds `new.app`, produces an uncertain commit, and supplies the authoritative empty resync snapshot:

```text
$ /home/cabewse/work_SPaC3/builds/qindaqt/review-launcher-codex/repros/persistence_uncertain
authoritative=[] live=[new.app]
[exit 1]
```

Observed: the live model still claims `new.app`. Expected: live pinned state becomes the authoritative empty list without replay. This violates ADR-0012's uncertainty/no-replay convergence contract and the launcher page's claim that resync restores authoritative truth.

#### P1-3 — Desktop actions discard entry-level `Terminal`/`Path` and honor forbidden action-group replacements

`src/shell/launcher/src/launch_execution.cpp:247-285` recognizes `Terminal`, `Path`, and `DBusActivatable` inside a `Desktop Action` group. `src/shell/launcher/src/launch_executor.cpp:50-52,70-99` then uses those action-group values instead of the entry-level `Terminal` and `Path`. The freedesktop [Desktop Entry actions contract](https://specifications.freedesktop.org/desktop-entry/latest/extra-actions.html) permits only `Name`, `Icon`, and `Exec` in an action group; launch policy such as `Terminal` and `Path` remains entry-level.

Reproduction with main entry `Terminal=true`, `Path=/expected-main-path`, and an action containing only `Exec=fixture --action`:

```text
$ /home/cabewse/work_SPaC3/builds/qindaqt/review-launcher-codex/repros/executor_probe /home/cabewse/work_SPaC3/builds/qindaqt/review-launcher-codex/repros/execution-root main-terminal run terminal
status=0 diagnostic= program=[fixture] args=[--action] cwd=[]
[exit 0]
```

Observed: it bypasses the injected terminal policy and loses the working directory. Expected: `program=[terminal-fixture] args=[--execute|fixture|--action] cwd=[/expected-main-path]`.

The inverse hostile control puts unsupported `Terminal=true` and `Path=../../action-outside` in the action group while the entry is non-terminal:

```text
$ /home/cabewse/work_SPaC3/builds/qindaqt/review-launcher-codex/repros/executor_probe /home/cabewse/work_SPaC3/builds/qindaqt/review-launcher-codex/repros/execution-root action hostile terminal
status=0 diagnostic= program=[terminal-fixture] args=[--execute|fixture|--action] cwd=[../../action-outside]
[exit 0]
```

Observed: unsupported action keys take control of the dispatch surface and cwd. Expected: those action-group keys are ignored/refused and the entry-level values govern.

The other hostile execution controls support the narrower no-shell-interpolation claim: `sh -c 'printf pwned'` arrived as `program=[sh] args=[-c|printf pwned]`; backticks and `$()` arrived as literal arguments; unsupported `%U` was refused; the fake D-Bus activator received `org.example.App`; no probe spawned an application.

#### P1-4 — The compiled applet fails the required `QT_FATAL_WARNINGS=1` gate in both profiles, and its null-access fallback dereferences null

`src/shell/launcher/qml/LauncherApplet.qml:82` assigns an undefined token to `QColor`; further undefined token assignments follow. `LauncherApplet.qml:153` unconditionally evaluates `root.access.query` even when `access` is null. The test at `tests/shell/launcher/tst_launcher_qml.cpp:176` declares the null-access surface valid but does not reject warnings.

Exact required gate:

```text
$ QT_FATAL_WARNINGS=1 ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-launcher-codex/debug -R '^qindaqt\.launcher-offscreen$' --output-on-failure --no-tests=error
QWARN ... LauncherApplet.qml:82:13: Unable to assign [undefined] to QColor
Received signal 6 (SIGABRT)
0% tests passed, 1 tests failed out of 1
[exit 8]

$ QT_FATAL_WARNINGS=1 ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-launcher-codex/release -R '^qindaqt\.launcher-offscreen$' --output-on-failure --no-tests=error
QWARN ... LauncherApplet.qml:82:13: Unable to assign [undefined] to QColor
Received signal 6 (SIGABRT)
0% tests passed, 1 tests failed out of 1
[exit 8]
```

Direct null-fallback selector without fatal warnings exits 0 but emits `LauncherApplet.qml:153: TypeError: Cannot read property 'query' of null`, plus multiple undefined-token warnings. Expected: compiled QML and the promised disabled fallback instantiate with no warnings or exceptions.

#### P1-5 — Settings persistence failure truth is exposed by C++ but never rendered by the applet

`src/shell/launcher/src/launcher_applet_controller.h:37,53` exposes `persistenceStatus`; `src/shell/launcher/src/launcher_applet_controller.cpp:164-168` supplies it. No QML file consumes the property. `src/shell/launcher/qml/LauncherApplet.qml:132-146` renders only scanner `diagnostic` and launch `feedback`.

```text
$ rg -n 'persistenceStatus' src/shell/launcher/qml src/shell/launcher/src/launcher_applet_controller.*
src/shell/launcher/src/launcher_applet_controller.h:37:  Q_PROPERTY(QString persistenceStatus READ persistenceStatus NOTIFY stateChanged)
src/shell/launcher/src/launcher_applet_controller.h:53:  [[nodiscard]] QString persistenceStatus() const;
src/shell/launcher/src/launcher_applet_controller.cpp:164:QString LauncherAppletController::persistenceStatus() const
[exit 0; no QML match]
```

Observed: malformed stored values, transport loss, conflicts, `UnknownKey`, and uncertain-save status cannot be visible in the compiled applet. Expected: the applet exposes the controller's persistence degradation/failure truth as required by ADR-0012 and `launcher.md`.

### P2

#### P2-1 — The installed-package test does not prove relocation

`tests/shell/launcher/run_installed_launcher.cmake:19-30` installs into `stage`; lines 55-68 execute the probe against that same path. The script never renames or moves the stage before execution and also supplies a build-tree QML fallback. Therefore the named installed-package row does not supply the relocation negative control required by this review lane.

I separately moved the Debug stage to `/home/cabewse/work_SPaC3/builds/qindaqt/review-launcher-codex/relocated-launcher` and ran the installed probe against the moved manifest, policy, and launcher QML. It exited 0 with `installed launcher probe passed`; the artifact currently relocates, although it emitted the P1-4 warnings. Expected automated coverage: move/rename the staged prefix, prove its original path no longer exists, and run from the relocated prefix so future absolute-path regressions fail the row.

#### P2-2 — The QML row does not cover most of the keyboard/accessibility contract it claims

`docs/wiki/shell/launcher.md:186-196,226` claims Tab, flat Up/Down across sections, Return/Space activation, Escape, categories, pinned/recent/search sections, and complete accessible states. `tests/shell/launcher/tst_launcher_qml.cpp:105-174` exercises summary Space, typing, one Down, one Return, and the accessible role/name/description of one search row. It has no controls for Tab, Up, Escape, row Space, cross-section traversal, category/pinned/recent rendering, denied/disabled states, or persistence failure truth. This is a material missing negative/control matrix for the only compiled-QML proof.

#### P2-3 — Normative documentation contradicts the candidate and itself

`docs/wiki/shell/applet-runtime.md:76-82` says launcher is the fourth registry entry and resolves `ready`; lines 92-95 say launcher remains an accepted contract that resolves `implementation-unavailable`. `docs/wiki/shell/launcher.md:74-80` likewise calls durable pinned/recent persistence a future Settings1 boundary, while its L1 section at lines 125 onward says that boundary landed. Expected: each normative page states one current contract matching the implementation and tests.

### P3

#### P3-1 — The process-starting test's safety comment and docs omit `/bin/false`

`tests/shell/launcher/tst_launch_executor.cpp:194-203` says the only real process started is `/bin/true`, but the test starts both `/bin/true` and `/bin/false`. The test remains host-safe and starts no real application, but the searchable `AGENT-CONTRACT` and the launcher test-matrix prose are inaccurate.

#### P3-2 — The stored-list helper contract comment describes behavior the function deliberately rejects

`src/shell/launcher/src/launcher_persistence.h:96-100` says duplicates collapse and over-limit lists truncate. `normalizeStoredIdList()` at `src/shell/launcher/src/launcher_persistence.cpp:22-48` rejects the entire list on either condition, matching the wiki and hostile-value policy. The private header comment is stale and hazardous to future maintenance.

## Required review questions

1. **Execution safety:** no launcher shell interpolation was observed; literal metacharacters stayed literal, unsupported field codes and overlong values were refused, the fake activator/spawner seams captured requests, and only `/bin/true` plus `/bin/false` were started by the production-spawner test. However, P1-3 breaks action `Terminal`/`Path` semantics.
2. **Scanning bounds:** the 4,096-file ceiling, huge/unreadable fixture behavior, deterministic ordering, watcher debounce, and generation-fencing focused tests pass. P1-1 defeats root confinement and time bounds with ordinary hostile filesystem nodes.
3. **Persistence:** Settings1 keys, validation bounds, confirmed-conflict rollback, serialization, and transport-loss tests pass. P1-2 defeats authoritative resync after uncertainty, and P1-5 hides persistence failure truth from the applet.
4. **Applet:** `launcher.json` requests only `applications.launch`; registry addition is a single additive line; manifest/catalog/resolver rows pass. P1-4 fails warning-clean instantiation, while P2-2 leaves much of the declared keyboard/accessibility behavior unproved.
5. **Boundaries/docs:** no `src/shell/runtime/**` or `src/shell/qml/**` path changed; the boundary-poison and source-shape rows pass; the manual moved-stage probe passes. P2-1 and P2-3 identify the remaining automated relocation and documentation defects.

## Commands and results

### Identity and cleanliness

```text
$ git rev-parse HEAD
40f1372ef54d4c434626686095a18957ef3cb66f
$ git rev-parse HEAD^{tree}
ffc230c98fa94a4ef01441d6758f9723d13b625e
$ git rev-parse HEAD^
ce9228d9694622d503d92a38d01986f8f124f188
$ git status --porcelain
[no output; exit 0 before and after review]
```

### Configure and focused builds

Both exact configure recipes exited 0:

```text
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-launcher-codex/debug -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-launcher-codex/release -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

For each profile, this focused/adjacent target build exited 0:

```text
cmake --build <profile-build-root> --parallel 3 --target qindaqt_shell_launcher qindaqt_shell_launcher_runtime qindaqt_shell_launcher_qml qindaqt_shell_launcher_qmlplugin qindaqt_desktop_entry_parser_tests qindaqt_application_catalog_tests qindaqt_launcher_category_model_tests qindaqt_launcher_search_ranker_tests qindaqt_launcher_pinned_recent_tests qindaqt_launcher_presentation_tests qindaqt_launcher_scanner_tests qindaqt_launcher_execution_tests qindaqt_launcher_executor_tests qindaqt_launcher_persistence_tests qindaqt_launcher_controller_tests qindaqt_launcher_qml_tests qindaqt_launcher_installed_probe qindaqt_applet_manifest_tests qindaqt_applet_catalog_tests qindaqt_applet_instance_resolver_tests
```

Ninja reported `premature end of file; recovering` for its log after an earlier interrupted/overlapping review invocation, rebuilt the affected targets, and completed with exit 0 in both profiles. No product file was involved.

### Focused selectors

```text
$ ctest --test-dir .../debug -R '^qindaqt\.launcher-' --output-on-failure --no-tests=error
100% tests passed, 0 failed out of 14 [exit 0]
$ ctest --test-dir .../release -R '^qindaqt\.launcher-' --output-on-failure --no-tests=error
100% tests passed, 0 failed out of 14 [exit 0]
$ ctest --test-dir .../debug -R '^qindaqt\.applet-(manifest|catalog|runtime-resolution)$' --output-on-failure --no-tests=error
100% tests passed, 0 failed out of 3 [exit 0]
$ ctest --test-dir .../release -R '^qindaqt\.applet-(manifest|catalog|runtime-resolution)$' --output-on-failure --no-tests=error
100% tests passed, 0 failed out of 3 [exit 0]
```

The additional required fatal-warning selectors failed 0/1 in each profile with exit 8, as reproduced in P1-4.

### Static gates

```text
$ ./tools/validate-docs
validated 117 documentation files [exit 0]
$ /home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-launcher-codex/site
Documentation built in 1.80 seconds [exit 0]
$ ./tools/check-source-shape
checked 1783 source files; 0 policy violation(s) [exit 0; only existing advisory large-file warnings]
$ git diff --check
[no output; exit 0]
$ git diff --check HEAD^ HEAD
[no output; exit 0]
$ python3 -m json.tool data/applets/launcher.json
[formatted JSON; exit 0]
```

No nested compositor/session row, host D-Bus service, hardware/uinput path, network operation, or real application was invoked.

## Verdict

VERDICT REJECT P0/P1/P2/P3=0/5/3/2
