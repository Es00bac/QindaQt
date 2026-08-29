# Noether the 4th passed the exact Settings S1 repair descendant

- Timestamp: 2026-08-28T16:04:43-06:00
- State: terminal review PASS
- Exact candidate: `95ebbd476acad4c3b40c67226e7298091527f0f5`
- Exact tree: `5c02f21a4f8d6d3267146048abf09c55a8ef6458`
- Exact parent/rejected candidate: `7e6f133e280920f98fcb0ea79385d496b7871bd6`
- P0/P1/P2/P3: `0/0/0/0`
- Detached review worktree: `/mnt/d/QindaQt/reviews/settings-s1-noether`

## Verdict

**PASS.** This verdict accepts only the immutable repair descendant identified
above. The original candidate `7e6f133e280920f98fcb0ea79385d496b7871bd6`
remains rejected with its prior `0/1/2/0` findings.

## Prior-finding disposition

| Finding | Rereview result | Independent evidence |
| --- | --- | --- |
| P1-1 installed package could borrow build-tree QML | Closed | Runtime authority now compares the canonical executable path to the exact compiled build executable. With installed Appearance removed while build QML remained, both an install beneath the build root and a sibling-prefix install exited 3; restored modules were then proven usable. |
| P2-1 unavailable reason absent from wide and compact navigation | Closed | Both presentations expose the exact registered reason in their accessible descriptions. Candidate direct-page tests pass 6/6 under `QT_FATAL_WARNINGS=1`; an external review-only harness passes 5/5. |
| P2-2 Escape could not focus the active unavailable route | Closed | Wide and compact active unavailable routes remain focusable while activation is guarded. Escape returns focus in both presentations, and pointer activation does not mutate the controller route. Candidate and external mutation-sensitive tests pass. |

## Exact evidence

- Fresh strict Debug build and focused selector: 9/9 pass.
- Fresh strict Release build and focused selector: 9/9 pass.
- Direct compiled, token-published navigation page under
  `QT_FATAL_WARNINGS=1`: 6/6 pass with no warning abort.
- External review-only wide/compact reason, focus, and activation harness: 5/5
  pass.
- Independent package poison contrast: in-build-root relocated install exit 3;
  outside-build-root relocated install exit 3; both staged modules restored.
- `./tools/validate-docs`: 92 documents pass.
- `./tools/check-source-shape`: 1,357 source files pass with 0 skips.
- `mkdocs build --strict`: pass.
- Exact lineage and tree identity, full/repaired `git diff --check`, ownership
  provenance, and forbidden-path audit: pass.
- Final `git status --short`: empty. Candidate tree remained byte-clean.

Detailed logs are under
`/mnt/d/QindaQt/builds/settings-s1-noether/`, including
`rereview-debug-focused.log`, `rereview-release-focused.log`, and
`rereview-static.log`.

## Requested next action

The Program Manager may integrate exact commit
`95ebbd476acad4c3b40c67226e7298091527f0f5` and rerun affected gates on the
combined integration tree. Do not substitute the rejected parent, prose, or an
amended/unreviewed commit.
