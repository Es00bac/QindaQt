# Exact-candidate review verdict — production shell runtime repair

- Reviewer persona: **Tu Youyou**, independent shell reviewer
- Provider/model: Moonshot Kimi `kimi-code/k3`
- Candidate SHA: `99de545aa521c4d5428b72ee491268e401a5b84c` (`worker/shell-production-runtime-repair`, implementer Ada Yonath, OpenAI Codex)
- Tree SHA: `de8b32ee719b7f6509aa2ec172df40e8bac8b7af`
- Parent SHA: `dd415f48f3a12ab95db9ca26b7227a1e0bcf69af`
- Base SHA used for the product diff: `dd415f48f3a12ab95db9ca26b7227a1e0bcf69af` (see P3-3)
- Review worktree: `/home/cabewse/work_SPaC3/container-wm-workers/shell-production-runtime-repair-k3-review` (never modified; `git status --porcelain` empty and HEAD exact before and after all work)
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-runtime-repair-k3`
- Implementer handoff read: `ops/team/messages/shell-production-surface/1788566021-ada-yonath-handoff.md`
- Grounding read: root `AGENTS.md`, `module-boundaries.md`, `coding-practices.md`, and every owning wiki page touched by the diff (`design-tokens.md`, `applet-runtime.md`, `controls.md`, `panel-surfaces.md`, `task-list.md`, `global-menu.md`, `testing-harness.md`, `compositor-control-v1.md`, ADR-0071, `mkdocs.yml`)

The product diff is the full parent-to-candidate diff: 51 files, +1224/-355, matching the handoff's changed-paths list exactly.

## Findings ledger

### P0 — none

No destructive, host-affecting, bus-affecting, hardware, or network behavior. The new `ShellTokenPublisher` is process-local QML singleton publication; the PTY slave guard is a private `O_NOCTTY | O_CLOEXEC` descriptor; no host surface is touched. All test rows ran with display variables unset and both D-Bus addresses pointed at `unix:path=/nonexistent`.

### P1 — none

Every claimed outcome was reproduced or code-verified:

1. **Token publication (Q1).** `ShellTokenPublisher` is constructed and started before any panel/hosted-applet QML in both composition roots: production `initializeRuntime` calls `initializeTokens` (`src/shell/runtime/shellruntimeapplication.cpp:237`) before the launcher runtime, service applet compositions, and the window factory; preview `run()` starts the publisher before `loadWindow` (`src/shell/app/shellpreviewapplication.cpp:45-58`). Initial failure is fatal (return 4 / `resetRuntime` + exit); later republication failure emits `publicationFailed` → `QCoreApplication::exit(4)` in both executables — fail closed. Republish on `ThemeCatalog::currentChanged` is atomic: `TokenFacade::publish` replaces the complete cached map and bumps one monotonic generation before a single aggregate `tokensChanged` (`src/design_tokens/src/token_facade.cpp:66-94`, AGENT-GUARD at :109-113). Attack surface checked: theme switch mid-render is a synchronous GUI-thread publish; missing/empty selection fails closed in `publishSelected` (`shelltokenpublisher.cpp:107-118`); neither shell recreates its engine, and a 5 s bounded await in `ensureFacade` prevents a startup hang on a stuck async registration. `qindaqt.shell-capture-matrix` is now warning-fatal across the complete dispatcher (`tests/shell/tst_shell_capture.cpp:57`, asserts normal exit 0 plus a valid PNG of the required size, 4 rows including the `qinda-macos` theme) — non-vacuous. The `ContainerTabStrip.qml` `required property int index` fix is correct delegate-role hygiene.
2. **`ShellDevelopment1.Snapshot` tokens fact (Q2).** Exactly five fields emitted (`src/shell/runtime/shelldevelopmentevidence.cpp:265-279`); snapshot construction happens only after successful runtime init and PID-authenticated registration; the sampler calls the unique owner and re-proves name ownership after Snapshot (splice guard retained in the extracted `tests/session/desktopnotificationshellsample.cpp`). Hostile mutations rejected by both consumers: C++ validator `validReadyTokens` (exact size 5, `ready:true`, `qstRevision==1`, canonical positive generation, non-empty theme, lowercase canonical `#rrggbb`) with dedicated mutation rows in `tests/session/tst_desktopnotificationshellreadiness.cpp` (including `#ABCDEF` and `"0"` generation); both Python consumers (`desktop_session_interactive.py:84-100`, `desktop_session_notification_shell.py:127-144`) enforce the identical contract. The boot-row validator rejects an unready shell: `desktop_session_readiness.py:19,169` routes probe evidence through `_validate_evidence`.
3. **Terminal (Q3).** The slave-guard fix (`src/apps/terminal/session/pty_bridge.cpp:61-76`) holds a close-on-exec, non-controlling slave until `closeChildChannel`, eliminating the pre-child EIO gap and the post-exit POLLHUP hot loop; reap remains exit authority. The attach-before-start ordering (`terminal_session.cpp:193-202`) publishes the widget synchronously before `start()`, disposes it on start failure (`viewDisposalRequested`), primes the teletype, and delivers the attached resize synchronously (`terminal_widget_adapter.cpp:259-264`); `main()` realizes the top-level before scheduling the first session (`src/apps/terminal/main.cpp:296-303`). The offscreen pixel row is a real control: it grabs only qtermwidget's pixels, excludes the pristine-cursor cell, and requires >100 foreground-like pixels from real child output. Session teardown (process-group-complete), scrollback, and search rows all pass in both configurations (selector A below).
4. **Documented non-fixes (Q4).** `task-list.md`'s **Limited** statement and `global-menu.md`'s **Menu unavailable** statement are accurate and minimal; no task-list or global-menu product code is in the diff, so nothing was papered over. ADR-0071 matches the implementation; `panel-surfaces.md` startup ordering now lists token publication as step 2, matching `initializeRuntime`.

