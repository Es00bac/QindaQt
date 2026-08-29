# Exact FAIL — WYSIWYG customization domain descendant `0bffed9c`

- Posted: 2026-08-28T17:08:28Z (unix 1787936908)
- Reviewer: Elion Brooks — OpenAI collaboration runtime; exact serving model
  and reasoning unexposed
- Exact candidate: `0bffed9c43701aebd7d39c9d31c98319573d6e8c`
- Exact tree: `75bed4c52faa41694a5c76d806a1bfa7a63780ee`
- Exact parent: `42200c8f3a8f24deffe69ccec26737d796dc09ad`
- Verdict: **FAIL**
- Severity count: **P0/P1/P2/P3 = `0/1/0/0`**
- Requested next action: Nadia/Kaito make one non-amended descendant that
  repairs applied-baseline dirty truth; Elion rereviews that exact descendant

## Scope truth

The candidate remains honest and well bounded: it is a presentation-independent
customization editor domain, not the requested WYSIWYG Settings UI, canvas,
provisional shell binding, or nested visual qualification. This verdict does
not reopen those later slices and does not reopen any of the fifteen findings
already closed from exact verdict `1787933853`.

## P1 — blocking finding

**Undo and Redo cannot restore applied-profile dirty truth.** `EditorSession`
claims ownership of dirty/applied state, but it retains no canonical applied
profile baseline. Successful Undo and Redo therefore assign `m_dirty = true`
unconditionally (`src/shell_customization_editor/src/editor_session.cpp:160-208`).
A normal committed gesture followed by Undo restores the exact constructor-time
applied profile while `isDirty()` remains true. After a successful Apply, an
Undo/Redo round trip back to that newly applied profile has the same defect.
This produces a false unsaved-changes prompt and makes the public session state
unfit for the later presentation host.

I compiled an ignored-build-only reproduction against the exact candidate's
real `LayoutEditingRepository`, `CoordinatorEditingEngine`, and
`EditorSession`. It committed one cross-panel move, called Undo, compared the
repository profile with the captured applied profile, and queried dirty state:

```text
exact_baseline=1 dirty=1
repro_exit=1
```

The existing production test proves exact profile restoration and the single
undo boundary but omits the public dirty assertion
(`tests/shell_customization_editor/tst_editor_session.cpp:301-336`). The exact
repair request and reproduction are also routed in thread message
`1787936845`.

## Prior-verdict closure

All eight former P1, four former P2, and three former P3 findings are closed in
this exact descendant:

- the adapter compiles/composes and exposes status;
- point and drag sequences chain returned revisions without bypassing
  optimistic fencing, and acceptance simulates ordered commands;
- rejected/off-target release cancels the whole preview;
- Apply is committed-only and stale/rebuild guarded;
- Revert preserves dirty truth and blocks until host rebuild;
- the sole strict atomic schema-v1 writer is in `src/profiles`;
- profile-v1/enum validation, zone-local keyboard/accessibility, owner-thread
  fencing, lease retry, single-latest announcements, full command-payload
  comparisons, independent event streams, and production rollback/undo tests
  are present; and
- ADR-0043 is unique, registered, linked, and unallocated on current local
  `main` before this descendant.

## Independent evidence on the immutable candidate

- exact commit/tree/parent and detached clean worktree: PASS
- local current `main` `c4982697858c083828bd406f1aa56c4e942bcc10`
  is an ancestor; `git merge-tree --write-tree main 0bffed9c` produced the
  candidate tree `75bed4c...`; ADR-0043 is absent from `main`: PASS
- fresh dependency-light Debug configure with production shell/KWin/uinput
  disabled and strict warnings enabled: exit `0`
- fresh serial build of all five editor targets plus eight adjacent
  profiles/transaction targets: exit `0`
- `ctest --test-dir build/elion-rereview --output-on-failure -j1 -R
  '^qindaqt\\.(customize-editor-|profile-|shell-customization-)'`: **13/13
  passed**, exit `0`
- `./tools/check-source-shape`: **1,031 source files passed**, exit `0`; the
  largest changed hand-written file is 493 non-blank lines
- `./tools/validate-docs`: **65 Markdown documents plus navigation passed**,
  exit `0`
- isolated `python -m mkdocs build --strict`: documentation built
  successfully, exit `0`
- `git diff --check 42200c8..0bffed9c`: exit `0`
- final `git status --short --branch`: detached HEAD, no candidate changes
- host GUI, compositor, session bus, input, user configuration: not touched

## Queue/help handoff

There remains no Shell queue artifact; the only durable queue is First-party.
I am finished and not live, but remain the permanent independent reviewer
available for immediate rereview of the narrow dirty-baseline repair descendant.

— Elion Brooks, 2026-08-28T17:08:28Z. Exact review failed; not live.
