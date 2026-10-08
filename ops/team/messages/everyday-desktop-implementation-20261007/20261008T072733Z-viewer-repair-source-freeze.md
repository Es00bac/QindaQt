# Viewer keyboard/capability repair source freeze — 2026-10-08T07:27:33+00:00

Exact base59a1f99576a75c2ec11aa567f91918cbed722b6d, preserved original15e
applied cleanly at432ab97fc2d131e4448125c6b0b11e75a07d46ab. Source-only
repairs retain the actual PDF parser/permission/bounds/thread/retirement
contract and public toolkit boundary.

- Fully default-construct cancellation results before setting revision;
  do not leave strict warning-fatal partially initialized aggregates.
- Gate public Dialog primary action; public footer Close remains keyboard
  accessible. Return/keypad Enter belongs to search and must keep the pane open.
- New Tk.Button capabilities use available, preserving toolkit busy ownership.
- Actual UI journey now asserts keypad search delivery, open pane/selection,
  disabled busy controls, restoration and explicit keyboard Close.
- No native result, image or installed completion is claimed at source freeze.
  Next are static/docs, strict seven targets, eight owning rows, pixel evidence
  and different-author exact review. Printing remains a separate outcome.

Source-only gates now pass: docs/link/navigation533, strictMkDocs14.64s,
whitespace0. Full source-shape exits1 with 63 inherited errors and
106 warnings on unchanged paths; no changed-path findings. Raw full
audit and scoped comparison preserved under ignored .cache/viewer-repair-static-20261008.
Native and independent review gates remain pending.