### P2 — none

No defect with a needed workaround and no missing negative control in the candidate's surface. The token-publication base negative is structurally compile-time (see P3-4) and the brief's fallback negative was executed exactly.

### P3 — four nonblocking precision items

- **P3-1.** The handoff says `qindaqt.shell-runtime-token-publication` asserts "QST revision/theme". `tests/shell/tst_shellruntime_tokens.cpp` asserts `Tokens.ready`, `sourceThemeId`, `bg.base`, a hosted Controls label's muted foreground, and generation-monotonic republish; it does **not** assert `qstRevision` (that is asserted as `==1` by the C++ and Python snapshot validators instead). Product docs (`testing-harness.md`) do not overclaim; only the handoff sentence is imprecise.
- **P3-2.** The handoff's "Debug 131/131, Release 131/131" for its selector is not verbatim-reproducible under the brief's exact display-less environment: 6 rows in selector A abort in xcb platform-plugin initialization (`qindaqt.clipboard-applet-{controller,fencing,admission,snapshot-invariants,seam}` and `qindaqt.shell-runtime-component-closure`). Proven pre-existing, not a candidate regression: the same 5 clipboard rows abort identically at base `dd415f48` (control run, see below), every involved file is outside the 51-file diff, and all 6 pass with `QT_QPA_PLATFORM=offscreen` added. The implementer's 131/131 was most likely recorded with an inherited offscreen platform variable. Recommend a separate, non-candidate harness fix (pin `QT_QPA_PLATFORM=offscreen` in those rows' ctest ENVIRONMENT).
- **P3-3.** Brief metadata mismatch: the brief names base `37f8523e` and "52 files". The candidate's actual parent and the implementer's stated exact base is `dd415f48`; `37f8523e` is a sibling manager commit ("Withdraw install readiness…") whose diff to the candidate adds only manager bookkeeping (`docs/HANDOFF.md`, `docs/TASK_LIST.md`, `ops/team/workers/claude-program-manager.md`). The product diff reviewed is the 51-file parent diff, identical to the handoff's changed-paths list.
- **P3-4.** A direct base run of `qindaqt.shell-runtime-token-publication` is impossible by construction: `src/shell/common/shelltokenpublisher.{h,cpp}` do not exist at base, so CMake generate fails with "Cannot find source file" and a syntax-only probe confirms the header's absence. The prescribed indirect negative was executed instead and confirmed exactly (below).

## Commands executed and results

Environment for every ctest/direct run: `DISPLAY`, `WAYLAND_DISPLAY`, `XDG_SESSION_TYPE`, `XDG_CURRENT_DESKTOP` unset; `DBUS_SYSTEM_BUS_ADDRESS=DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent`. No nested-compositor, windowed, host-service, uinput, hardware, or network rows were run.

### Candidate configure and build (worktree at 99de545a, read-only)

