# Erna Hoover-Codex midpoint: hidden phase now waits for settled authority

- Timestamp: 2026-09-03T07:35:59-06:00
- Exact base: `22b31b94e0da12f0be54c5d0d3c48b639815e562`
- Mechanism: Global Menu did not hold a popup lease and did not own the empty intelligent rail. Its integrated timing exposed a pre-existing observation race: after the animation lease releases and the shell begins the Wayland unmap, KWin can publish one mapped/committed layer role with 0x0 geometry before removing the role. The probe's old negative geometry match accepted that record as hidden and captured it immediately; the strict validator correctly rejected the captured inventory.
- Repair: the session phase waiter now rejects the entire compositor snapshot until every surface geometry is positive and framebuffer-contained, then evaluates the requested visible/hidden predicate. Hidden qualification therefore requires authoritative role absence; a role which never disappears reaches the bounded deadline and fails closed.
- Regression: the existing `desktop.virtual.panel-visibility.validator-unit` row now includes fake authority and poll-timer controls for 0x0-then-absent, never-absent, and escaped-geometry sequences.
- Evidence: focused strict Debug and Release builds pass; all 41 visibility, shell-runtime, and Global Menu rows pass in both configurations; `desktop.virtual.boot.1080p` and both panel-visibility rows pass serially twice in Debug with no `kwin_wayland` survivor.
- Static caveat: `./tools/check-source-shape` currently reports the unchanged exact-base file `tests/session/DesktopSessionTests.cmake` at 621 non-blank lines. This lane does not own that active desktop-stage path; the candidate adds no lines to it.
