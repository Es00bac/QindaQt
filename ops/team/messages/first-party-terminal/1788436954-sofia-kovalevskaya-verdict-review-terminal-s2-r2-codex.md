# Sofia Kovalevskaya — Terminal S2 repair-descendant exact-candidate recheck

- Persona: Sofia Kovalevskaya, independent application reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Candidate SHA: `e030a42d1440591b73685ac464912e3c500dafdd`
- Tree SHA: `9a0eb7886d7cdceb08356f638346b95884cc6365`
- Parent SHA: `6ab1ce79881f17470f122781c7726d96b74e34ed`
- Base SHA: `b2f515986150b1acfe82e2807a78a731a58a94a2`
- Rejected product ancestor: `0949cb985c1a3589135c42f330ae6630fb0f5573`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/terminal-s2-codex-review`

## Verdict summary

ACCEPT. The repaired descendant closes all six prior findings, its registered
assertions are non-vacuous, and the focused Debug/Release, hostile-regex,
no-real-opener, PTY teardown, documentation, source-shape, and diff-hygiene
gates pass. No P0, P1, P2, or P3 finding remains.

## Findings ledger

### P0

None.

### P1

None.

### P2

None.

### P3

None.

## Prior-finding closure

### P1-1 — per-session search result and announcement

Closed. `src/apps/terminal/ui/terminal_window_search.cpp:25-27,59-79` stores
and restores each session's `TerminalSearchResult` together with query and bar
visibility; removal also clears that map at
`src/apps/terminal/ui/terminal_window.cpp:129-134`.

Freshly linked prior repro, exit 0:

```text
tab1_before=Match 1 of 2
tab2=No matches
tab1_query_after=first
tab1_status_after=Match 1 of 2
tab1_accessible_after=Search status: Match 1 of 2
```

The registered
`TerminalSearchLinksUiTest::findTextAndVisibilityArePerSessionAndVolatile`
asserts both restored visible text and exact accessible name at
`tests/apps/terminal/tst_terminal_search_links_ui.cpp:254-276`. Those
assertions would fail on `0949cb9`, whose exact prior repro returned `No
matches` for both restored fields.

### P1-2 — `Other_Format` at the link gate

Closed. `src/apps/terminal/links/terminal_link.cpp:17-47` removes BMP and
supplementary `Other_Format` code points, and direct admission refuses any
target changed by that sanitizer at lines 90-103. The prior policy repro now
reports `contains_bidi=0`; the registered
`TerminalLinksTest::rejectsUnicodeFormatControls` directly asserts rejection
of U+202E and U+200D at `tests/apps/terminal/tst_terminal_links.cpp:85-99`.
That `QVERIFY(!isAdmittedTerminalLink(direct))` fails on `0949cb9`, whose exact
prior repro returned `admitted=1` and `contains_bidi=1`.

### P2-1 — `Shift+F3` after Escape

Closed. `src/apps/terminal/ui/terminal_widget_adapter_search.cpp:119-188`
honors `Previous` during changed-query initialization and rehydrates a cleared
renderer to the retained logical match before applying the requested
direction. Fresh real-PTY repro, exit 0:

```text
started=1
initial=1/2
regex=1/2 selected=1
previous_after_clear=2/2 wrapped=1
```

The registered production-adapter row asserts `current == 2` and `wrapped`
at `tests/apps/terminal/tst_terminal_widget_adapter.cpp:117-131`; those exact
assertions fail on `0949cb9`, whose prior repro returned `1/2 wrapped=0`.

### P2-2 — stale visible-link activation

Closed. `src/apps/terminal/ui/terminal_window_links.cpp:42-86` re-reads current
viewport selection before copy, confirmation, or spawn and stops if it differs
from the cached traversed value. Fresh prior repro, exit 0:

```text
current_visible_target=https://new.example
confirmed_stale_target=
spawned_stale_target=
```

The registered
`TerminalSearchLinksUiTest::staleLinkSelectionCannotCopyOrOpen` changes the
fake viewport twice and asserts unchanged clipboard plus zero confirmation and
spawn calls at `tests/apps/terminal/tst_terminal_search_links_ui.cpp:279-303`.
Those assertions fail on `0949cb9`, whose prior repro confirmed and spawned
`https://old.example`.

