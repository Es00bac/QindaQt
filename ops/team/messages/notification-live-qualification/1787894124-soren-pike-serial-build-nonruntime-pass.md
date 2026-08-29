# Notification Live serial build and non-runtime checkpoint

- **From:** Soren Pike
- **Timestamp:** 2026-08-28T05:15:24Z
- **Exact source:** `worker/notification-live` at
  `c4982697858c083828bd406f1aa56c4e942bcc10`, preserved uncommitted 70-path
  candidate
- **State:** source/build/non-runtime checkpoint; no installed live-session
  qualification is claimed

## Reconciliation and static evidence

The tracked binary diff, untracked candidate-content manifest, and candidate
path manifest reproduce the earlier handoff hashes exactly:

- `dc5d63b9cdf117457e8d341b7e5be41cdd49fe2a7203bc4a20fbeac692683e26`
- `9454c5dc57c1f3eacd21c0680173ace6b7f87e1a88eb6f7ac661e1389cddd4c4`
- `8f0deffb84a07d2de654be6a220ab9d8decae85504e22beafe371585daf895fb`

The separate `.omc/` local-state directory is excluded and will not enter the
candidate. Omar's C1 and Theo's import/registration repair remain present;
Lyra's F1–F5/F8 and N1/N3 closures compile and execute. Fresh Python driver
unit 10/10, documentation validation 44, source shape 799 with zero skips or
warnings, compositor descriptor parsing, and whitespace pass.

## Serial build and test evidence

Measured pre-build headroom was 9.4 GiB available memory, 31 GiB workspace
disk, 1.5 GiB `/tmp`, and load 9.57 on 24 CPUs. Every build used
`--parallel 1` in this worktree.

- Invalidated Debug targets: exit 0, 32/32 steps; repaired supervisor/driver
  rows: 2/2 pass.
- Complete current-source Debug incremental build: exit 0.
- Debug focused notification/settings/session/shell selector: 50/50 pass.
- Debug safe broad registry: 102/102 plus 46/46 = 148/148 pass.
- Complete current-source Release incremental build: exit 0, 23/23 invalidated
  steps.
- Release focused selector: 50/50 pass.
- Release safe broad registry: 102/102 plus 46/46 = 148/148 pass.

The broad split deliberately excludes registered tests 103–122: production
surface, nested Wayland/Weston, all six Notification Live rows, virtual-output,
KWin plugin live workflows, production-control live workflow, and installed
plugin discovery. The manager has not allocated the single private runtime
lane, so none was started. Sanitizer, staged-install/package, QML lint, strict
documentation, final source gates, and public-main collision recheck remain
before a commit decision.
