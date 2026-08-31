# Dorothy Vaughan — package plus four-row S3 matrix green

- Timestamp: 2026-08-31T14:15:29-06:00
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/virtual-desktop-s3-selene`
- Build root: `/tmp/qindaqt-s3-selene-build`
- State: available; serialized private-bus/private-runtime lane released

The one authorized exact command exited 0 and passed 5/5 in 33.63 seconds:

```text
QINDAQT_PRIVATE_RUNTIME_LANE=interactive-virtual-desktop ctest --test-dir /tmp/qindaqt-s3-selene-build --parallel 1 --output-on-failure --stop-on-failure -R '^(desktop\.virtual\.package-contract|desktop\.virtual\.interactive\.matrix\.(single-wuxga|single-1440p-125|single-1080p-150|dual-1080p-horizontal))$'
```

Its preserved log is
`/tmp/qindaqt-s3-shell-readiness-full-matrix.log`. The result roots and audited
evidence are:

| Row | Result ID | PSS KiB | Capture | SHA-256 | colors/content |
| --- | --- | ---: | --- | --- | --- |
| WUXGA | `3f25519eaf5404e6d96fb9ae4e01be9f` | 181281 | 1920x1200 WL-0 | `07139e21de827672164f9fa2b4b6909f0a6254655f21fdd0312357454fb54279` | 53/16 |
| 1440p@125% | `59a629ea596d37ba47981edfb5785a96` | 177438 | 2560x1440 WL-0 | `162d09e77932d7adbacb6e428436951bc28f5a554da8df47396d4ecff130806d` | 56/16 |
| 1080p@150% | `5f3071bada4273f96936f731f3e0c714` | 170315 | 1920x1080 WL-0 | `088dff3cb1e8a77a73e5f40be14149947f1a6e5b5d639a8a9a6234078679cd7f` | 74/16 |
| dual 1080p | `f9a6b026c658a65efe84e404601b0748` | 239375 | 1920x1080 WL-1 | `cee51d130f212498e96a9c7ea5ecb3c71bca7d7b79a10dd65c91ce1c89b195f6` | 36/16 |

Every row is below the 1048576 KiB ceiling. In all four canonical interaction
documents and diagnostic markers, component `qindaqt-shell`, action
`qindaqt_toggle_notification_center`, and pressed/released are exact. Each row
keeps one stable owner and joined shell/service/compositor PID, privacy=true,
and changes the created center from closed, hidden, count `0` to open,
visible, count `1` on the selected output. The compositor notification surface
is mapped and committed with both current and desired output equal to that
selection.

All twelve host display/input/session-bus reachability flags are false. All
four artifact byte counts and computed image hashes equal their evidence;
captures have the required dimensions and nontrivial full/content color
regions. Teardown is bounded in all four rows, terminal phases are recorded,
survivor lists are empty, and fresh process inspection is empty.

For the dual row, canonical `postSelectorOutputs` is exactly WL-1 priority 1
then WL-0 priority 2; shell before/after state, compositor surface, and capture
all select WL-1. The raw `outputs` field intentionally remains pre-selector
WL-0/WL-1. An initial ad hoc audit mistakenly asserted post-selector order on
that raw field and exited nonzero; the corrected audit queried
`postSelectorOutputs` and passed all row assertions. No runtime retry occurred.

CTest-created Python cache files were removed after exact path/timestamp
inspection. The serialized runtime lane is terminally released. No commit,
freeze, physical-hardware claim, or package-signature claim was made. Awaiting
the Program Manager's next explicit gate.
