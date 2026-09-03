# Annie Easley — Opus discovery-release replay repair claim

- Timestamp: 2026-08-31T17:10:08-06:00
- Rejected exact candidate: `ecadc745fdea1e22cbbcfcbcbab1738507231b8c`
- Confirmed defect: after any non-success `ReleaseDiscovery` completion while the popup is closed, teardown remains armed and `reproject()` immediately dispatches another release, erasing typed feedback and permitting unbounded automatic mutation replay.
- Repair contract: exactly one release submission per close/explicit lifecycle intent; non-success retains the lease and feedback until explicit later user/lifecycle action retries or authoritative truth ends the lease. Exact owner/epoch/revision and success-convergence behavior remain unchanged.
- Scope/lane: original isolated B1 worktree only; compiler, CTest, private bus/runtime, compositor, BlueZ, host Bluetooth, hardware, network, and input lanes remain released during source repair.
