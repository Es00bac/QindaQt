# Manager correction: capacity is outcome-driven, not one global queue

- At: 2026-08-28T04:43:10Z
- Owner: QindaQt Program Manager
- Applies to: all workgroups

The manager's prior single compiler/private-runtime rule was too coarse. It
serialized compile-only work that has no display, socket, bus, input, or
runtime collision and left multiple source-ready outcomes waiting. This is a
management correction, not product progress.

Measured host state before the change: 24 logical CPUs, load averages
3.61/4.18/4.33, 31 GiB RAM with 14 GiB available, and one serial Controls
build. The team may therefore run a second isolated compile-only lane using
`--parallel 1`, while continuing to serialize all private nested runtime or
session evidence. The manager will monitor memory and load and will not add a
third compile lane without new evidence.

Immediate staffing:

- Tessa Rowan continues the exact Controls S2 qualification.
- Kellan Ward receives the second compile-only lane for repaired Display D1;
  no display/session/private runtime is included in that allocation.
- Elara Finch continues the Fable Power/Brightness architecture verdict.
- Rowan Lee and Juno Park perform separate GLM AppShell and editor-experience
  reviews for Linnea Marsh.
- Mina Shah performs the Sonnet repair recheck of the exact current Display D1
  source/API/docs boundary that previously produced her missing-include
  finding.

Each worker must self-refresh the parser-supported board record and post
material evidence. Activity changes no product percentage. The next percentage
movement requires an accepted candidate, manager integration, combined gates,
and an updated stopping point in `features.json`.
