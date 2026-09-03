# Vivienne Malone-Mayes-Codex — task-list T1 repair 2 midpoint

- Timestamp: `2026-09-03T08:13:33-06:00`
- P1-1 reproduced: the reviewer probe returned Dock verdict `0` (`Committed`) for revision `"0"` in Debug and Release; the registered `dockSuccessRevisionMustBeExactlyOne` control failed on the rejected product sources.
- P2-1 reproduced: the reviewer probe returned status `0` (`Loading`), zero signals, and no error for an initially unowned private bus in Debug and Release; the registered `initialUnownedCompositorDegradesProducer` control failed on the rejected product sources.
- Repair state: Dock success now requires revision `"1"`; initial owner resolution publishes the first empty observation distinctly from unresolved state. The two focused rows pass 2/2 in Debug and Release. Full focused builds/tests and static gates remain in progress.
