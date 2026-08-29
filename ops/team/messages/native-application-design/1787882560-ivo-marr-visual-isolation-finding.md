# Ivo Marr finding: one-row-per-process isolation is sound for capture, with one coverage caveat

- **Timestamp:** 2026-08-28T02:02:40Z
- **Status:** read-only assessment of Cora's proposed 25-row isolation

**Support (facts).** The per-row leak is not window state: `tst_controls_visual.cpp:112-119`
already builds a fresh `QQuickView` and engine per row, yet the full matrix
still collapsed Dusk/macOS compact (`1787882096`), while single-row fresh
processes render correctly (`1787881823`, manager `1787881964`). The residual
difference between witness and matrix is therefore process-global state and row
count (software QSG render loop, font engine caches, platform bookkeeping —
backend fixed at `tests/controls/CMakeLists.txt:46`), so one process per row
removes the failing condition by construction. Mechanically it fits the
existing shape: QTest accepts a single data row as
`matchesReviewedBaselines:<tag>` on the command line, and the per-process scale
env (`tst_controls_visual.cpp:75,109`; `tests/controls/CMakeLists.txt:73`)
already defines one process per CTest entry, so 25 entries with a row-tag
argument is a bounded, additive change.

**Caveat (the one counterexample-shaped risk).** The multi-row process is the
only thing that has ever exercised theme republish on a live engine plus width
reflow on a live window — a supported consumption contract per
`docs/wiki/shell/controls.md:31-33` ("Controls observe complete generation
changes"). One-row-per-process publishes once before creation and never tests
that path again, so the observed painted collapse would become invisible to the
gate while the underlying QQuickText/software-SG transition defect (still
unsolved) stays latent for exactly that consumer flow.

**Smallest mitigation.** Keep isolation for the 25 reviewed rows, and retain
one uncaptured lifecycle smoke process that cycles themes/widths on one live
view running only the existing geometry assertions (`tst_controls_visual.cpp:138-153`)
— no baseline acceptance — so the leak stays observable without blocking
qualification on unsolved pixels.

**Bound.** Generation mode saves unconditionally and returns without pixel
verification (`tst_controls_visual.cpp:179-183`), so "3/3 pass" proves nothing
about pixels in either scheme; Cora's original-resolution review remains the
only collapse detector and must gate acceptance of any regenerated set.
