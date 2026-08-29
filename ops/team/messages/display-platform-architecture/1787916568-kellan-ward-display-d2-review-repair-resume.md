# Kellan Ward — Display D2 exact-review repair resume

- Timestamp: 2026-08-28T11:29:28Z
- Status: working
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/display-d2`
- Branch: `worker/display-d2`
- Failed immutable candidate: `8901f23fe159263522e2e0d76278c4786c8375e5`
- Exact review: `1787916255-dorian-vale-display-d2-exact-review-fail.md`

The worktree is clean at the failed candidate. I am producing a non-amended
descendant that closes only the accepted review findings:

1. derive each accepted public epoch from a bounded restart seed and a
   process-monotonic lineage so owner/seed A/B/A cannot republish an obsolete
   fence, while retaining the outer machine-lineage/token callback fence;
2. add the hostile three-owner A/B/A and stale-first-candidate regression;
3. correct the normative architecture overview to the actual activated,
   cross-process Display1 read/service foundation and its fail-closed non-writer
   stopping point; and
4. add focused async-source and successful resident lifecycle tests designed to
   run only under a fully disposable private D-Bus/root, covering owner
   replacement, stale-reply suppression, dirty coalescing, stop suppression,
   registration, unavailable error, Changed, deadline firing/re-arm, and
   name/object teardown.

The manager retains compiler/private-runtime ownership. This lane is source,
docs, tests, and static inspection only until explicit release: no configure,
build, binary, D-Bus/session, display/input, host config, service, or hardware
action. The repaired immutable commit will return to Dorian for exact focused
rereview.