- Debug configure (exact prescribed recipe, `<ROOT>/debug`): exit 0.
- Release configure (`<ROOT>/release`, `-DCMAKE_BUILD_TYPE=Release`): exit 0.
- Focused targets only (106 targets mapped from the selectors' `CTestTestfile.cmake` entries, including `qindaqt-shell`, `qindaqt-shell-preview`, `qindaqt-terminal`, all selector test binaries, `qindaqt-desktop-session-probe`): `cmake --build <ROOT>/{debug,release} --parallel 12 --target …` → exit 0 in both (2261 steps each). Whole-`all` build was not needed.

### Candidate test rows

- A `^qindaqt\.(shell-runtime-|shell-capture|applet|task-list-|global-menu-|status-notifier-|clipboard-applet-|launcher|notification-center|terminal)` `--output-on-failure --no-tests=error`:
  - Debug: exit 8, **126/132 passed**; Release: exit 8, **126/132 passed**. The same 6 failures in both, all xcb platform aborts, all pre-existing (P3-2). Every candidate-added row — `qindaqt.shell-runtime-token-publication`, `qindaqt.shell-runtime-catalog`, all `qindaqt.terminal-*` (including `terminal-pty-bridge`, `terminal-session`, `terminal-widget-adapter-offscreen` glyph-pixel, teardown/process-group, scrollback, search), `qindaqt.shell-capture-matrix`, `desktop.virtual.notification-shell-readiness-unit` — passed in both configurations.
  - Diagnostic rerun of only the 6 failing rows in Debug with `QT_QPA_PLATFORM=offscreen` added: **6/6 passed** (confirms environment-only cause).
- B `^desktop\.virtual\.(sandbox-unit|package-contract|stage-closure)`: Debug **3/3** exit 0; Release **3/3** exit 0.
- C `^(desktop\.virtual\.notification-shell-readiness-unit|qindaqt\.shell-capture-matrix)$`: Debug **2/2** exit 0; Release **2/2** exit 0.

### Base negative controls (scratch `git archive dd415f48` under `<ROOT>/base-src`, no git mutation of the review worktree; overlay integrity re-verified afterward)

- Terminal negatives (base product code + candidate test files only): build exit 0; `ctest -R '^qindaqt\.terminal-(pty-bridge|session)$'` → exit 8.
  - `qindaqt.terminal-pty-bridge`: **7 passed / 1 failed**; failing `childOpenGapDoesNotDiscardLaterOutput` at `tst_pty_bridge.cpp:217` (`readNotifier->isEnabled()` FALSE) — the transient-slave EIO gap disables forwarding at base. Matches the handoff exactly.
  - `qindaqt.terminal-session`: **15 passed / 2 failed**; `successfulStartPublishesWidgetAndRunningState` (`tst_terminal_session.cpp:253`, widget published only after start) and `backendStartFailurePublishesDiagnosticWithoutWidget` (`:286`, widgetSpy 0 vs 1). Matches the handoff exactly.
- Token-publication indirect negative via the base's own `qindaqt.shell-capture-matrix` wiring (base `qindaqt-shell-preview`, offscreen):
  - Normal run: exit 0, valid PNG, and exactly **33,768** `Unable to assign [undefined]` occurrences (first hits in `QindaQt/Controls/qml/SectionHeader.qml`).
  - `QT_FATAL_WARNINGS=1` run: **exit 134** (SIGABRT), no PNG; fatal frame `ContainerTabStrip.qml:28 ReferenceError: index is not defined` (first fatal-eligible warning; ordering not guaranteed, exit code is the claim and is confirmed).
  - Base's own capture row passes at base (exit 0), confirming the base harness hid the 33,768 warnings — the candidate's warning-fatal matrix and token test close that gap.
- Pre-existing-failure control: the 5 clipboard-applet rows built and run at base under the prescribed env → **0/5 passed, same "Subprocess aborted" xcb cause** as on the candidate, proving P3-2 is not introduced by this diff.

### Static gates (from the worktree root)

- `./tools/validate-docs`: exit 0 (144 Markdown documents plus navigation).
- `mkdocs build --strict --site-dir <ROOT>/site` (venv `qindaqt-docs-venv`): exit 0.
- `./tools/check-source-shape`: exit 0, 2546 files, 0 allowlisted; 11 decomposition-review warnings, of which the only candidate-diff file warned is `src/shell/CMakeLists.txt` (599 non-blank) — already over threshold at base (580), pre-existing.
- `git diff --check`: exit 0.
- `python3 -m json.tool` on `probe-observed-fallback-1080p.json` and `probe-ready-1080p.json`: exit 0 both.

### Cleanliness

`git status --porcelain` on the review worktree empty and `git rev-parse HEAD` = `99de545aa521c4d5428b72ee491268e401a5b84c` before and after all work. All scratch artifacts confined to `<ROOT>`.

## Verdict

All claimed fixes verified in both configurations; every base negative reproduced exactly; the two documented non-fixes are truthful; no P0/P1/P2 findings. The 6 selector-A failures are a pre-existing, base-reproduced harness/platform gap outside the candidate diff and do not block acceptance of this candidate (recommend a separate harness fix per P3-2).

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/4
