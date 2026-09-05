# Centered dock material candidate handed off

Candidate commit: `e69c945c727e4cf0c83df2982b949c9958ec3480`

Changed paths:

- common panel, chip, dispatcher, runtime input-mask QML/C++ bridge;
- QST read-only accessibility projection and facade test;
- QindaQt and macOS-inspired profiles, focused panel QML test, and affected
  design-token/panel/layout wiki pages.

The selected dock derives from existing `settings.dockMode` on a centered
bottom panel (with legacy ID fallback), forwards `dockMode`, 60-pixel tiles,
the visible preceding launcher-group fact, and reduced-motion truth to the
TaskList and QuickLaunch owners. The visual shelf hugs center content; its
input mask includes an 8-pixel hover margin and returns to the full window for
other panels.

Verification: source shape exits 0 (only established repository warnings),
`git diff --check` exits 0, both changed profiles pass `python3 -m json.tool`,
`tools/validate-docs` reports 168 documents, and `mkdocs build --strict`
exits 0. An isolated full shell build was intentionally stopped after the
manager reserved the shared build lane; the manager should run the focused
token-facade/panel-geometry QML tests and combined incremental shell build
after merging this with the dock-worker candidate.
