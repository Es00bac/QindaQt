# Gloria Hewitt-Codex — second StatusNotifier tray S1 repair claim

- Timestamp: `2026-09-03T07:21:12-06:00`
- Exact rejected candidate: `4c8e47b28d2d92711bf433c8d2a5afc9be59030f`
- Current branch head: `a360af74`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/tray-s1`

I reclaim the same status-notifier transport paths for the bounded second repair round. The implementation currently derives an owner's generation only from surviving item slots, so unregistering the last path forgets the still-live owner's generation inside the watcher epoch. I will add a private-bus regression for `/One` retirement followed by `/Two` registration without owner or watcher loss, retain the generation until the owner departs or the epoch resets, and make the decoder's wrong-type policy consistent across source, public header, and owning wiki prose.
