# Nadia Park — WYSIWYG C0 executable repair midpoint

- **Time:** 2026-08-28T10:44:14-06:00
- **Rejected base:** `42200c8f3a8f24deffe69ccec26737d796dc09ad`
- **Worktree:**
  `/home/cabewse/work_SPaC3/container-wm-workers/wysiwyg-customization-c0-repair-nadia`
- **Status:** working; documentation/registration and broad gates remain

The inherited adapter repair remains preserved and is now part of a compiling
whole repair. The dependency-light strict-warning build succeeds. `ctest -j1`
passes all 5/5 focused targets: intent, gesture machine, session, persistence,
and accessibility. The session target includes real repository/coordinator
coverage for cross-panel + zone mutation, chained revisions, one durable undo
step, exact cancel restoration, rejected-release cancellation, return from an
invalid hover, preview-safe Apply, typed rebuild-required Revert, and lease
retry.

Material contracts now implemented:

- `src/profiles` is the sole user-profile file writer. It rejects empty
  directories/IDs, validates the typed profile, proves a strict loader round
  trip, and commits atomically without direct-write fallback.
- The editor retags every command from the preceding result. Multi-command
  acceptance runs on a disposable real repository with the same outputs and
  manifest catalog, so cross-panel zone moves are evaluated in sequence
  without advancing live state.
- Rejected/off-target releases cancel the whole preview; Apply cannot persist
  an open preview; Revert preserves dirty truth and returns a typed host-rebuild
  requirement.
- Structural bounds match profile v1, `always` is disabled until reveal lands,
  keyboard/AT positions use the current zone only, and announcements coalesce
  to one latest tuple per event turn.

I am completing the unique ADR allocation, primary wiki/module/root build
registrations, hostile/broad gates, and one clean descendant before requesting
Elion Brooks's exact rereview.

— Nadia Park, 2026-08-28T10:44:14-06:00
