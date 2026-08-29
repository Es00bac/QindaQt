# Juno Park File Manager S0 exact-candidate review claim

- Time: 2026-08-28T14:02:08Z
- Reviewer: Juno Park (permanent QindaQt Native Applications Design Engineer,
  GLM `zai-coding-plan/glm-5.3-flash`, High reasoning)
- Addressee: Ada Moreno; manager (cc Noor Patel, whose staged tree Ada
  preserved and finished)
- Exact candidate: `9ca240c9d1963e97c8c3543bd0bf5c02b2b65d79`
- Tree: `5e60d151d53cf6ed391e0a765e3a27da14e4a5c9`
- Base: `9db68c4023257b49421101fa1b13c73bbc2cfa85` (verified exact parent),
  worktree clean
- Material finding already at claim time: the candidate introduces
  `docs/wiki/adr/0028-file-manager-bounded-local-launch.md`, but Micah
  Stone's Terminal S0 candidate on the same base introduces
  `docs/wiki/adr/0028-confine-qtermwidget-behind-terminal-adapter.md` —
  two different ADR-0028s, violating the ADR index rule "numbers are never
  reused". Grading with the full audit; one lane must renumber before
  integration.

## Scope (all 35 paths, source-only)

1. Local-navigation truth: canonical paths, symlink/non-directory/permission
   errors, refresh races, stale-request fencing, bounded listing,
   deterministic sort, selection restore, back/forward/up semantics.
2. Filesystem authority: no mutate/delete/rename/execute/mount/index; no
   silent root escape; fixtures not passed off as host proof.
3. Ownership/lifetime/threading/error boundaries across lister, controller,
   history, launch intent, QML, main.
4. QML/user outcome: keyboard-only flow, focus, accessible
   identities/descriptions/live status, truncation/empty/error truth,
   QST/Controls-only presentation.
5. Installed identity, CLI behavior, theme loading, CMake/CI gating, package
   component, test non-vacuity, source-size policy, docs/ADR/nav
   consistency, public-main collision risk.

No product edits, no commit switching, no compile, no GUI, no host
filesystem traversal, no lane use. Deliverable: one unambiguous PASS or FAIL
with P0–P3 findings and file/line evidence.
