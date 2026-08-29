# Linnea Marsh handoff: native QindaQt Text Editor S1 candidate

- Timestamp: 2026-08-28T05:49:33Z
- Exact candidate: `a7a3c3117130278932ef653caacf670a3899f6fc`
- Exact parent/base: `94e84077e33a279dcebee24511e7dbdf1b87e3e1`
- Branch: `worker/text-editor-s1`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/text-editor-s1`
- Worktree state: clean

## Outcome and changed paths

The candidate delivers the bounded lightweight `qindaqt-editor` vertical
slice: one valid-UTF-8 local document, BOM and dominant line-ending round trip,
incremental exact dirty truth, watched external byte revisions, optimistic
conflict refusal, atomic `QSaveFile` persistence, explicit replacement consent,
standard menus/actions/shortcuts, keyboard/accessibility behavior, installed
desktop/theme payload, and public QST-1-only styling.

Owned changes are exactly:

- `src/apps/text_editor/**` — document value/store/controller, Qt Widgets
  presentation, QST-1 adapter, CLI, desktop metadata, and focused install
  component
- `tests/apps/text_editor/**` — five focused C++ binaries plus metadata, CLI,
  staged-install, five-theme, startup/PSS, and 8 MiB rows
- `docs/wiki/apps/text-editor.md` and
  `docs/wiki/adr/0022-keep-text-documents-local-and-atomic.md`
- minimal additive entries in `src/CMakeLists.txt`, `tests/CMakeLists.txt`,
  `mkdocs.yml`, wiki index, ADR index, module boundaries, and roadmap

No file-manager, terminal, Settings, Controls, shell, service, compositor, or
general AppShell source/API was added or changed.

## Rowan and Juno finding ledger

Every original should-fix is closed:

- Rowan SF-1/SF-5: full-widget text copies were replaced with QTextDocument
  UTF-16 edit deltas, cached dirty truth with a length fast path, and a dedicated
  measured 8 MiB open/edit/atomic-save row.
- Rowan SF-2: controller tests now use `QTEST_GUILESS_MAIN`, matching its
  QCoreApplication/GUI-thread QObject contract.
- Rowan SF-3: same-path Save As uses the same CreateOnly-then-explicit-
  ReplaceExisting policy as every other destination, with an external-byte
  preservation/consent test.
- Rowan SF-4 / Juno SF-J2: Changed consumes QST warning colors; Missing and
  Unreadable consume danger colors; `Warning:`/`Error:` prefixes and distinct
  truthful status lines make severity independent of color.
- Juno SF-J1: assertive accessibility announcements are driven only by real
  external-state transitions; a QAccessible capture proves dirty flips and
  repeated refreshes do not re-announce.

Bounded notes taken now:

- Rowan N-1: actions are retained by typed window-owned pointers; all eleven
  identities/standard shortcuts/window contexts are asserted and their
  additive compatibility rule is documented.
- Rowan N-2: dead appearance fields were removed, BrightText was corrected,
  and the intentionally shared Link/LinkVisited semantic is explained.
- Juno NF-J3/NF-J5/NF-J6/NF-J7/NF-J8: banner-hide focus recovery,
  plain-language replace copy, all-five-theme adapter and installed loops,
  explicit high-contrast derivation input, and extension-neutral dialogs all
  landed with tests/docs.
- Rowan NF-R1 / Juno NJ-2: the rendered external state is cached so dirty flips
  do not reapply the banner stylesheet/text.
- Juno NJ-1: Unreadable now has an explicit danger-style assertion as well as
  text/status assertions.

Bounded deferrals are explicit and non-blocking: Rowan N-3/NF-R3 and Juno
NF-J4/NJ-3 keep Tab as editor input while Alt mnemonics and reverse traversal
reach recovery controls until a desktop-wide forward-focus convention exists;
Rowan N-4 keeps native modal presentation inside the window until a second app
proves an injectable shared seam; Rowan N-5 keeps the platform text-editor icon
until a branding slice; Rowan NF-R2 records the natural banner split before the
447-line window source reaches the 500-nonblank-line review threshold; Juno
NJ-4 caret-blink pinning plus screenshot/live-AT/nested-session rows remain
future harness work. Rowan's and Juno's repaired-tree rereviews each returned
no blocking finding before commit; a different worker must still review this
exact hash.

## Exact candidate evidence

Configuration used strict warnings with testing/shared libraries enabled and
KWin plugin, shell, production shell, and host-uinput rows disabled. Every
build invocation was `--parallel 1` after a fresh headroom check and used the
worktree-local `build/text-editor-tmp` because host `/tmp` was 92% full.

- Explicit `qindaqt-editor` plus five C++ test targets, Debug: exit 0.
- Explicit `qindaqt-editor` plus five C++ test targets, Release: exit 0.
- `ctest --test-dir build/text-editor-debug --parallel 1 -R
  '^qindaqt\.editor-' --output-on-failure`: 8/8 passed, exit 0.
- Same focused selector in Release: 8/8 passed, exit 0.
- Public dependency rows `theme-formats`, `design-tokens-derivation`,
  `design-tokens-built-in-contrast`, and `design-tokens-benchmark`: Debug 4/4
  and Release 4/4, both exit 0.
- Debug 8 MiB row: open 285 ms, incremental edit 2 ms, atomic save 118 ms;
  limits are 5,000/500/5,000 ms.
- Clean staged `TextEditor` component: all five built-ins returned exact
  `<id> qst-1`; installed desktop payload passed; first painted frame 266 ms;
  PSS samples `[19385, 19515, 19456, 19514, 19511]` KiB, median 19,511 KiB;
  hard limits are 400 ms and 65,536 KiB. Exit 0.
- Exact-commit `git diff <base> HEAD --check`: exit 0; worktree clean.
- `clang-format --dry-run --Werror` across owned C++: exit 0.
- `tools/check-source-shape`: exit 0, 855 files, zero allowlisted.
- `tools/validate-docs`: exit 0, 49 Markdown documents/navigation valid.
- `uvx --from mkdocs mkdocs build --strict`: exit 0.
- installed-probe Python syntax, source desktop validator, and private/sibling
  dependency scan: exit 0 / zero matches.

All GUI rows used `QT_QPA_PLATFORM=offscreen`. No host GUI/input, private
nested runtime, compositor session, or private app/framework dependency was
used. An accidental default whole-repository build was stopped after it entered
unrelated session/service targets and is deliberately not claimed as evidence;
the exact explicit editor targets and additive root configuration are green.
ADR-0022 truthfully retains the bounded compare-to-rename race.

## Requested next action

Have a different worker inspect and run the exact candidate
`a7a3c3117130278932ef653caacf670a3899f6fc`. If accepted, the manager should
integrate that hash, rerun the focused 8-row editor matrix plus the four
theme/QST dependency rows on the integrated tree, update task/handoff truth,
and preserve this commit before retiring the worktree.
