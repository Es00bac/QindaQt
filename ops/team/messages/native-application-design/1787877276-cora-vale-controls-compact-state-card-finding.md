# Cora Vale finding: compact StateCard text collapse

- **Timestamp:** 2026-08-28T00:34:36Z
- **Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/controls-s2`
- **Status:** source repair authored; compiler/runtime and replacement baselines pending

The first compact fixture repair succeeded in containing the final Apply button,
but original-resolution inspection stopped acceptance on a second defect. In the
420x840 Qinda Dusk and Qinda macOS rows, DegradedNotice's StateCard text column
collapsed to roughly one character beside Retry: title rendered as `F` and the
message wrapped one character per line. Dark, light, and high-contrast happened to
receive enough width, proving theme-dependent layout allocation rather than missing
content. No normal baseline comparison was run and the current generated set remains
rejected.

The bounded product repair keeps `RowLayout` and the Retry button's native minimum.
`StateCard` now gives its fill-width text `ColumnLayout` an explicit zero
minimum/preferred basis and the two wrapping Text children a zero minimum, ensuring
the layout assigns remaining width instead of treating theme text implicit width as
an unsplittable preference. Searchable object names expose the text column/title/
message geometry. The 420px behavior row now requires at least 160 logical pixels for
the text column/message, a non-pathological title line count, visible Retry, and its
96px product minimum. Common accessibility/motion test helpers moved cohesively into
`control_test_support` to keep the behavior source below its 500-line review limit.

Source-only gates after the edit:

- `git diff --check`: exit 0.
- `./tools/check-source-shape --largest 5`: exit 0; largest Controls test is 498
  non-blank lines.
- direct Controls source policy: exit 0; exactly 14 QML files and no forbidden
  theme IDs, palette hex, or imports.

Display currently owns the compiler. The existing 25 images must not be accepted;
after a narrow rebuild/behavior pass, all 25 will again be removed only from the
dedicated baseline tree and regenerated before contact-sheet/original review.
