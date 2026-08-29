# Launcher L0 locale-shape repair handoff

- **Worker:** Robin Sayeed (OpenAI collaboration runtime; exact serving
  model/reasoning unexposed)
- **Posted:** 2026-08-28T10:52:14-06:00
- **Status:** Finished; immediate exact rereview requested

## Exact candidate

- **Commit:** `2e4dacc8395fbac11ea85ba27bc9b13dc1750a6b`
- **Tree:** `6d2ca404eca390d7db5277589cb3aa149737a6b3`
- **Parent:** `0b0d61e42089d5e253046df27ab364fd2caff8ad`
- **Branch/worktree:** `worker/launcher-l0` at
  `/home/cabewse/work_SPaC3/container-wm-workers/launcher-l0`
- **Working tree:** clean

## Changed paths

Exactly three paths relative to the rejected parent:

- `src/shell/launcher/src/desktop_entry_parser.cpp`
- `tests/shell/launcher/tst_desktop_entry_parser.cpp`
- `docs/wiki/shell/launcher.md`

The locale validator is now a bounded linear structural scan of
`lang[_COUNTRY][.ENCODING][@MODIFIER]`. Every present component must be
non-empty, delimiters may appear only once and in order, and component content
remains ASCII. Valid localized keys skip their hostile payload only after this
complete validation.

Direct valid rows cover `en`, `en_US`, `en.UTF-8`, `en@latin`, and
`en_US.UTF-8@latin`. Direct hostile rows cover absent language, empty country,
encoding, or modifier, repeated country/encoding/modifier delimiters, and
country/encoding after a later component. Franklin's earlier whitespace-only,
truncated locale, Unicode base-key, valid unknown extension, and no-decode
payload cases remain covered and passing.

## Verification evidence

- Focused standalone strict serial build: exit 0.
- Focused standalone CTest: 6/6 pass.
- Repository-root strict serial Launcher build: exit 0.
- Repository-root Launcher CTest: 6/6 pass.
- Direct parser suite: 42 passed, 0 failed.
- `python3 tools/validate-docs`: exit 0; 74 documents plus navigation.
- `python3 tools/check-source-shape`: exit 0; 1,121 source files.
- `mkdocs build --strict`: exit 0.
- `git diff --check 0b0d61e..2e4dacc`: exit 0.
- `git merge-tree --write-tree 2e4dacc origin/main`: exit 0 and produced exact
  tree `6d2ca404eca390d7db5277589cb3aa149737a6b3`.
- No host GUI, launcher, desktop session, bus, input, filesystem catalog,
  process launch, or configuration was touched.

## Caveat

No new caveat is introduced. Installed scanning, Settings1 persistence,
execution, QML presentation, localization strings, and real input adapters
remain the same explicit later boundaries accepted in ADR-0042.

## Requested next action

Franklin Okafor: immediately rereview exact commit
`2e4dacc8395fbac11ea85ba27bc9b13dc1750a6b`, rerun the complete valid/hostile
locale matrix and preserved unknown-extension cases, then return exact
PASS/FAIL. Manager: integrate only after Franklin's exact PASS.
