# Integration handoff

## Current baseline

- Branch: public `main`
- Functional commit: `05a8636fb8ba9914e51d1cae5f117f77e90c75e3`
- Tree: `bf0e61dd1fad12bbb6498a943b69b17921e17656`
- Outcome: persistent Settings1 plus QST-1 semantic design tokens
- State: independently accepted, manager-qualified, and published with a
  documentation-only project-identity descendant

The baseline combines generic persistent Settings1 and the first-class
Notifications route with QST-1's pure semantic token derivation, accessibility
overrides, read-only QML adapter, and installed consumer packages. The
Settings1 resident exits on permanent session-bus loss; a new daemon activates
a new process and lineage rather than reconnecting stale repository state.
QST-1 owns semantic policy without importing a general application framework
or widening the theme schema.

Integrated evidence:

- Independent exact-tree review: accepted with P1/P2/P3 `0/0/0`.
- Strict-warning Debug and Release production-shell builds: 906/906 each.
- Debug and Release complete QindaQt registries: 87/87 each; QST-1 5/5 and
  Settings1 16/16 in each configuration.
- Settings process daemon-loss lifecycle: 20 consecutive passes in each
  configuration.
- Fresh testing-disabled production/package build: 454/454; staged install:
  168 files with exact Settings1 descriptor/executable resolution and no
  missing linked library.
- Installed QST QML consumer: 3/3; installed two-daemon Settings1 loss,
  replacement, and `UnknownKey` lifecycle: ten consecutive passes.
- Debug, Release, and production QML lint; 789-file source-shape audit;
  44-document link/navigation validation; strict MkDocs; whitespace; and
  post-test process cleanup all passed.
- No active desktop, user session bus, global input, real audio graph, physical
  display, or physical screen lock was touched by this evidence.

## Next outcome

Qualify and integrate the bounded Audio1 service described in
[Task list](TASK_LIST.md). Its current repair work is not part of this baseline
and must produce a new exact commit, pass independent re-review, resolve the
shared QST-1 registries and documentation additively, and pass combined manager
gates before integration.

In parallel, qualify the installed notification shortcut, keyboard/focus,
Settings1 replacement, Do Not Disturb, and real nested lock transitions at
1080p, WUXGA, and 1440p. That lane must remain private and nested: it may not
move the host cursor, inject host input, replace the active compositor, or use
the live session bus. Neither active lane is complete merely because its worker
process exists or a source-only test passes.
