# Micah Stone Terminal S0 source/static candidate handoff

- Time: 2026-08-28T13:48:30Z
- Exact commit: `a15a5f24c6075fe855ac263739fde59dc008e122`
- Tree: `20c720ab5c17e3e64395627406c3f37f4a311c29`
- Parent/base: `9db68c4023257b49421101fa1b13c73bbc2cfa85` (exact required base)
- Branch/worktree: `worker/terminal-s0` at
  `/home/cabewse/work_SPaC3/container-wm-workers/terminal-s0`
- Worktree: clean (0 modified paths)
- Exact sorted name-status manifest SHA-256:
  `ce125927a2cba411ff0aef11dde61a97a9f6a15b44fa7aff73e3bac43e837040`
- Verdict: source/static checkpoint only; **not** acceptance, not executable
  evidence, no milestone claim.

## Manifest summary

35 paths: 28 added (28 under `src/apps/terminal/**`, `tests/apps/terminal/**`,
`docs/wiki/apps/terminal.md`, ADR-0028) and 7 minimal-additive registry/shared
edits (both CMake registries, mkdocs nav, wiki index, ADR index, one
module-boundaries row plus one dependency paragraph, and `qtermwidget` added to
the two Arch CI pacman lists and two version-record lines). No other worker's
source, test, or private boundary was touched.

## Exact static evidence (all exited 0 on the committed content)

- `python3 tools/check-source-shape` — 1027 files checked
- `python3 tools/validate-docs` — 65 documents
- `uvx --from mkdocs mkdocs build --strict`
- `git diff --cached --check` before commit and `git diff --check` after

The full repo build/test suite was **not** run: the serialized compiler lane
(Anika/Devika) is closed and the lane contract forbids compiling, launching a
shell/PTY/UI, or entering host session/display/input. Zero executable claims
are made; the commit body records the same boundary.

## What the candidate delivers

- ADR-0028 (Proposed): `qtermwidget6` 2.4.x as the terminal-scoped
  VT/rendering dependency, confined to one adapter translation unit, with the
  audited upstream teletype contract. CI gains exactly `qtermwidget` in both
  Arch lanes. ADR numbering note: 0026/0027 are consumed by parallel workers
  (0027 = AppShell per board), so this slice takes 0028.
- Launch policy: argv-only shell resolution (`--shell` → `$SHELL` →
  `/bin/bash`), hostile program/argument/environment rejection, forced
  `TERM=xterm-256color`/`COLORTERM=truecolor`, UTF-8 locale fallback,
  positional-argument CLI rejection (exit 2), view-extent clamping.
- Session lifecycle: one PTY generation per session, application-owned
  fork/execve child (real exit-code/signal truth), typed exit publication,
  restart as fresh generation, bounded master-close → SIGTERM → SIGKILL
  escalation to the revalidated process-group leader, honest
  ShutdownFailed for an unkillable child, close-blocks-quit teardown
  guarantee.
- Presentation: stable action identities with Shift-modified terminal-safe
  shortcuts (plain readline sequences deliberately unbound), exit severity
  rendering with QST colors, focus/accessibility metadata, `Ctrl+Shift+*`
  keyboard semantics; QST-1 appearance adapter and 16-slot scheme documents
  from public tokens (no hex, no second theme authority).
- Tests (`^qindaqt\.terminal-`, 8 rows) plus wiki
  `docs/wiki/apps/terminal.md` with honest deferrals: tabs, profiles, search,
  links, GPU, advanced VT, live lane work.

## Requested next actions

1. Independent exact-commit source review of `a15a5f2` (reviewer per manager
   routing; I make no routing claim).
2. On the serialized compiler lane: configure with `qtermwidget` present,
   build, and run `ctest --test-dir build/dev -R '^qindaqt\.terminal-'
   --output-on-failure` plus the standing gates. My new CMake/test rows are
   designed to fail loudly (QVERIFY/typed outcomes), not silently skip.
3. If review finds blocking defects, I repair in this same worktree as a
   non-amended descendant and request rereview of that exact commit.

No acceptance, product progress, model, test-pass, or process claim beyond
this message is made.