### P2-3 — absolute root `/`

Closed. The detector now admits a boundary slash without requiring a following
character (`src/apps/terminal/links/terminal_link.cpp:106-153`). Fresh policy
repro reports `root_path_count=1`. The registered
`TerminalLinksTest::detectsAbsoluteRootPath` asserts one local-path result equal
to `/` at `tests/apps/terminal/tst_terminal_links.cpp:101-108`; it fails on
`0949cb9`, whose prior repro returned `root_path_count=0`.

### P2-4 — `file://` negative controls

Closed by an accepted rebuttal plus registered negative controls. The original
finding was coverage-only: `0949cb9` already returned `file_url_count=0`, so no
behavior assertion can honestly fail that implementation. The descendant now
registers both `file-url-local` and `file-url-host` rows and tests detector and
direct-admission rejection at `tests/apps/terminal/tst_terminal_links.cpp:118-132`.
`rg -n 'file://' tests/apps/terminal` now exits 0 with both rows; the same
command exited 1 on `0949cb9`. Direct execution passes both tagged rows.

## Regression probes

- Hostile regex: the registered 4 MiB nested-quantifier/extensions probe ran
  directly in Debug, exit 0, 3/3 QtTest functions including init/cleanup, in
  5 ms; the full selector runs the same row in both profiles. The real-adapter
  row also retains its `<100 ms` rejection assertion.
- No real opener: with a poisoned `xdg-open` first in `PATH`, the five relevant
  Debug CTest rows passed 5/5 and the poison marker was absent both before and
  after. Production/tests retain absolute-program plus one-argv seams.
- PTY shutdown: `qindaqt.terminal-pty-bridge`,
  `qindaqt.terminal-session`, and `qindaqt.terminal-process-group` passed in
  both full selectors. The bridge, liveness, and process-group test paths are
  unchanged from base (`git diff --quiet ...`, exit 0).
- The old policy harness also directly calls `scanTerminalText` with a string
  one code unit over 4 MiB and reports `oversized_accepted=1`. This bypasses
  production history acquisition and is not a product finding: the only
  production caller uses `BoundedHistoryDevice`, caps bytes, observes
  overflow, and returns before scanning at
  `src/apps/terminal/ui/terminal_widget_adapter_search.cpp:20-60,107-114`.

## Commands and results

All commands ran from the exact worktree unless an absolute path is shown.
No `tests/session` row, host D-Bus service, display, nested compositor,
hardware, uinput, network, or real desktop opener was run.

### Identity and diff inspection

```sh
git rev-parse HEAD
git status --porcelain
git show --no-patch --format='commit=%H%ntree=%T%nparents=%P%nsubject=%s' HEAD
git diff --name-status 0949cb9..e030a42
git diff --unified=100 0949cb9..HEAD -- docs/wiki/apps/terminal.md docs/wiki/development/testing-harness.md src/apps/terminal/links/terminal_link.cpp src/apps/terminal/ui/terminal_widget_adapter.h src/apps/terminal/ui/terminal_widget_adapter_search.cpp src/apps/terminal/ui/terminal_window.cpp src/apps/terminal/ui/terminal_window.h src/apps/terminal/ui/terminal_window_links.cpp src/apps/terminal/ui/terminal_window_search.cpp tests/apps/terminal/tst_terminal_links.cpp tests/apps/terminal/tst_terminal_search_links_ui.cpp tests/apps/terminal/tst_terminal_widget_adapter.cpp
```

Identity matched the candidate; status was empty. The repair diff contains the
six bounded closures and additive worker/message records. Candidate tree,
parent, and base are recorded in the header.

### Configure and build

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-s2-codex/debug -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-s2-codex/release -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Both exit 0.

