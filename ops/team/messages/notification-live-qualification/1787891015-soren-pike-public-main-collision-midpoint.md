# Notification Live public-main collision midpoint

- **From:** Soren Pike
- **Timestamp:** 2026-08-27T22:23:35-06:00
- **Candidate identity:** `worker/notification-live` at exact base/HEAD
  `c4982697858c083828bd406f1aa56c4e942bcc10`; 70 preserved paths = 38 tracked
  modifications + 32 untracked additions
- **Public main:** ten commits through
  `2c52c985f846b083c2aebb7a08f04aa8318a2912`; 89 changed paths since base

Exactly four path intersections exist; no product source path intersects:

1. `docs/wiki/adr/index.md`: both sides insert at base line 21. Public owns
   ADR-0013/0014; Notification owns ADR-0019/0020. This is a direct same-anchor
   additive resolution: preserve all four in numeric order.
2. `mkdocs.yml`: public adds QST/Audio architecture/reference navigation and
   ADR-0013/0014; Notification adds ADR-0019/0020. Only the ADR tail shares the
   base-line-64 insertion anchor. Preserve every public entry, then append
   0019/0020 in numeric order.
3. `docs/wiki/development/testing-harness.md`: shared file, disjoint base
   hunks. Public owns QST at line 120, deterministic never-hidden surface proof
   at 181-211, and Audio at 595. Notification owns focused/live presentation
   material at 221, 251, 298, 309-329. Preserve the public surface wording and
   QST/Audio sections around the Notification additions.
4. `tests/session/CMakeLists.txt`: shared registry, disjoint base hunks. Public
   owns the surface-proof fixture/profile arguments at 165/177. Notification
   owns unit/syntax/tool registration at 71-96 and the live include at 225.
   Preserve both; the new profile fixture must remain the production-surface
   row input, while Notification rows remain isolated in
   `NotificationLiveTests.cmake`.

No merge, rebase, stash, commit, rewrite, configure, build, CTest, install, or
runtime launch occurred. Next I will run only the Python driver unit (which
starts Python-only disposable children, no product executable), docs validator,
source-shape checker, and whitespace check, then publish the safe later
integration order and exact next compiler commands.
