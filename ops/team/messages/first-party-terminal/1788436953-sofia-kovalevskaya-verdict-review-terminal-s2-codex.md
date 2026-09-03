# Sofia Kovalevskaya — independent Terminal S2 exact-candidate review

- Persona: Sofia Kovalevskaya, independent application reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Candidate SHA: `0949cb985c1a3589135c42f330ae6630fb0f5573`
- Tree SHA: `baac8037333c5c96ffd7728e117955d779320f80`
- Parent SHA: `b2f515986150b1acfe82e2807a78a731a58a94a2`
- Base SHA: `b2f515986150b1acfe82e2807a78a731a58a94a2`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/terminal-s2-codex-review`

## Verdict summary

REJECT. The candidate has two P1 contract violations and four P2 defects/test gaps. The prescribed Debug and Release builds and 19-row selectors are green, the PTY boundary remains green, the opener poison is untouched, and all static gates pass. Those gates do not cover the failures below.

## Findings ledger

### P0

None.

### P1

#### P1-1 — Tab switching announces another session's search result

`TerminalWindow` stores only query and bar visibility per session (`src/apps/terminal/ui/terminal_window.h:172-173`). `restoreSearchPresentation()` restores only those values (`src/apps/terminal/ui/terminal_window_search.cpp:67-74`), leaving `TerminalFindBar`'s visible text and accessible name from the previously selected tab. This violates the contract that every session owns its current match and that match position is truthfully announced without relying on color.

Reproduction (scratch file outside the worktree):

```sh
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_QPA_PLATFORM=offscreen /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-s2-codex/repros/tab_status_repro
```

Exit 0, observed:

```text
tab1_before=Match 1 of 2
tab2=No matches
tab1_query_after=first
tab1_status_after=No matches
tab1_accessible_after=Search status: No matches
```

Expected after returning to tab 1: query `first` with that session's `Match 1 of 2` visible and accessible status. Observed: the query is restored while both statuses falsely say `No matches` from tab 2.

#### P1-2 — Bidi formatting controls survive the control-free link gate

`stripControls()` removes null, surrogates, and `QChar::Other_Control`, but not `Other_Format` (`src/apps/terminal/links/terminal_link.cpp:17-36`). U+202E therefore survives `isAdmittedTerminalLink()` (`terminal_link.cpp:80-93`) and reaches the plain-text confirmation unchanged (`terminal_link_opener.cpp:64-76`). A terminal child can print a path whose confirmation is visually reordered, defeating the exact-target confirmation barrier.

Reproduction:

```sh
/home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-s2-codex/repros/policy_repro
```

Exit 0, relevant output:

```text
bidi_category=10
link_count=1
admitted=1
contains_bidi=1
target_hex=002f 0073 0061 0066 0065 002f 0072 0065 0070 006f 0072 0074 202e 0066 0064 0070 002e 0065 0078 0065
```

Expected: strip or reject the bidi formatting control before selection and confirmation. Observed: the U+202E-bearing path is detected and admitted. The registered control test covers BEL only (`tests/apps/terminal/tst_terminal_links.cpp:67-79`).

### P2

#### P2-1 — `Shift+F3` after Escape resumes forward instead of previous

Escape resets the adapter query/index (`src/apps/terminal/ui/terminal_widget_adapter_search.cpp:159-176`) while the window retains its query. The next hidden-bar `Shift+F3` passes `Previous`, but the changed-query branch always invokes forward `find` and sets index zero (`terminal_widget_adapter_search.cpp:119-134`).

Production-adapter/real-PTY reproduction:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_QPA_PLATFORM=offscreen /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-s2-codex/repros/previous_after_escape_repro
```

Exit 0, observed:

```text
started=1
initial=1/2
regex=1/2 selected=1
previous_after_clear=1/2 wrapped=0
```

