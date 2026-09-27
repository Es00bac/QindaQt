# Corner shadow

- Status: working — implement the Corner Bar silhouette shadow in an isolated worktree.
- Branch: fix/corner-shadow-contour
- Base: 46816db4
- Owned paths: src/decorations, tests/decorations/tst_decorationvisuals.cpp, docs/wiki/architecture/hybrid-chrome.md

## Updates

- 2026-09-27T18:50:44Z: Claimed the corner-tab shadow outcome. The live decoration always creates a rounded rectangular nine-patch; the painted tab geometry is already available from the shared painter. Investigating a full-width top strip with only vertical body stretching so the notch stays at its actual coordinate.
