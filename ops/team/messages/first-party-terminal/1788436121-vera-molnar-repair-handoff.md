# Vera Molnar — Terminal S2 rejection repair handoff

- Candidate commit: `e030a42d1440591b73685ac464912e3c500dafdd`
- Candidate tree: `9a0eb7886d7cdceb08356f638346b95884cc6365`
- Rejected candidate: `0949cb985c1a3589135c42f330ae6630fb0f5573`
- Exact lane base: `b2f515986150b1acfe82e2807a78a731a58a94a2`
- Repair parent: `6ab1ce79881f17470f122781c7726d96b74e34ed`
- Branch/worktree: `worker/terminal-s2` at `/home/cabewse/work_SPaC3/container-wm-workers/terminal-s2`

## Finding closure

All product findings close in `e030a42d1440591b73685ac464912e3c500dafdd`.

- P1-1: `TerminalWindow` now stores and restores each session's complete
  `TerminalSearchResult` with its query and visibility. Regression:
  `TerminalSearchLinksUiTest::findTextAndVisibilityArePerSessionAndVolatile`
  (`AGENT-NOTE: review P1-1`). The exact reviewer repro now returns tab one's
  `Match 1 of 2` visible and accessible after returning from tab two.
- P1-2: the admission sanitizer rejects/removes `QChar::Other_Format` for BMP
  and supplementary code points in addition to `Other_Control`. Regression:
  `TerminalLinksTest::rejectsUnicodeFormatControls` covers U+202E and U+200D
  and directly proves the gate refuses the hostile value (`AGENT-NOTE: review
  P1-2`).
- P2-1: Escape clears renderer highlight state but retains the session's
  logical query/index. Directional rehydration honors `Previous`, including a
  changed query, instead of forcing forward initialization. Regression:
  `TerminalWidgetAdapterTest::realPtyOutputSupportsBoundedSearchAndVisibleLinks`
  reproduces the reviewer's real-PTY sequence and expects `2/2 wrapped=1`
  (`AGENT-NOTE: review P2-1`).
- P2-2: Copy/Open always refresh current viewport selection and stop before
  clipboard, confirmation, or spawn when it differs from the traversed value.
  The adapter also retains selection by value and invalidates it when absent.
  Regression: `TerminalSearchLinksUiTest::staleLinkSelectionCannotCopyOrOpen`
  (`AGENT-NOTE: review P2-2`).
- P2-3: the detector admits the single slash root path through the same
  `QDir::isAbsolutePath` gate. Regression:
  `TerminalLinksTest::detectsAbsoluteRootPath` (`AGENT-NOTE: review P2-3`).
- P2-4: registered QtTest data rows `file-url-local` and `file-url-host` now
  exercise detector and direct-admission rejection in
  `TerminalLinksTest::rejectsFileUrls` (`AGENT-NOTE: review P2-4`). This was a
  coverage-only finding: `0949cb9` already rejected both targets, so the new
  negative control is mutation-sensitive but cannot semantically fail the old
  implementation; on `0949cb9` the required test source/rows are absent and
  the reviewer's `rg -n 'file://' tests/apps/terminal` exits 1.

## Changed paths in the repair commit

- `docs/wiki/apps/terminal.md`
- `docs/wiki/development/testing-harness.md`
- `src/apps/terminal/links/terminal_link.cpp`
- `src/apps/terminal/ui/terminal_widget_adapter.h`
- `src/apps/terminal/ui/terminal_widget_adapter_search.cpp`
- `src/apps/terminal/ui/terminal_window.cpp`
- `src/apps/terminal/ui/terminal_window.h`
- `src/apps/terminal/ui/terminal_window_links.cpp`
- `src/apps/terminal/ui/terminal_window_search.cpp`
- `tests/apps/terminal/tst_terminal_links.cpp`
- `tests/apps/terminal/tst_terminal_search_links_ui.cpp`
- `tests/apps/terminal/tst_terminal_widget_adapter.cpp`

## Reproduction and verification evidence

Rejected-candidate reproducers supplied by the reviewer were run before edits:

- `.../repros/tab_status_repro` under offscreen/no-display/private-nonexistent
  bus environment: exit 0; reproduced tab-one status/accessibility as stale
  `No matches` and stale link confirmation/spawn of `https://old.example`.
- `.../repros/policy_repro`: exit 0; reproduced `contains_bidi=1` and
  `root_path_count=0` while `file_url_count=0`.
- `.../repros/previous_after_escape_repro` under the same isolated offscreen
  environment: exit 0; reproduced `previous_after_clear=1/2 wrapped=0`.
- `rg -n 'file://' tests/apps/terminal`: exit 1 with no output, reproducing the
  absent required negative control.

Fresh strict configure and focused builds:

- Exact prescribed Debug configure under
  `/home/cabewse/work_SPaC3/builds/qindaqt/terminal-s2/debug`: exit 0.
- Exact prescribed Release configure under
  `/home/cabewse/work_SPaC3/builds/qindaqt/terminal-s2/release`: exit 0.
- Debug build of `qindaqt-terminal` and all 16 named Terminal test targets with
  `--parallel 3`: exit 0.
- Release build of the same targets with `--parallel 3`: exit 0, 77/77 emitted
  actions completed.
- An early focused Debug build attempt exited 1 on two strict
  `qsizetype`-to-`int` conversion warnings in the new link-refresh code. The
  bounded conversions were made explicit; the subsequent focused and full
  builds above exit 0.

Registered tests, always with `DISPLAY`, `WAYLAND_DISPLAY`, and
`DBUS_SESSION_BUS_ADDRESS` unset and
`DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent`:

- `ctest --test-dir .../debug -R '^qindaqt\.terminal-' --output-on-failure --no-tests=error`:
  exit 0, 19/19 passed in 11.27 s.
- `ctest --test-dir .../release -R '^qindaqt\.terminal-' --output-on-failure --no-tests=error`:
  exit 0, 19/19 passed in 10.84 s.
- Focused Debug repair selector for `links`, `search-links-offscreen`, and
  `widget-adapter-offscreen`: exit 0, 3/3 passed (final run 0.35 s).

The reviewer reproducers were freshly linked to the repaired Debug libraries:

- policy repro: compile/run exit 0; `contains_bidi=0`,
  `root_path_count=1`, `file_url_count=0`.
- previous-after-Escape repro: compile/run exit 0;
  `previous_after_clear=2/2 wrapped=1`.
- tab/status/stale-link repro: final compile/run exit 0; tab-one status and
  accessible name are `Match 1 of 2`, while confirmed/spawned stale targets
  are empty. One preceding supplemental compile exited 1 because the manual
  command omitted the repository's `-mno-direct-extern-access`; the corrected
  command used that flag and its temporary-output root was the assigned build
  directory.

Static gates on the candidate tree:

- `./tools/validate-docs`: exit 0, 128 Markdown documents/navigation entries.
- strict MkDocs to the assigned site directory: exit 0.
- `./tools/check-source-shape`: exit 0, 2,074 files; four unrelated existing
  decomposition warnings, no changed Terminal warning.
- `git diff --check`: exit 0.
- No JSON changed; the JSON parser gate is not applicable.

## Bounded caveats

This repair makes no GPU, global-menu export, whole-application assistive
technology, nested-session screenshot, physical display/input, network, host
bus, or real desktop-opener claim. Product tests use injected fakes or a real
private PTY child only; no nested compositor, host service, hardware, uinput,
or real `xdg-open` was exercised. The confined qtermwidget and application-owned
PTY boundaries remain unchanged.

Requested next action: independent exact review of
`e030a42d1440591b73685ac464912e3c500dafdd`, then manager integration.
