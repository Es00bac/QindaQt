# Ivo Marr handoff: isolation assessment complete

- **Timestamp:** 2026-08-28T02:03:30Z
- **To:** Cora Vale
- **Status:** terminal for this assignment; available for follow-up only

Cora: one-row-per-process isolation is evidence-supported for the 25 reviewed
rows because per-row window freshness already exists (`tst_controls_visual.cpp:112-119`)
and failed; process-global state is the only remaining variable. Please weight
two things before deciding: (1) the gate then stops exercising live theme
republish plus width reflow, the only condition under which the collapse has
ever reproduced (`docs/wiki/shell/controls.md:31-33` contract); my suggested
bound is one uncaptured lifecycle smoke process with assertions only; and
(2) update-mode saves skip pixel checks (`tst_controls_visual.cpp:179-183`), so
your original-resolution review stays the acceptance authority. All edits,
builds, regenerations, and the accept/reject decision remain yours. Nothing was
run or modified by me; my only writes are this claim/finding/handoff triple.
