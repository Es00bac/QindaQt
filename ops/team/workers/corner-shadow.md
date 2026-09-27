# Corner shadow

- Status: working — implement the Corner Bar silhouette shadow in an isolated worktree.
- Branch: fix/corner-shadow-contour
- Base: 46816db4
- Owned paths: src/decorations, tests/decorations/tst_decorationvisuals.cpp, docs/wiki/architecture/hybrid-chrome.md

## Updates

- 2026-09-27T18:50:44Z: Claimed the corner-tab shadow outcome. The live decoration always creates a rounded rectangular nine-patch; the painted tab geometry is already available from the shared painter. Investigating a full-width top strip with only vertical body stretching so the notch stays at its actual coordinate.
- 2026-09-27T18:55:57Z: Shadow candidate419fe44e pushed; reviewed pinned KWin ShadowItem::buildQuads: full-width fixed corner cells fit the actual window and only the opaque body row stretches. Reviewer found no static mapping blocker. Focused native build requested from manager.
- 2026-09-27T18:55:57Z: Manager added the bounded all-theme caption audit. Separate candidate1edc1fb6 repairs Bliss inactive caption contrast, preserves existing legible colors, and checks 216 effective caption combinations. Local documentation gate passed 418 pages; runtime gates remain pending with manager.
