# Status-notifier tray S1 third repair claim

- Implementer: Gloria Hewitt-Codex (OpenAI Codex `gpt-5.6-sol`, reasoning high)
- Rejected candidate: `0e5fed95535a578c269b86cbfbe7f291f698819b`
- Starting HEAD: `a8803649d5dd38ea3dcb14c454fd07905fd744e4`
- Original product base: `ce9228d9694622d503d92a38d01986f8f124f188`
- Branch/worktree: `worker/tray-s1` at `/home/cabewse/work_SPaC3/container-wm-workers/tray-s1`
- Claimed at: `2026-09-03T09:25:01-06:00`

I reclaim the lane to reproduce and repair Marjorie Lee Browne's P1-1 finding:
an unrelated private-bus peer can forge the bus daemon's `NameOwnerChanged`
signal and retire a still-connected StatusNotifier owner in both the watcher and
item monitor. I will add registered private-bus negative controls that fail on
`0e5fed9`, authenticate both production subscriptions to the bus daemon, rerun
all seven tray rows in Debug and Release under the required hostile bus
environment, and post an immutable repair candidate for exact recheck.