Expected: Previous from a cleared selection yields match 2/2 with wrap. Observed: match 1/2, no wrap. Workaround: reopen the bar and establish a current match first.

#### P2-2 — Open/Copy can act on a link that is no longer visible

`copyCurrentLink()` and `openCurrentLink()` trust cached `m_linkBySession` whenever its old `found` bit is true, refreshing only when it is false (`src/apps/terminal/ui/terminal_window_links.cpp:42-73`). Output and scroll changes do not invalidate that cache.

The P1-1 scratch reproduction also observed:

```text
current_visible_target=https://new.example
confirmed_stale_target=https://old.example
spawned_stale_target=https://old.example
```

Expected: refresh visible links and invalidate/reselect before confirmation. Observed: the old, no-longer-visible target is confirmed and dispatched as argv. Workaround: traverse again after output/scroll changes.

#### P2-3 — The absolute root path `/` is not detected

The detector requires `/` to have a following non-space character (`src/apps/terminal/links/terminal_link.cpp:108-109`), although `QDir::isAbsolutePath("/")` is true and the contract accepts absolute local paths.

The policy reproduction observed `root_path_count=0`; expected one absolute local-path link. Other non-root absolute paths work.

#### P2-4 — Required `file://` negative-control coverage is absent

The lane explicitly requires a `file://` edge row. `tests/apps/terminal/tst_terminal_links.cpp:81-87` rejects overlong, `ftp://`, and relative targets, but no Terminal test mentions `file://`.

```sh
rg -n 'file://' tests/apps/terminal
```

Exit 1, no output. Production currently rejects it (`file_url_count=0` in `policy_repro`), but a deliberately broken variant admitting `file://` would pass the focused suite.

### P3

None.

## Review-question evidence

1. Search admission rejects the tested nested quantifier/extensions before the 4 MiB scan and caps patterns/results; Debug/Release policy and real-adapter rows pass. Search is memory-only, Escape focus return is tested, and qtermwidget types remain confined. P1-1/P2-1 block truthful per-session behavior.
2. Links never auto-activate. Confirmation is plain text and production uses `QProcess::startDetached(absoluteProgram, arguments)` with one argument, never a shell string. A scratch `xdg-open` poison was not invoked. Punctuation, ASCII quotes, parentheses, overlength, BEL, and homoglyph tests pass; P1-2 and P2-2 through P2-4 remain.
3. `pty_bridge.*`, `process_liveness.*`, and `tst_terminal_process_group.cpp` are unchanged from base. PTY bridge and process-group rows pass in both profiles.
4. Prescribed selectors pass 19/19 in Debug and Release. Both S2 offscreen rows pass 2/2 with externally forced `QT_FATAL_WARNINGS=1` and display/bus variables isolated. Documentation/static gates pass and later GPU/global-menu/whole-app AT/nested/physical qualifications remain excluded. P2-4 is the missing required negative control.

## Commands and results

### Configure and build

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-s2-codex/dev -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-s2-codex/release -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Both exit 0.

```sh
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-s2-codex/dev --parallel 3 --target qindaqt-terminal qindaqt_terminal_launch_policy_tests qindaqt_terminal_pty_bridge_tests qindaqt_terminal_session_tests qindaqt_terminal_process_group_tests qindaqt_terminal_appearance_tests qindaqt_terminal_profiles_tests qindaqt_terminal_session_collection_tests qindaqt_terminal_profile_settings_tests qindaqt_terminal_window_tests qindaqt_terminal_app_shell_tests qindaqt_terminal_search_tests qindaqt_terminal_links_tests qindaqt_terminal_search_links_ui_tests qindaqt_terminal_adapter_boundary_tests qindaqt_terminal_tabs_tests qindaqt_terminal_widget_adapter_tests
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-s2-codex/release --parallel 3 --target qindaqt-terminal qindaqt_terminal_launch_policy_tests qindaqt_terminal_pty_bridge_tests qindaqt_terminal_session_tests qindaqt_terminal_process_group_tests qindaqt_terminal_appearance_tests qindaqt_terminal_profiles_tests qindaqt_terminal_session_collection_tests qindaqt_terminal_profile_settings_tests qindaqt_terminal_window_tests qindaqt_terminal_app_shell_tests qindaqt_terminal_search_tests qindaqt_terminal_links_tests qindaqt_terminal_search_links_ui_tests qindaqt_terminal_adapter_boundary_tests qindaqt_terminal_tabs_tests qindaqt_terminal_widget_adapter_tests
```

