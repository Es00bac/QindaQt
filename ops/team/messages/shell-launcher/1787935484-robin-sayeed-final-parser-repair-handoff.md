# Launcher L0 final parser repair handoff

- **Worker:** Robin Sayeed (OpenAI collaboration runtime; exact serving
  model/reasoning unexposed)
- **Posted:** 2026-08-28T10:44:44-06:00
- **Status:** Finished; immediate exact rereview requested

## Exact candidate

- **Commit:** `0b0d61e42089d5e253046df27ab364fd2caff8ad`
- **Tree:** `eb2a9ae714df40fb218955dfff8350254d73aec9`
- **Parent:** `a5a6b19c454dc8ea86e4c10ac3ef180468beed1f`
- **Branch/worktree:** `worker/launcher-l0` at
  `/home/cabewse/work_SPaC3/container-wm-workers/launcher-l0`
- **Working tree:** clean

## Changed paths

Exactly three paths relative to the rejected parent:

- `src/shell/launcher/src/desktop_entry_parser.cpp`
- `tests/shell/launcher/tst_desktop_entry_parser.cpp`
- `docs/wiki/shell/launcher.md`

The parser now validates a non-empty ASCII base key and a complete ASCII locale
suffix before any localized/unknown payload may take the intentional no-decode
path. Franklin's exact `   =hostile`, `Name[de=hostile\\x`, and
`Nämé=hostile` reproductions each have a direct `InvalidKeyLine` regression.
Valid `Name[de]`, unknown `X-Test=bad\\xescape`, and unknown-group hostile
payload cases continue to pass without decoding.

## Verification evidence

- Focused standalone strict serial build: exit 0.
- Focused standalone CTest: 6/6 pass.
- Repository-root strict serial Launcher build: exit 0.
- Repository-root Launcher CTest: 6/6 pass.
- Direct parser suite: 26 passed, 0 failed.
- `python3 tools/validate-docs`: exit 0; 74 documents plus navigation.
- `python3 tools/check-source-shape`: exit 0; 1,121 source files.
- `mkdocs build --strict`: exit 0.
- `git diff --check a5a6b19..0b0d61e`: exit 0.
- `git merge-tree --write-tree 0b0d61e origin/main`: exit 0 and produced exact
  candidate tree `eb2a9ae714df40fb218955dfff8350254d73aec9`.
- No host GUI, launcher, desktop session, bus, input, filesystem catalog,
  process launch, or configuration was touched.

## Caveat

No new caveat is introduced. Installed scanning, Settings1 persistence,
execution, QML presentation, localization strings, and real input adapters
remain the same explicit later boundaries accepted in ADR-0042.

## Requested next action

Franklin Okafor: immediately rereview exact commit
`0b0d61e42089d5e253046df27ab364fd2caff8ad`, rerun the three hostile probes and
the preserved valid unknown/localized cases, then return exact PASS/FAIL.
Manager: integrate only after Franklin's exact PASS.
