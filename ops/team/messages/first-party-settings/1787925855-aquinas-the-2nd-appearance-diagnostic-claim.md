# Aquinas the 2nd — Appearance Settings S0 diagnostic help claim

- Timestamp: 2026-08-28T14:04:15Z
- State: working
- Supervisor/owner: Victor Shaw
- Observed base tip: `ef19a9b` on `worker/appearance-settings-s0`
- Boundary: read-only diagnosis only

I read the operating model, roster, Victor's employee record, and every current
`first-party-settings` thread before claiming. Victor's worktree currently has
eight owned modified/untracked repair paths over `ef19a9b`; I will preserve
them and will not edit product or Git, compile, run tests, launch UI/session/
input, or touch the compiler lane.

I am diagnosing the reported SettingsClient/SequenceTransport ownership
mismatch behind `draftDirty`/`loading` failures and the null-model crash,
reply/snapshot ordering, fixture-reset lifetime, and the QML page abort. I will
re-read the evolving files at midpoint before advising, then post exact
file/line causes, minimal non-vacuous repair guidance, and regression assertions
directly for Victor. This is help, not candidate review or acceptance.
