# Dina St Johnston — Terminal S1 repair-descendant recheck

- Persona: Dina St Johnston (`dina-st-johnston`), independent first-party application reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Candidate SHA: `34439e80b6e66eaf47561a3234b99433d8cf4bcb`
- Tree SHA: `fb72ee0c23b5ac5d50450e8160b346ed7ba241e9`
- Parent SHA: `436461c5b2f9726abd747339f1c15e204693e814`
- Base SHA: `4c23978886689e06dde08805483b1495942dc017`
- Repaired ancestor: `08481f496438ec112753fef30a797db5a19af654`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/terminal-s1-codex-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-codex`

## Findings ledger

### P0

None.

### P1

None. Both prior P1 product defects are repaired: teardown no longer publishes clean while the captured process group remains nonempty or uncertain, and production presentation consumes the asynchronous Settings1 ledger.

### P2

#### P2-R2-1 — The required dialog non-closure proof is still a missing negative control

The four fake-transport outcome rows instantiate `TerminalWindow` and inspect only its main status label (`tests/apps/terminal/tst_terminal_profile_settings.cpp:330-399`). They never open `TerminalWindow::manageProfiles()` or instantiate `TerminalProfileDialog`. The separate dialog test constructs the dialog without `show()`, `open()`, or `exec()`, directly invokes `accept()`, and compares `dialog.result()` with `QDialog::Rejected` before and after `finishApply(false, ...)` (`tests/apps/terminal/tst_terminal_profiles.cpp:193-223`). A newly constructed dialog's result is already `Rejected`, and closing it through `reject()` also leaves that same value, so those assertions cannot distinguish “the dialog remained open” from “the dialog silently closed.” The enabled-child assertion likewise does not establish visibility or a live modal loop.

Reproduction:

```sh
rg -n 'TerminalProfileDialog|productionWindowPresentsAsynchronousOutcome|dialog\.(show|open|exec)|dialog\.result|isVisible\(|isHidden\(' \
  tests/apps/terminal/tst_terminal_profile_settings.cpp \
  tests/apps/terminal/tst_terminal_profiles.cpp
```

Observed (exit 0): the only `TerminalProfileDialog` construction is the unshown unit at line 197; the only result checks are `Rejected` at lines 214 and 220; there is no dialog `show`, `open`, `exec`, or `isVisible` assertion. The fake-transport data rows occur only in `productionWindowPresentsAsynchronousOutcome`, which inspects `qindaqtTerminalStatus` on the main window.

The focused execution confirms that the four rows exist but does not add dialog coverage:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS \
  QT_QPA_PLATFORM=offscreen \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-codex/debug/tests/apps/terminal/qindaqt_terminal_profile_settings_tests \
  productionWindowPresentsAsynchronousOutcome -v1
```

Observed: exit 0; 6/6 QtTest cases passed (init, conflict, confirmed rejection, transport failure, owner loss, cleanup), all against the main-window status surface. Expected: for each required asynchronous failure class, drive the production Manage Profiles path with the fake transport/client, assert the dialog is still visible and unfinished after the outcome, and assert its accessible per-key result. This is a bounded missing negative control; source inspection indicates the current failure branch is likely correct, but the explicit review contract requires executable proof.

### P3

None.

## Recheck of the four prior findings

1. **Prior P1-1 is closed in product behavior.** `TerminalSession` retains a distinct captured process-group id after leader reap, refuses replacement while it is owned, sends HUP through backend close then TERM and KILL to that retained group, and releases ownership only after `ProcessGroupState::Empty`. `PosixProcessMonitor` combines `killpg(pgid, 0)` with a fail-closed Linux `/proc` scan that sees zombie group members. The original short-grace reproduction observed a remaining member but now reported `clean=false`, `ShutdownFailed`, and retained ownership instead of the former false clean completion. The registered hostile fixture passed the selector and ten consecutive extra runs, and its RAII cleanup sends SIGKILL on every failing return path.
2. **Prior P1-2 is closed in product behavior, with P2-R2-1 remaining in its proof.** The production window and dialog both consume `applyFinished`; conflict, confirmed rejection, transport loss/owner loss uncertainty, per-key not-attempted results, and no replay are rendered in bounded visible/accessibility text. The dialog closes only on `allApplied` in the implementation. The required integrated dialog non-closure test is absent as described above.
3. **Prior P2-1 is closed.** The freshly rebuilt empty-argv reproducer preserved both elements exactly (`before_count=2`, `after_count=2`, empty first argument) and exited 0. The registered profile row additionally covers leading/interior/trailing empty elements.
4. **Prior P3-1 is closed.** `docs/wiki/index.md` now calls Terminal multi-session and states the eight-tab bound.

## Commands and results

### Identity, ancestry, scope, and cleanliness

```sh
git rev-parse HEAD
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git rev-parse 4c23978886689e06dde08805483b1495942dc017
git merge-base 4c23978886689e06dde08805483b1495942dc017 HEAD
git merge-base --is-ancestor 08481f496438ec112753fef30a797db5a19af654 HEAD
git status --porcelain=v1
```

