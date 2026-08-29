# Kellan Ward — Display D2 exact-review repair claim

- Timestamp: 2026-08-28T11:26:06Z
- Failed candidate: `8901f23fe159263522e2e0d76278c4786c8375e5`
- Exact review: `1787916255-dorian-vale-display-d2-exact-review-fail.md`
- Worktree/branch: `/home/cabewse/work_SPaC3/container-wm-workers/display-d2`, `worker/display-d2`
- Status: working, source/docs/static only; manager retains compiler/private runtime

I accept Dorian's complete P0/P1/P2/P3 = 0/2/1/0 verdict. The repair remains
one non-amended descendant of the exact failed candidate:

1. Replace one-value epoch reuse detection with a bounded construction: every
   accepted lineage receives a public epoch derived from a process-monotonic
   lineage plus a bounded restart-unique factory seed. This permanently fences
   every earlier accepted public epoch in the process without retaining an
   attacker-controlled owner/epoch set. Add the exact A/B/A seed across three
   owners and stale first-lineage candidate regression. Preserve the separate
   request-scoped outer machine-lineage/token fence.
2. Update normative `architecture/overview.md` to state that Display1 is now a
   real activated cross-process resident/read/service foundation while the
   packaged mutation path remains fail-closed and public KWin mutation,
   persistence, client/UI, and runtime convergence remain later work.
3. Add executable focused transport/resident evidence using only a fully
   private disposable D-Bus/root: successful object/name registration,
   unavailable error, accepted inventory and Changed signal, async exact-owner
   reads, owner replacement and stale reply suppression, dirty-read
   coalescing, deadline tick/re-arm, stop suppression, and name/object teardown.

I will implement and statically audit the source/docs/tests now. I will not
configure, compile, run a test binary, launch a bus, or touch any host
session/display/config/hardware until the manager explicitly releases the
compiler/private-runtime lane. A qualified immutable descendant will be
routed back to Dorian for exact focused rereview.
