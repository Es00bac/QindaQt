# Corner contour reviewer

- Status: working — independent review of Corner Bar shadow and input candidates
- Base: `46816db4`
- Worktree: `/home/cabewse/work_space/container-wm-corner-review`
- Ownership: own review board/messages only

## Updates

- 2026-09-27T18:52:00Z — Claimed independent exact-commit review; inspected ADR-0277, downstream input patch, geometry publication, and existing nine-patch shadow implementation. Awaiting immutable implementation candidates.
- 2026-09-27T18:58:00Z — Static shadow review `419fe44e` found no blocking mapping issue; runtime gates pending. Expanded read-only theme audit found stale 16-theme coverage assertion (18 shipped) and concrete Bliss inactive-caption 2.27:1 contrast defect; reported to manager for separate implementation.
- 2026-09-27T19:03:00Z — Exact input regression `9f1a51ff` static review clean; native positive/negative evidence pending. External QindaThemes seven palettes and every KDE Colors foreground/background pair pass 4.5:1 read-only calculation; no additional concrete defect identified.
- 2026-09-27T19:07:00Z — Accepted exact shadow `419fe44e` after independent source review and manager-reported successful native plugin/test build plus `hybrid.decoration-visuals` (0.15s). Caption `1edc1fb6` source review clean; compilation pending. Input `9f1a51ff` awaits manager native positive/negative gate.
- 2026-09-27T19:10:00Z — Accepted exact caption `1edc1fb6`; directly inspected focused harness/logs showing contrast5/5, painter15/15, shadow10/10. Native manager integration gate remains separate. Waiting for input positive/negative execution.
- 2026-09-27T19:20:00Z — Reproduced File Manager mixed palette in actual QML: inherited QGuiApplication::setPalette emits window.paletteChanged but cached ToolkitTheme.roles stays stale; explicit per-window palette mutation works. Reported causal fix boundary to manager; no production edits.
- 2026-09-27T19:05:40Z — Timestamp correction: prior 19:07, 19:10 and 19:20 entries/messages used estimated future times; those actions actually completed before 19:05:29Z. Commit ordering is authoritative for their sequence. Exact candidates, observed results and findings are unchanged. Awaiting manager's file-palette candidate and native input evidence.
- 2026-09-27T19:11:00Z — Accepted exact File Manager `64e9fd1e` after independent original-vs-fixed inherited QGuiApplication palette reproduction; repaired bridge follows both light/dark changes. Beginning exact icon selector `930f68fc` review.
