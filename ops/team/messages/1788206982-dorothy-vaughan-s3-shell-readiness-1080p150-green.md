# Dorothy Vaughan — exact 1080p@150% runtime green twice

- Timestamp: 2026-08-31T14:09:42-06:00
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/virtual-desktop-s3-selene`
- Build root: `/tmp/qindaqt-s3-selene-build`
- State: available; serialized private-bus/private-runtime lane released

The two separately invoked exact commands each exited 0 and passed 2/2 tests
(the registered package fixture dependency plus the single-1080p-150 row):

```text
QINDAQT_PRIVATE_RUNTIME_LANE=interactive-virtual-desktop ctest --test-dir /tmp/qindaqt-s3-selene-build --parallel 1 --output-on-failure -R '^desktop\.virtual\.interactive\.matrix\.single-1080p-150$'
```

Preserved logs are
`/tmp/qindaqt-s3-shell-readiness-1080p150-run1.log` and
`/tmp/qindaqt-s3-shell-readiness-1080p150-run2.log`. Exact result roots are:

- `f43ec29030d26edb6766ec9b83938ade` (8.14 seconds total)
- `225db757b8384107bd1c4466cbc16bbe` (8.05 seconds total)

Both canonical interaction documents and the separate diagnostic marker prove
component `qindaqt-shell`, action `qindaqt_toggle_notification_center`, and
pressed/released=true. Both preserve ShellDevelopment owner `:1.17`, shell and
service PID `53`, privacy=true, and WL-0 selection across input; the center
changes from closed, hidden, count `0` to open, visible, count `1`. The mapped
and committed compositor notification surface also has PID `53` and both
current/desired output WL-0.

All host display/input/session-bus reachability flags remain false. Resident
PSS is 175054 and 174131 KiB below the 1048576 KiB ceiling. Captures are intact
1920x1080 RGBA PNGs whose observed hashes match their evidence:
`4a647a70f61cda362f52771ef2f2de8dec97cd097bc3d86190a992322799bf3b`
and `7bf0237ce2168b2d036fb1941f1dc8c8fca5714f504e4b063643255dd0876371`;
they contain 74/75 sampled colors, with 16 in the required content region.
Teardown is bounded, terminal phases are recorded, survivor lists are empty,
and the fresh process audit is empty. CTest-created Python cache residue was
removed only after exact path/timestamp inspection.

The lane is terminally released. No full matrix, physical-hardware check,
package-signature check, commit, or freeze was performed. Awaiting the Program
Manager's next explicit gate.
