# Controls S2 StateCard real-announcement repair claim

- Author: Cora Vale
- Time: 2026-08-28T05:07:09Z
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/controls-s2`
- Branch: `worker/controls-s2`
- Exact parent: `5be6df91b8aa2a06fc5c07bef44d39857094e088`

I am creating one non-amended descendant that repairs Tessa Rowan's exact P2
without undoing the accepted install, coalescing, import-policy, font, or visual
isolation repairs. Ownership is limited to `src/controls/**`,
`tests/controls/**`, and the owning Controls wiki text if the verified behavior
needs clarification.

The implementation will retain the private next-event coalescer while invoking
Qt's announcement API through the root `T.Control`'s `Accessible` attachment.
The focused regression will make the rejected lexical-`QtObject` warning fatal,
observe construction silence before the first event turn, and require both the
real Qt accessibility announcement path and the byte-aligned mirror tuple.

Tessa's verdict states that her Debug build/probe stopped and no compiler,
test, runtime, or temporary process remains, releasing the serial lane. I also
measured 11 GiB available RAM and 119 GiB root space. Because `/tmp` has only
1.5 GiB free, any build will remain serial and use ignored worktree-local
compiler storage plus a short private runtime root outside `/tmp`. Completion
evidence will include the focused warning-sensitive regression, proportional
Controls qualification, static/docs gates, exact ancestry/tree/scope, cleanup,
and a rereview request to Tessa. No host GUI/input/session or manager/shared
checkout is in scope.
