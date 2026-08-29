# Juno Park AppShell S0 exact-checkpoint source-review claim

- Time: 2026-08-28T12:58:20Z
- Reviewer: Juno Park (permanent QindaQt Native Applications Design Engineer)
- Addressee: Anika Rao; manager
- Exact checkpoint: `de52a04966763cc11f8a551c58bd76ca38694c5c`
- Tree: `c5a9e591314d4f3cd755a6595ca949f6ff0dc85c`
- Parent/base: `1b4e2846e40d31d79ffb03db2229c07ff9bca271`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/appshell-s0-review-juno`
- Worktree state verified: HEAD/tree/parent match Anika's `1787921583`
  manifest exactly; `git status --porcelain` empty.

## Scope

Early repair-partner source review before Anika's final compiler replay; not
qualification and not acceptance. I audit, at source level only:

- action/menu registry semantics (register, duplicate, unknown actions);
- lifecycle and degraded states, async ownership/lifetime/error behavior;
- QML registration and installed-consumer isolation;
- QST/Controls-only presentation (no framework leakage);
- focus/keyboard/accessibility semantics, including native QWindow identity
  and item-level accessible application name;
- responsive behavior, module dependency direction, API compatibility,
  package surface;
- tests for non-vacuous hostile inputs and correct identity/a11y assertions.

## Method

Read AGENTS.md and the full wiki entry path (ADR-0027,
`apps/application-shell.md`, `architecture/module-boundaries.md`, QST/Controls
pages), the exact parent→checkpoint diff, every file under `tests/app_shell`,
and the live thread. No product edits, no Git mutation, no compilers, no
product UI, no host-state contact; the product worktree is read-only.

## Deliverable

Concrete P0–P3 findings with file/line evidence and exact reproduction/test
additions, routed directly to Anika and the manager, plus a source-checkpoint
advice handoff. Final-commit review after the five-row build passes remains
required; this review does not qualify the checkpoint.
