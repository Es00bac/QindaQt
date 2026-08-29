# Launcher L0 exact repair handoff

- **Worker:** Robin Sayeed (OpenAI collaboration runtime; exact serving
  model/reasoning unexposed)
- **Posted:** 2026-08-28T10:29:48-06:00
- **Status:** Finished; exact rereview requested from Franklin Okafor

## Exact candidate

- **Commit:** `a5a6b19c454dc8ea86e4c10ac3ef180468beed1f`
- **Tree:** `ad81ececbe184007d441782a518ef3de1830d356`
- **First parent / immutable rejected candidate:**
  `7c68618667627c3e3dfa7417c13ef47c135e7667`
- **Second parent / current public main:**
  `ab36cd8d71876bc0c68f9f50d252ab04f234ba5c`
- **Branch/worktree:** `worker/launcher-l0` at
  `/home/cabewse/work_SPaC3/container-wm-workers/launcher-l0`
- **Working tree:** clean

The merge parent is intentional: it preserves the rejected candidate without
amending it and makes current public main a direct ancestor, so AppShell, File
Manager, Flow, power/brightness, and contained virtual-desktop work cannot be
overwritten during integration.

## Changed paths relative to current public main

Exactly 31 paths, all Launcher-owned plus six additive coordination points:

- `src/shell/launcher/**` — bounded parser/catalog/category/search,
  pinned/recent, presentation, intents, public headers, and module registration.
- `tests/shell/launcher/**` — six compiled suites and literal hostile fixtures.
- `src/CMakeLists.txt`, `tests/CMakeLists.txt` — one additive Launcher
  registration each.
- `docs/wiki/shell/launcher.md`,
  `docs/wiki/adr/0042-launcher-model-without-execution.md`,
  `docs/wiki/adr/index.md`, `docs/wiki/architecture/module-boundaries.md`,
  `docs/wiki/index.md`, and `mkdocs.yml` — current contract, unique ADR, and
  validated navigation.

`git diff --stat HEAD^2..HEAD` reports 31 files and 3,155 insertions. Public
main supplies every other path in the merge candidate unchanged.

## Closed exact findings

- Both focused and repository-root CMake paths now resolve and register all six
  suites; the non-void QtTest helper and aggregate-macro compile failures are
  removed.
- The catalog claims each bounded valid source ID before parsing or visibility
  disposition, bounds its reserve and diagnostic identity, and regression-tests
  hidden-first, NoDisplay-first, invalid-first, and visible-first precedence.
- Recognized desktop-entry grammar now rejects duplicate groups/keys, malformed
  or duplicate action IDs, and non-boolean flag values; accepts whitespace
  around `=` and escaped semicolons in lists; and never decodes unknown
  key/group payloads.
- Blank display names and hostile pinned/recent identities fail closed. The
  QString input ceiling is explicitly named in UTF-16 code units.
- Valid no-match search still publishes one SearchResults section; accessible
  descriptions have a deterministic non-empty fallback; section/category
  identities leave localized strings to the future UI adapter.
- The vacuous keyboard/pointer assertion is replaced by direct confined-intent
  and action icon-fallback evidence. The category precedence guard and direct
  `<algorithm>` dependency are corrected; the dead hidden diagnostic is gone.
- Launcher is ADR-0042. Accepted ADR-0026, ADR-0027, ADR-0029, and ADR-0041 and
  all public navigation/architecture entries remain present.

## Verification evidence

- Focused standalone configure and strict serial build: exit 0.
- Repository-root configure and strict serial build of the Launcher library and
  six test targets: exit 0, no Launcher warnings.
- Root CTest selector `qindaqt[.]launcher-`: 6/6 rows pass.
- Direct QtTest counts: parser 23, catalog 13, category 7, search 11,
  pinned/recent 11, presentation 12 — **77 passed, 0 failed**.
- `python3 tools/validate-docs`: exit 0; 74 Markdown pages plus navigation.
- `python3 tools/check-source-shape`: exit 0; 1,121 sources checked.
- `mkdocs build --strict`: exit 0.
- `git diff --check HEAD^2..HEAD`: exit 0.
- `git merge-tree --write-tree HEAD origin/main`: exit 0 and produced the exact
  candidate tree `ad81ececbe184007d441782a518ef3de1830d356`; no conflicts.
- No host GUI, desktop session, bus, input, filesystem catalog, process launch,
  or configuration was touched.

## Bounded remaining boundary

Installed-application scanning, Settings1 persistence, launch execution, QML
presentation, localization strings, and real keyboard/pointer adapters remain
later milestones. L0 intentionally provides only their bounded pure model and
intent boundary.

## Requested next action

Franklin Okafor: rereview exact commit
`a5a6b19c454dc8ea86e4c10ac3ef180468beed1f` and its two-parent provenance.
Attack every item in your `0/8/9/4` verdict and return an exact PASS/FAIL with
counts. Manager: integrate only after Franklin's exact PASS.
