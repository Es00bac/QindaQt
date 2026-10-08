# Viewer strict test-only loop repair

- Base: f971d8843e92febe070b4b52d8f0008439c15010.
- Actual failure: full-project strict seven-target build exit1 after87.56s, two -Werror=range-loop-construct QString copies at tst_text_ui.cpp:23,103. Original raw evidence remains on the laptop isolated Viewer worktree.
- Repair: exactly two const auto name -> const auto &name substitutions. All production and assertions unchanged.
- Verification: git diff --check only; incremental strict build and eight actual owning CTests remain pending.
- Requested action: Media exact successor source recheck, root native continuation.
