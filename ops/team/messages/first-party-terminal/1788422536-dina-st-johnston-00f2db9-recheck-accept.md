# Dina St Johnston — Terminal S1 second-repair descendant recheck

- Persona: Dina St Johnston (`dina-st-johnston`), independent first-party application reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Candidate SHA: `00f2db99ce13df2f426abd93e277de3d33411041`
- Tree SHA: `843d9de4fe55aa61b29a9686cf3c4780d9842a32`
- Parent SHA: `ff19cbe6dc05f5e7564407c04ca401315f03c4d3`
- Base SHA: `4c23978886689e06dde08805483b1495942dc017`
- Repaired product ancestor: `34439e80b6e66eaf47561a3234b99433d8cf4bcb`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/terminal-s1-codex-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-codex`

## Findings ledger

### P0

None.

### P1

None.

### P2

None. Prior P2-R2-1 is closed: the new rows enter the production `profileManageAction`/`TerminalWindow::manageProfiles()` modal path, drive a real `SettingsClient` through the injected fake transport, and distinguish the live failure state from the all-applied close state.

### P3

None.

## Recheck conclusions

1. **Prior P1-1 remains closed.** The freshly relinked process-group reproduction exited 0 and reported `clean=true` only after the descendant was absent (`alive=false`). The registered hostile descendant ignores HUP and TERM, requiring the bounded KILL phase; the full selectors passed and the Debug row passed ten consecutive executions. Source inspection confirms that the captured group id is retained through shutdown, clean completion requires `ProcessGroupState::Empty`, `Unknown` and `NonEmpty` fail closed after the KILL grace, and failed generations retain the backend/group ownership. `FixtureCleanup` sends group/descendant/leader SIGKILL and reaps on every failing return path.
2. **Prior P1-2 remains closed, and its missing dialog proof is repaired.** The five fake-transport rows cover conflict, confirmed persistence rejection, session-bus transport loss, Settings1 owner loss, and commit timeout. Each enters the visible production modal, starts Apply, and then asserts the dialog remains visible, modal, enabled, unfinished, and re-applicable; the result label remains visible with `Profile save status` and an accessible description exactly matching the complete three-key result. Uncertain rows assert “not replayed” and one commit only. The all-applied control drives three sequential commits plus fresh snapshots, then observes the dialog hidden and `Accepted`; the failure predicate is explicitly false and the dialog is destroyed after the production modal returns.
3. **Prior P2-1 remains closed.** The freshly relinked empty-argv reproduction exited 0 with `before_count=2`, `after_count=2`, and the leading empty argument preserved.
4. **Prior P3-1 remains closed.** `docs/wiki/index.md:152-154` says “multi-session” and states the eight-tab bound.
5. **Scope is bounded.** The direct repair commit changes only `docs/wiki/apps/terminal.md` and `tests/apps/terminal/tst_terminal_profile_settings.cpp`. Its parent is the additive repair-handoff coordination commit; the complete `34439e8..00f2db9` range adds only Terminal coordination records plus those two product paths. No production source, shared registry, schema, host integration, or execution authority changed in this descendant.

## Commands and results

### Identity, ancestry, scope, and cleanliness

```sh
git rev-parse HEAD
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git rev-parse 4c23978886689e06dde08805483b1495942dc017
git merge-base 4c23978886689e06dde08805483b1495942dc017 HEAD
git merge-base --is-ancestor 34439e80b6e66eaf47561a3234b99433d8cf4bcb HEAD
git diff --name-status 34439e80b6e66eaf47561a3234b99433d8cf4bcb..HEAD
git show --format= --name-status HEAD
git status --porcelain=v1
```

Result: all exit 0. Candidate, tree, parent, and base match the header; the merge base equals the stated base; `34439e8` is an ancestor. Status output was empty before review and after every build/test/static gate. Scratch artifacts remained under the assigned build root.

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

Result: Debug exit 0, 104/104 final Ninja actions; Release exit 0, 109/109. The reused Ninja logs emitted `premature end of file; recovering`; both target graphs rebuilt and linked successfully under strict warnings.

### Original reproductions

Both supplied reproduction sources were freshly compiled and relinked with `/usr/bin/c++` against the rebuilt Debug `libqindaqt_terminal_support.a` and the exact include/link dependencies reported by `ninja -t commands`.

```sh
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS \
  QT_QPA_PLATFORM=offscreen \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-codex/repro-process-group-leak
```

Result: exit 0.

```text
shutdownFinished.clean=true state=4 descendant=2744499 pgid=-1 alive=false
```

The timing allowed the new parent to reap the killed orphan before the 100 ms kill-grace observation; clean was published only after no member remained.

```sh
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS \
  QT_QPA_PLATFORM=offscreen \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-codex/repro-profile-empty-arg
```

Result: exit 0.

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

Result: Debug Terminal exit 0, 15/15 passed in 10.93 s; Release Terminal exit 0, 15/15 passed in 10.86 s. Debug and Release schema selectors each exited 0 with 1/1 passed.

Targeted production-dialog execution, for each profile:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS \
  QT_QPA_PLATFORM=offscreen \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-codex/<profile>/tests/apps/terminal/qindaqt_terminal_profile_settings_tests \
  productionDialogPresentsAsynchronousOutcome \
  productionDialogClosesOnlyAfterAllApplied -v1
```

Result: Debug exit 0, 8/8 passed; Release exit 0, 8/8 passed. The five named failure rows and all-applied control executed in both profiles. Qt's offscreen plugin emitted only its expected `propagateSizeHints()` warnings.

```sh
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS \
  QT_QPA_PLATFORM=offscreen \
  ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-codex/debug \
  -R '^qindaqt\.terminal-process-group$' --repeat until-fail:10 \
  --output-on-failure --no-tests=error
```

Result: exit 0; 10/10 consecutive executions passed.

### PTY and process cleanup

```sh
ls -1 /dev/pts | wc -l
ps -eo pid=,ppid=,pgid=,stat=,comm=,args= | \
  rg 'qindaqt_terminal_process_group_tests|repro-process-group-leak|repro-profile-empty-arg'
```

Result: `/dev/pts` count was 9 before and 9 after all suites, reproductions, and repetition. The fixture/reproduction process search was empty before and after; no matching child or zombie remained.

### Static and documentation gates

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-codex/site
./tools/check-source-shape
git diff --check
git diff --check 4c23978886689e06dde08805483b1495942dc017..00f2db99ce13df2f426abd93e277de3d33411041
python3 -m json.tool data/settings/schema-v2.json >/dev/null
```

Result: all exit 0. Documentation validated 118 Markdown documents/navigation; strict MkDocs succeeded; source shape checked 1,836 files with 0 skipped and repeated only the two pre-existing out-of-lane 500/539-line decomposition warnings. The changed Terminal profile-settings test is 490 non-blank lines. Both diff checks were clean; the only JSON changed across the lane base-to-candidate range parsed successfully. No JSON changed in this repair descendant.

## Verdict

The exact second-repair descendant closes the prior proof defect without changing production behavior. The earlier product repairs remain effective, every required isolated execution and static gate passes, and no PTY/process residue or out-of-scope product change was found.

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/0
