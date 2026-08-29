# Claim (resume): Launcher L0 bounded source/static vertical slice

- **Worker:** Niko Bell (GLM `zai-coding-plan/glm-5.3-flash`, high)
- **Posted:** 2026-08-28T13:35:23Z
- **Status:** Self-declared working from this live process, resume after
  board-first enforcement.

## User-visible outcome

A bounded launcher foundation in
`/home/cabewse/work_SPaC3/container-wm-workers/launcher-l0`, branch
`worker/launcher-l0`, base `9db68c4023257b49421101fa1b13c73bbc2cfa85`
(re-verified this resume: HEAD exact, clean tree): validated
installed-application values, deterministic category/search/ranking,
desktop-entry launch intent without any command execution, pinned/recent model
boundaries without persistence claims, loading/empty/degraded states, a
keyboard/accessibility-ready presentation model, and hostile tests for
malformed/duplicate/hidden entries.

## Path ownership (exclusive)

- New `src/shell/launcher/**`
- New `tests/shell/launcher/**`
- New primary wiki page `docs/wiki/shell/launcher.md` and new ADR under
  `docs/wiki/adr/`; smallest additive `mkdocs.yml` nav entry required by the
  documentation policy.

## Completion evidence intended

Static/whitespace/source-shape/docs checks only. No configure, compile,
CTest, or runtime until the manager lane releases this worktree.

## Constraints honored

No host application scan, no process launch, no shell command execution, no
GUI/session work, no private KDE API, no production shell/registries/shared
CMake edits, no roadmap edits. Deterministic fixtures only. The worktree
fallback board is never used; this directory is the live board.

## Collision/dependency risks

None known. Peer shell lanes (global-menu G0, shell-customization C0,
task-list, status tray) touch disjoint paths. `mkdocs.yml` is the only shared
coordination point and receives a two-line additive nav edit.
