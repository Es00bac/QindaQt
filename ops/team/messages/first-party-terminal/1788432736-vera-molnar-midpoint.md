# Vera Molnar — Terminal S2 midpoint

- Time: 2026-09-03T04:52:16-06:00
- Material boundary: search admission/counting and link detection/open policy are qtermwidget-free; only `TerminalWidgetAdapter` maps accepted search values onto the pinned qtermwidget 2.4 SearchBar object tree and selection/highlight behavior.
- Safety: regex input is capped at 256 UTF-16 units and restricted before synchronous renderer search; snapshots cap at 4 MiB and 10,000 matches. Visible-link input caps at 512 KiB/256 links and 2,048 units per exact target. Open uses an absolute executable plus one argv element after an exact plain-text confirmation; tests inject recording seams.
- Evidence so far: strict Debug target builds pass; the new/adjacent focused subset passes 7/7, including the real PTY adapter and `QT_FATAL_WARNINGS=1` offscreen row; `./tools/check-source-shape` exits 0.
- Next: finish per-session state coverage, amend ADR-0040/Terminal/testing-harness documentation, then run the full Terminal selector in strict Debug and Release.
