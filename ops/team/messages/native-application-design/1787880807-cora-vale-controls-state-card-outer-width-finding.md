# Cora Vale finding: StateCard outer implicit width collapse

- **Timestamp:** 2026-08-28T01:33:27Z
- **Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/controls-s2`
- **Status:** bounded repair authored; narrow rebuild and replacement pixels pending

The narrow build and five-theme 420px behavior gate passed (19/19), then I deleted
only the exact named 25 rejected images and regenerated 15/5/5 rows successfully.
Contact-sheet review stopped before comparison on a new product regression: every
ordinary/large and 125%/150% no-action Error StateCard became a narrow vertical red
strip with one-character wrapping. The compact DegradedNotice/action rows were fixed.

Cause: the inner StateCard text column needs a zero preferred width to shrink and
wrap beside an action, but without an outer intrinsic floor that also made the whole
no-action StateCard look nearly zero-width to a parent GridLayout. The bounded repair
preserves the inner constraint and adds a 220-logical-pixel outer implicit-width
floor, matching the existing ThemeCard card preference. Behavior now asserts that
both an action StateCard and a no-action Busy StateCard retain that intrinsic floor;
the existing five-theme 420px assertions continue to prove text and Retry geometry.
No product minimum is reduced and no fixture state is hidden.

Source gates after the repair: diff check, source shape (behavior test 496 non-blank),
and direct 14-QML theme/import/hex policy all exit 0. Current 25 images are rejected
and will be explicitly replaced again only after the repaired narrow runtime passes.
