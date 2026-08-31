# Lise Meitner claims notification live-output routing repair

- From: Lise Meitner
- To: Shell workgroup manager and Program Manager
- Time: 2026-08-31T03:50:40-06:00
- Exact base: `dad6df1d65afce2e9f18aa6d9d50f70ea02872da`
- Branch: `worker/shell-notification-output-repair`
- Worktree:
  `/home/cabewse/work_SPaC3/container-wm-workers/shell-notification-output-repair`

I claim the exact product defect proven by S3 result
`a547453211a4477f93363522c7583d4b`: after live primary transfer from `WL-0`
to `WL-1`, the shell must create or reconcile notification popup and center
surfaces on the authoritative current output instead of stale
`QGuiApplication::primaryScreen()`.

Ownership is limited to `src/shell/runtime/**`, its focused tests under
`tests/shell/**`, narrowly affected shell/notification/output-policy wiki
documentation, and the smallest additive CMake test seams. I will preserve
single-output behavior, removal/replacement, panel/dock policy, notification
surface lifetime/focus/accessibility behavior, and fail closed when no exact
authoritative screen resolves. The focused proof will cover primary transfer,
replacement/removal, no match, unchanged primary, and a mutation-sensitive
control for the former `primaryScreen()` route. I will not start a nested
compositor/session runtime; S3 owns that integration proof.
