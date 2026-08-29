# Authority refresh creates phantom Appearance write intent

- Timestamp: 2026-08-28T09:09:53-06:00
- From: Maxwell the 2nd
- State: material P1 exact-candidate finding; review continues
- Exact candidate: `9a495aad63034a5fa02613df7ab0d17b9d920385`

## Two deterministic failure paths

`setConfirmed()` assigns the new confirmed value but copies it into the draft
only when no baseline existed (`appearance_settings_model.cpp:423-439`). Every
later authoritative snapshot retains the entire old draft, regardless of what
the user actually edited.

1. Start clean at revision 7. Receive a routine same-lineage revision 8 whose
   wallpaper or theme changed externally. The old draft is now unequal to the
   new confirmed values, so `draftDirty`/`applyAvailable` become true and the UI
   offers to reapply stale values despite zero user edits. Replacement authority
   with different values has the same defect.
2. Start at revision 7, edit only Theme, then receive an unrelated external
   Wallpaper change. The draft retains the old Wallpaper. At Apply,
   `startApplySequence()` diffs the full draft against the new confirmed map
   (`:441-450`) and queues both Theme and stale Wallpaper, overwriting a setting
   the user never touched.

This violates the claimed changed-key-only write contract and creates user-data
loss at an ordinary multi-writer settings boundary. Repair needs explicit
per-key draft intent: clean fields rebase to every fresh snapshot, only fields
edited by the user remain retained, and an explicit Revert clears all intent.
Tests must cover clean same-lineage refresh, clean owner replacement, one-field
draft plus unrelated external change, and assert the exact outbound key list.
Product worktree remains untouched.
