# Dina St Johnston — independent Terminal S1 exact-candidate review

- Persona: Dina St Johnston (`dina-st-johnston`), independent first-party application reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Candidate SHA: `08481f496438ec112753fef30a797db5a19af654`
- Tree SHA: `ab51077cd0241eaddf20872bbf3e30335debd621`
- Parent SHA: `832077f56be9540c5c6645ed98b307112b4753ca`
- Base SHA: `4c23978886689e06dde08805483b1495942dc017`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/terminal-s1-codex-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-codex`

## Findings ledger

### P0

None.

### P1

#### P1-1 — Shutdown reports clean after reaping only the group leader and leaks an HUP-immune descendant

`TerminalSession::pollTick()` treats `waitpid(groupLeader) == Exited` as proof that teardown is complete and immediately releases the backend and captured process-group id (`src/apps/terminal/session/terminal_session.cpp:312-315`, `:219-239`). That proves only the direct child/group leader is gone. A descendant can remain in the same process group after the leader exits. Once the leader is reaped, the production signal guard requires `getpgid(groupLeader) == groupLeader` (`src/apps/terminal/session/process_liveness.cpp:17-20`), so later TERM/KILL cannot reach the surviving group through this API. The only focused proof is a fake monitor that models one PID (`tests/apps/terminal/tst_terminal_session.cpp:310-331`); it cannot observe a group member.

Reproduction source: `/home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-codex/repro-process-group-leak.cpp`. It links the exact Debug candidate library, creates a session leader plus an HUP/TERM-immune descendant in the same process group, models the PTY-close HUP, invokes the real `TerminalSession` with the real `PosixProcessMonitor`, and kills the leaked fixture after observation.

Command:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS \
  QT_QPA_PLATFORM=offscreen \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-codex/repro-process-group-leak
```

Observed (exit 1):

```text
shutdownFinished.clean=true state=4 descendant=2539482 pgid=2539481 alive=true
REPRO_EXIT=1
```

The descendant was then killed by the reproduction and `/proc/2539482/stat` was absent. Expected: `clean=true`/`ShutdownComplete` must not publish until the captured process group is empty; otherwise the session must retain ownership and refuse close/quit. This violates the documented deterministic child/process-group teardown and makes close-all/application quit orphan a real process.

#### P1-2 — Asynchronous Settings1 apply failures are never presented, so conflict/no-replay truth is not user-visible

`TerminalProfileSettings` emits its only complete per-key result ledger from `src/apps/terminal/profiles/terminal_profile_settings.cpp:320-363`. The production caller closes the modal dialog as soon as `applyProfiles()` synchronously accepts the request and handles only the immediate `false` case (`src/apps/terminal/ui/terminal_window_actions.cpp:226-245`). No production object connects to `applyFinished`; only tests observe it. Consequently an authoritative conflict, confirmed rejection, or uncertain timeout/owner loss arrives after the dialog has closed and produces no visible or accessible diagnostic. The user cannot know that remaining writes were aborted or that explicit re-apply is required.

Command:

```sh
rg -n 'applyFinished|applyProfiles\(' src/apps/terminal tests/apps/terminal/tst_terminal_profile_settings.cpp
```

Observed: production matches are only the signal declaration, the emission, and the synchronous `applyProfiles()` call; every `applyFinished` receiver is a `QSignalSpy` in `tst_terminal_profile_settings.cpp`. Expected: the application presentation must consume the terminal-owned ledger and expose conflict/uncertain/partial-success truth accessibly (or retain an equivalent visible draft/apply surface). This violates the review's draft/apply/conflict/no-replay requirement.

### P2

#### P2-1 — The profile editor silently deletes valid empty argv elements

Empty arguments are valid under the model and launch policy: `validateShellArguments()` checks count, byte size, and controls but does not reject empty strings (`src/apps/terminal/profiles/terminal_profile.cpp:66-81`). The dialog renders argv by newline joining (`src/apps/terminal/ui/terminal_profile_dialog.cpp:239-240`) but parses it with `Qt::SkipEmptyParts` and discards blank lines (`:265-270`). Opening and accepting a valid profile therefore changes its executable argv without warning.

Reproduction source: `/home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-codex/repro-profile-empty-arg.cpp`.

Command:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS \
  QT_QPA_PLATFORM=offscreen \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-codex/repro-profile-empty-arg