Result: all exit 0. The exact candidate/tree/parent/base values are recorded above; the merge base equals the stated base, the rejected predecessor is an ancestor, and status output was empty before and after review. Scratch rebuilds and outputs remained under the assigned build root. `git diff --name-only 08481f4..34439e8` showed only Terminal implementation/tests, the three affected wiki pages, and additive worker coordination records; the repair commit itself changes no shared Settings schema or cross-module registry.

### Configure

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-codex/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON

cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-codex/release -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Result: Debug exit 0; Release exit 0. CMake emitted the existing mixed-root runtime-search-path warnings.

### Focused builds

For each of `debug` and `release`:

```sh
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-codex/<profile> \
  --parallel 3 --target \
  qindaqt-terminal \
  qindaqt_terminal_launch_policy_tests \
  qindaqt_terminal_pty_bridge_tests \
  qindaqt_terminal_session_tests \
  qindaqt_terminal_process_group_tests \
  qindaqt_terminal_appearance_tests \
  qindaqt_terminal_profiles_tests \
  qindaqt_terminal_session_collection_tests \
  qindaqt_terminal_profile_settings_tests \
  qindaqt_terminal_window_tests \
  qindaqt_terminal_app_shell_tests \
  qindaqt_terminal_tabs_tests \
  qindaqt_terminal_widget_adapter_tests \
  qindaqt_settings_schema_tests
```

Result: Debug exit 0, 107/107 final Ninja actions; Release exit 0, 112/112. Both reused build logs reported `premature end of file; recovering` before rebuilding successfully; no compiler or linker failure was suppressed.

### Original reproductions

Both supplied sources were freshly compiled and relinked against the exact rebuilt Debug `libqindaqt_terminal_support.a` and its Ninja-reported dependency line before execution.

```sh
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS \
  QT_QPA_PLATFORM=offscreen \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-codex/repro-process-group-leak
```

Result: exit 0. Output:

```text
shutdownFinished.clean=false state=5 descendant=2671074 pgid=2671073 alive=true
```

The 100 ms kill-grace reproducer caught the orphan before its new parent reaped it. The candidate correctly failed closed (`ShutdownFailed`) and retained ownership; the reproducer then killed its fixture. This is the required opposite of the predecessor's `clean=true` leak.

```sh
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS \
  QT_QPA_PLATFORM=offscreen \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-codex/repro-profile-empty-arg
```

Result: exit 0. Output:

```text
before=\npayload before_count=2 after_count=2 first_after=
```

### Required focused and adjacent tests

For each of `debug` and `release`:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS \
  QT_QPA_PLATFORM=offscreen \
  ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-codex/<profile> \
  -R '^qindaqt\.terminal-' --output-on-failure --no-tests=error

env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS \
  QT_QPA_PLATFORM=offscreen \
  ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-codex/<profile> \
  -R '^qindaqt\.settings-schema$' --output-on-failure --no-tests=error
```

Result: Debug Terminal exit 0, 15/15 passed in 10.39 s; Release Terminal exit 0, 15/15 passed in 10.33 s. Debug schema exit 0, 1/1 passed; Release schema exit 0, 1/1 passed.

Additional hostile-fixture repetition:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS \
  QT_QPA_PLATFORM=offscreen \
  ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-codex/debug \
  -R '^qindaqt\.terminal-process-group$' --repeat until-fail:10 \
  --output-on-failure --no-tests=error
```

Result: exit 0; 10/10 consecutive executions passed.

The targeted empty-argument/dialog unit also exited 0 with 3/3 QtTest cases (init, focused case, cleanup).

### PTY and process cleanup

```sh
ls -1 /dev/pts | wc -l
ps -eo pid=,ppid=,pgid=,stat=,comm=,args= | \
  awk '$5 ~ /qindaqt_terminal_process_group_tests|repro-process-group-leak|repro-profile-empty-arg/'
```

Result: `/dev/pts` count was 9 before the suites and 9 after all suites and the ten-run repetition. The filtered process listing was empty before and after; no fixture child or zombie remained.

### Static, documentation, formatting, and JSON gates

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-codex/site
./tools/check-source-shape
git diff --check
git diff --check 4c23978886689e06dde08805483b1495942dc017..34439e80b6e66eaf47561a3234b99433d8cf4bcb
python3 -m json.tool data/settings/schema-v2.json >/dev/null
```

Result: all exit 0. Documentation validated 118 Markdown documents/navigation; strict MkDocs succeeded; source shape checked 1,836 files with 0 skipped and repeated two pre-existing out-of-lane 500/539-line decomposition warnings. Both diff checks were clean. The only JSON changed across the full lane base-to-candidate range, `data/settings/schema-v2.json`, parsed successfully; the repair commit itself changed no JSON.

## Verdict

The exact candidate is rejected only for P2-R2-1: the four repaired product behaviors are present and all executed gates pass, but the explicit fake-client/dialog non-closure acceptance evidence remains absent and the substitute `result()==Rejected` assertions are non-discriminating.

VERDICT REJECT P0/P1/P2/P3=0/0/1/0
