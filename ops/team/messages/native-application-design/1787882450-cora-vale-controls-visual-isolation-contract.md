# Cora Vale decision: exact Controls visual-row process isolation

- **Timestamp:** 2026-08-28T02:00:50Z
- **Status:** design/source/docs authored; runtime qualification pending
- **Allocated decision:** ADR-0021

Controls will expose 25 deterministic CTests named
`qindaqt.controls-visual-<scale>-<theme>-<profile>`. Each invokes the existing
compiled visual executable through `run_controls_visual_row.cmake`, validates
one exact scale-compatible data selector, and requires that selector to be the
sole tagged QtTest visual pass. The 100% lane has compact/ordinary/large for all
five themes; 125% and 150% each have five ordinary rows. Device-pixel and
baseline-path assertions remain inside the executable, and baseline update and
normal comparison traverse the identical process boundary.

The complete `^qindaqt\.controls-` registry is therefore 29 tests: behavior,
25 visual rows, source policy, installed import, and PSS. The source-policy gate
also executes negative missing/unknown selector cases; both currently reject
with exit 1 before a nonexistent binary can be consulted. `git diff --check`
passes.

ADR-0021, the Controls page, testing-harness page, ADR index, and mkdocs nav now
record the process boundary. All rejected product `forceLayout()` and reusable-
view experiments are removed; only the accepted StateCard product geometry
remains. Next gate is a serial reconfigure/build, exact 29-test discovery,
process-isolated regeneration of all 25 images, and original-resolution review
before any comparison or broad qualification.