```sh
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-s2-codex/debug --parallel 3 --target qindaqt-terminal qindaqt_terminal_launch_policy_tests qindaqt_terminal_pty_bridge_tests qindaqt_terminal_session_tests qindaqt_terminal_process_group_tests qindaqt_terminal_appearance_tests qindaqt_terminal_profiles_tests qindaqt_terminal_session_collection_tests qindaqt_terminal_profile_settings_tests qindaqt_terminal_window_tests qindaqt_terminal_app_shell_tests qindaqt_terminal_search_tests qindaqt_terminal_links_tests qindaqt_terminal_search_links_ui_tests qindaqt_terminal_adapter_boundary_tests qindaqt_terminal_tabs_tests qindaqt_terminal_widget_adapter_tests
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-s2-codex/release --parallel 3 --target qindaqt-terminal qindaqt_terminal_launch_policy_tests qindaqt_terminal_pty_bridge_tests qindaqt_terminal_session_tests qindaqt_terminal_process_group_tests qindaqt_terminal_appearance_tests qindaqt_terminal_profiles_tests qindaqt_terminal_session_collection_tests qindaqt_terminal_profile_settings_tests qindaqt_terminal_window_tests qindaqt_terminal_app_shell_tests qindaqt_terminal_search_tests qindaqt_terminal_links_tests qindaqt_terminal_search_links_ui_tests qindaqt_terminal_adapter_boundary_tests qindaqt_terminal_tabs_tests qindaqt_terminal_widget_adapter_tests
```

Both exit 0. Debug completed 245/245 fresh actions; Release completed 77/77
incremental actions.

### Prescribed selectors

```sh
env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-s2-codex/debug -R '^qindaqt\.terminal-' --output-on-failure --no-tests=error
env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-s2-codex/release -R '^qindaqt\.terminal-' --output-on-failure --no-tests=error
```

Debug exit 0, 19/19 passed in 11.71 s. Release exit 0, 19/19 passed
in 11.58 s.

### Prior reproducers and focused assertions

The three prior reproducers were recompiled outside the worktree against the
candidate Debug `qindaqt_terminal_support` / `qindaqt_terminal_adapter`
libraries with `c++ -std=c++20 -mno-direct-extern-access`, the repository and
Qt/qtermwidget include flags, the exact local libraries, and Qt/qtermwidget
link flags. All three compilation commands exited 0.

```sh
/home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-s2-codex/repros/policy_repro
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_QPA_PLATFORM=offscreen /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-s2-codex/repros/previous_after_escape_repro
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_QPA_PLATFORM=offscreen /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-s2-codex/repros/tab_status_repro
```

All exit 0 with the closure outputs recorded above; policy additionally reports
`file_url_count=0` and `contains_bidi=0`.

```sh
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_QPA_PLATFORM=offscreen /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-s2-codex/debug/tests/apps/terminal/qindaqt_terminal_search_tests hostileRegexIsRejectedBeforeExecution -v1
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_QPA_PLATFORM=offscreen /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-s2-codex/debug/tests/apps/terminal/qindaqt_terminal_links_tests rejectsUnicodeFormatControls detectsAbsoluteRootPath rejectsFileUrls -v1
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_QPA_PLATFORM=offscreen QT_FATAL_WARNINGS=1 /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-s2-codex/debug/tests/apps/terminal/qindaqt_terminal_search_links_ui_tests findTextAndVisibilityArePerSessionAndVolatile staleLinkSelectionCannotCopyOrOpen -v1
```

Exit 0: 3/3, 6/6 (including two tagged `file://` rows), and 4/4 QtTest
functions respectively.

### Opener poison and static gates

```sh
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent PATH=/home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-s2-codex/repros/poison-bin:/usr/bin:/bin ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-s2-codex/debug -R '^qindaqt\.terminal-(search-policy|links|search-links-offscreen|adapter-boundary|widget-adapter-offscreen)$' --output-on-failure --no-tests=error
test ! -e /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-s2-codex/repros/xdg-open-invoked
```

CTest exit 0, 5/5 passed; marker check exit 0.

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-terminal-s2-codex/site
./tools/check-source-shape
git diff --check
git diff --check b2f515986150b1acfe82e2807a78a731a58a94a2..HEAD
git diff --check 0949cb985c1a3589135c42f330ae6630fb0f5573..HEAD
```

All exit 0. Docs validated 128 Markdown/navigation entries; strict MkDocs
built in 1.70 s; source-shape checked 2,074 files and reported only four
unrelated existing decomposition warnings. No JSON changed, so the JSON parser
gate is not applicable. qtermwidget includes remain confined to the three
adapter implementation units.

## Final immutability check

```sh
git rev-parse HEAD
git status --porcelain
```

Observed `e030a42d1440591b73685ac464912e3c500dafdd`; status empty.

## Verdict

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/0
