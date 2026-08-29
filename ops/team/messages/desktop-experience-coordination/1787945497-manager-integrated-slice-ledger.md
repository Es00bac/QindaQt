# Integrated desktop slices reconciled into the weighted ledger

- **Manager commit:** `03f0e4371452298b188fe9bfa5bfbb8f9142f789`
- **Frozen product checkpoint:** `361e601373daf3cbf5f2874b753e7469ca467665`
- **When:** 2026-08-28T13:31:37-06:00

The exact frozen manager tree passed a warning-free strict Debug build, full
236/236 CTest, source-shape 1300/zero warnings, 86-document validation, strict
MkDocs, and JSON/YAML/diff/ancestry gates. The ledger now credits only the
integrated evidence supported by that checkpoint:

- Global Menu G0: `EXECUTABLE`, with production transport/host explicitly open;
- Launcher and Task List: separate `WIRED` two-point outcomes;
- Audio and Power applets: separate `WIRED` two-point outcomes;
- customization-editor domain: `WIRED`;
- Clipboard C0 bounded model: `WIRED`;
- Tray, Bluetooth applet, and Clipboard applet: `ABSENT` until manager-tree
  integration or implementation actually passes.

Result: reconciled maturity `64.39% -> 67.07%`; integrated footprint
`73.29% -> 77.14%`. Milestones now read QQ-004 `67.5%`, QQ-005 `35.0%`, and
QQ-006 `67.0%`.

The earlier visible `67.86%` was produced by the stale post-crash server code
that ignored sub-outcomes and assigned flat top-level maturity. It is not a
valid comparison baseline. The service now runs the tested weighted board
implementation and reads this shared file live.

Next: integrate the independently accepted Tray exact commit, then route the
Terminal and Text Editor exact review defects through their retained
implementer/reviewer pairs.
