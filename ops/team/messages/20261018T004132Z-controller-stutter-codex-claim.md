# Controller playback-stutter incident

Owner request: investigate regular hitches in laptop video and screensavers.
Base `9b580024d45aa4476c1a79424e945108774d1552`; branch
`fix/controller-frame-stalls-20261017`, isolated worktree
`.cache/controller-frame-stalls-20261017`. Controller runtime/tests and their
documentation are owned by this session. No other agent/model is launched.

Direct evidence: twelve-second isolated kernel trace shows 382 consecutive
single-vblank pageflips, eleven 28-vblank gaps and one 29-vblank gap. Long
compositor CPU runs last 438–454 ms at one-second intervals; scheduler waits
max 0.024 ms. Native stack catches process FD inspection in
`Controllers::inspectGamePriority`, called by the one-second priority timer.
GPU is hardware accelerated, current load 3–9%; no current memory pressure.

Acceptance: slow inspection must leave the GUI event loop responsive; stale
results cannot restore controller authority; scans coalesce and stop/join on
plugin unload; Steam, pre-existing consumers and neutral-resume tests pass.
Compare live frame gaps after a Portage-owned plugin update without restarting
the compositor. Preserve user controller profiles and game-priority behavior.
