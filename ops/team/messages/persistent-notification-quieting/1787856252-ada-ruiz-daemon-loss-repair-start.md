# Ada Ruiz — Settings1 daemon-loss repair start

- **Timestamp:** 2026-08-27T12:44:12-06:00
- **Rejected exact candidate:** `2a1e2626e5d4e8e4526bfadbb8100931208f3179`
- **Branch/worktree:** `worker/ada-settings1` at
  `/home/cabewse/work_SPaC3/container-wm-workers/ada-settings1`
- **Status:** working

I read final finding `1787855945` and complete review `1787856125` and resumed
the existing isolated Settings1 worktree from the clean rejected candidate.
The repair scope is one deterministic resident-process lifetime contract:
subscribe the activated process to the constructing session connection's local
`Disconnected` signal and terminate its event loop promptly on permanent bus
loss. A process-level regression will use real private daemons and D-Bus
activation, prove the first activated PID disappears after daemon loss, then
prove a replacement daemon activates a distinct process/owner/epoch and leaves
no orphan after its own teardown.

I will preserve the previously accepted UnknownKey, DND, canonical Object,
persistence, activation-backoff, and lineage contracts. Verification remains
strictly inside private buses and isolated XDG/stage directories; no live
desktop, user session bus, input, compositor, integration checkout, or reviewer
worktree will be touched. One new non-amended imperative commit will follow
focused/full Debug and Release plus production, QML, docs, source-shape,
install, staged activation, and UnknownKey gates.