```

Observed (exit 1):

```text
before=\npayload before_count=2 after_count=1 first_after=payload
REPRO_EXIT=1
```

Expected: accepting an unchanged valid profile preserves `argv` exactly, including an empty element. A bounded workaround is to avoid empty arguments or avoid editing that profile through the dialog.

### P3

#### P3-1 — The wiki index still describes Terminal as single-session

`docs/wiki/index.md:152-154` says the Terminal page documents a “first-party single-session terminal,” while the candidate and owning page implement/document up to eight tabs. Command `rg -n -C 2 'single-session|QindaQt Terminal' docs/wiki/index.md` reproduces the stale text. The owning page itself moved tabs, profiles, and persistence out of S0 deferrals truthfully.

## Review-question evidence

1. **Session lifetime and teardown:** the eight-session bound, close/cancel/refusal paths, restart cancellation, and collection removal tests pass in both configurations. P1-1 defeats the central guarantee: the real candidate state machine reports a clean shutdown while a same-group descendant remains alive. No focused registered test checks descendant/group emptiness or zombies.
2. **No new execution authority:** profiles remain argv-only and resolve via `TerminalLaunchPolicy`; no shell string, `system`, `popen`, or `QProcess` execution was added. Hostile relative/control-bearing paths and arguments, duplicate/oversized lists, font-size, theme-id, and scrollback bounds are covered and pass. P2-1 is a lossy editor bug, not an authority expansion.
3. **Persistence:** the three additive Settings1 keys and schema defaults are exact; malformed/wrong-typed/inconsistent snapshots fall back to built-ins; owner loss and uncertain no-replay model tests pass; no session content/scrollback/title/argv history is persisted. P1-2 means asynchronous result truth exists only in an unconsumed signal and is not visible/accessibly actionable in the production UI.
4. **UI:** the tab row passes with `QT_FATAL_WARNINGS=1`, `QT_QPA_PLATFORM=offscreen`, and `DISPLAY`, `WAYLAND_DISPLAY`, and `DBUS_SESSION_BUS_ADDRESS` unset. It proves traversal/movement/close plus `PageTabList`/`PageTab` names and roles. The 14-action AppShell catalog and local enabled activation pass, and the window tests cover stable QAction names/shortcuts and state. No host display or bus was contacted by these executions.
5. **Boundaries and docs:** qtermwidget includes occur only in `ui/terminal_widget_adapter.cpp` and `ui/terminal_widget_adapter_appearance.cpp`; the only source link is `PRIVATE qtermwidget6` in the adapter target. Shared Settings/AppShell paths restored from WIP and installed metadata/CLI/desktop fixture paths are unchanged from base. Schema edits are append-only. Source shape and documentation gates pass, subject to P3-1.

## Commands and results

### Identity and cleanliness

```sh
git rev-parse HEAD
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git rev-parse 4c23978
git status --porcelain=v1
```

Result: exit 0; returned the candidate/tree/parent/base values in the header; status output was empty before review and after all product inspection. All scratch files are outside the worktree under the assigned build root.

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

Result: Debug exit 0; Release exit 0. CMake emitted pre-existing mixed-root runtime-search-path warnings outside the candidate lane.

### Focused builds

For each of `debug` and `release`:

```sh
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-codex/<profile> \
  --parallel 3 --target \
  qindaqt-terminal \
  qindaqt_terminal_launch_policy_tests \
  qindaqt_terminal_pty_bridge_tests \
  qindaqt_terminal_session_tests \
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

Result: Debug exit 0; Release exit 0. Ninja repeatedly reported `premature end of file; recovering` for its build log, rebuilt the selected graph, and completed all 99 Debug / 104 Release final edges successfully. This did not suppress a compiler failure; the final binaries then passed both selectors.

### Focused and adjacent tests

For each of `debug` and `release`:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS \
  QT_QPA_PLATFORM=offscreen \
  ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-codex/<profile> \
  -R '^(qindaqt\.terminal-|qindaqt\.settings-schema$)' \
  --output-on-failure --no-tests=error
```

Result: Debug exit 0, 15/15 passed; Release exit 0, 15/15 passed. The required terminal-only count is 14/14 in each; the fifteenth row is the adjacent additive schema test.

Explicit fatal-warning environment proof:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS \
  QT_QPA_PLATFORM=offscreen \
  ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-codex/debug \
  -R '^qindaqt\.terminal-tabs-offscreen$' -V --no-tests=error
```

Result: exit 0, 1/1 CTest row; QtTest 3 passed, 0 failed. Verbose CTest showed `QT_QPA_PLATFORM=offscreen` and `QT_FATAL_WARNINGS=1`.

### Static, documentation, registry, and confinement gates

```sh
./tools/validate-docs
```

Result: exit 0; 118 Markdown documents/navigation validated.

```sh
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-codex/site
```

Result: exit 0.

```sh
./tools/check-source-shape
```

Result: exit 0; 1,833 files checked, 0 skipped; two pre-existing out-of-lane 500/539-line decomposition warnings.

```sh
git diff --check 4c23978886689e06dde08805483b1495942dc017..08481f496438ec112753fef30a797db5a19af654
python3 -m json.tool data/settings/schema-v2.json >/dev/null
```

Result: both exit 0.

```sh
git diff --exit-code 4c23978886689e06dde08805483b1495942dc017 \
  08481f496438ec112753fef30a797db5a19af654 -- \
  src/app_shell/CMakeLists.txt \
  src/settings/include/qindaqt/settings/settings_types.h \
  src/settings/src/settings_types.cpp \
  tests/settings/tst_settings_schema.cpp \
  src/apps/terminal/org.qindaqt.Terminal.desktop \
  tests/apps/terminal/check_cli_rejection.cmake \
  tests/apps/terminal/check_desktop_metadata.cmake \
  tests/apps/terminal/run_installed_terminal.cmake
```

Result: exit 0, empty diff.

```sh
rg -n '^#include <qtermwidget\.h>|PRIVATE qtermwidget6' src
```

Result: the two adapter implementation includes and one private adapter link only.

## Verdict

The exact candidate is rejected. It passes its registered tests and static gates, but the registered lifecycle proof misses a reproducible orphaned same-group descendant; persistence failure truth is not wired to production presentation; and the profile editor changes a valid argv.

VERDICT REJECT P0/P1/P2/P3=0/2/1/1