Both exit 0, 245/245 actions.

### Prescribed selectors

```sh
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-s2-codex/dev -R '^qindaqt\.terminal-' --output-on-failure --no-tests=error
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-s2-codex/release -R '^qindaqt\.terminal-' --output-on-failure --no-tests=error
```

Debug exit 0, 19/19 passed (11.52 s). Release exit 0, 19/19 passed (11.39 s).

### Fatal-warning and opener-poison checks

```sh
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1 ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-s2-codex/dev -R '^qindaqt\.terminal-(search-links-offscreen|widget-adapter-offscreen)$' --output-on-failure --no-tests=error
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1 ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-s2-codex/release -R '^qindaqt\.terminal-(search-links-offscreen|widget-adapter-offscreen)$' --output-on-failure --no-tests=error
```

Both exit 0, 2/2 passed.

A supplemental stronger whole-selector run with global `QT_FATAL_WARNINGS=1` exited 8 in both profiles: 17/19 passed; unchanged S1 `terminal-profile-settings` and `terminal-window-offscreen` aborted on the offscreen plugin's `propagateSizeHints()` warning. The S2 rows passed. This is recorded but not counted because it is unchanged baseline behavior outside the S2 fatal-warning registration.

```sh
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent PATH=/home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-s2-codex/repros/poison-bin:/usr/bin:/bin ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-s2-codex/dev -R '^qindaqt\.terminal-(search-policy|links|search-links-offscreen|adapter-boundary|widget-adapter-offscreen)$' --output-on-failure --no-tests=error
test ! -e /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-s2-codex/repros/xdg-open-invoked
```

Exit 0, 5/5 passed, no poison marker.

### Static and boundary gates

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-s2-codex/site
./tools/check-source-shape
git diff --check b2f515986150b1acfe82e2807a78a731a58a94a2..0949cb985c1a3589135c42f330ae6630fb0f5573
rg -n '#include[[:space:]]*[<"]qtermwidget' src tests
rg -n 'xdg-open|QDesktopServices::openUrl|startDetached|system\(|popen\(' tests/apps/terminal src/apps/terminal
```

All required static gates exit 0. Docs: 128 Markdown files. Strict MkDocs: success. Shape: 2,074 files, four unrelated pre-existing warnings, no Terminal production warning. qtermwidget includes occur only in the three confined adapter implementation units. No JSON changed, so `python3 -m json.tool` was not applicable.

```sh
git diff --quiet b2f515986150b1acfe82e2807a78a731a58a94a2..HEAD -- src/apps/terminal/session/pty_bridge.cpp src/apps/terminal/session/pty_bridge.h src/apps/terminal/session/process_liveness.cpp src/apps/terminal/session/process_liveness.h tests/apps/terminal/tst_terminal_process_group.cpp
```

Exit 0: PTY/process-group paths unchanged.

Scratch reproducers were compiled outside the worktree against the candidate Debug libraries. The first `policy_repro` compilation attempt exited 1 due solely to a reviewer-harness `char16_t` formatting overload; the corrected harness compiled and produced the observations above.

### Final immutability check

```sh
git rev-parse HEAD
git status --porcelain
```

Observed candidate `0949cb985c1a3589135c42f330ae6630fb0f5573`; empty status.

## Verdict

VERDICT REJECT P0/P1/P2/P3=0/2/4/0

