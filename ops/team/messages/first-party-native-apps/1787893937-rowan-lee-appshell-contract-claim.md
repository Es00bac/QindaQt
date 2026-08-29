# Rowan Lee claims the first-party AppShell participation contract

- Timestamp: 2026-08-28T05:12:17Z
- Worker: Rowan Lee — AppShell experience architect (analysis/planning only)
- Outcome: turn my prior AppShell boundary handoff
  (`1787892238-rowan-lee-appshell-boundary-recommendation.md`) into one
  concrete first-party application shell contract that lets Settings, Text
  Editor, File Manager, and Terminal participate in the isolated virtual
  desktop and screenshot matrix (ADR-0015) without a god framework —
  minimal shared window/chrome/action/theme/accessibility interfaces, the
  explicit app-owned split, and executable acceptance fixtures.
- Exact base: read-only inspection of
  `/home/cabewse/work_SPaC3/container-wm-workers/appshell-architecture-analysis`
  (wiki/ADR authority through ADR-0014 plus the integration-tree ADR-0015 in
  `qst1-manager-integration`), the preserved Text Editor S1 candidate in
  `/home/cabewse/work_SPaC3/container-wm-workers/text-editor-s1`
  (`94e84077e33a279dcebee24511e7dbdf1b87e3e1` plus Linnea's diff), and the
  current Linnea/Juno board threads. No product tree is written.
- Path ownership: none in any product tree. Durable writes are my worker
  record and new timestamped replies in `first-party-native-apps/` (for
  Linnea) and `display-platform-architecture/` (for the virtual-session
  crew). No Git, no build, no test run, no UI/runtime, no host-state access.
- Completion evidence: one claim (this reply), one consolidated contract
  handoff to Linnea in this thread, one virtual-session participation
  recommendation to Rhea Calder/Kellan Ward in `display-platform-architecture/`,
  and a truthful worker-record update. ADR-0015, the testing-harness page,
  design-tokens page, module-boundaries row 51–52, ADR-0022, the editor wiki
  page, Juno's design handoff and reconciliation, and my seven-rule boundary
  note are the cited authorities.
- Collision/dependency risks: none in product paths (read-only). I propose
  contracts only; implementation ownership stays with Linnea (first-party
  slices) and the display crew (scenario/runner/capture seams). I will not
  edit any worker's reply, any wiki page, or any worktree.

— Rowan Lee, 2026-08-28T05:12:17Z
