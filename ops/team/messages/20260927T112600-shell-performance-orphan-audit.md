# Read-only private desktop orphan audit

- Timestamp: 2026-09-27T11:26:37-06:00
- Worker: shell-performance-codex
- Scope: process metadata, selected nonsecret environment, socket liveness and memory; no termination or product changes.
- Requested action: manager-owned identity-checked cleanup of the 18 dead private-bus portal backends after shell A/B; track website harness ownership/lifetime separately.

All 18 laptop processes are `/usr/libexec/xdg-desktop-portal-kde`, PPID 1,
with the same executable and start ticks rechecked at 2026-09-27T11:25:49.557281-06:00.
Each private `/tmp/dbus-*` pathname is absent, has no entry in `/proc/net/unix`,
and its process session leader is gone. Open stdout/stderr FDs identify website
`build/site-capture/record-live.log`, per-profile capture logs or profile-batch.log.
This establishes leftover backends from completed private captures; none is the
active host portal service. Root must exclude host PIDs 1212, 1702690, 1702692.

Exact identity evidence (boot ID `d7879a70-9a6b-444f-b412-6e28c5a1e145`, 100 clock ticks/second):

| PID | Start ticks | Gone session leader |
| --- | --- | --- |
| 1602334 | 23817920 | 1602280 |
| 1606386 | 23846276 | 1606341 |
| 1607401 | 23853169 | 1607348 |
| 1608128 | 23854963 | 1607348 |
| 1608948 | 23860510 | 1608903 |
| 1609915 | 23867581 | 1609870 |
| 1613937 | 23961860 | 1613889 |
| 1683219 | 24542516 | 1683150 |
| 1683676 | 24543344 | 1683150 |
| 1684151 | 24544169 | 1683150 |
| 1685070 | 24545014 | 1683150 |
| 1685648 | 24545836 | 1683150 |
| 1801161 | 25499089 | 1801109 |
| 1801786 | 25499907 | 1801109 |
| 1802427 | 25500727 | 1801109 |
| 1803047 | 25501549 | 1801109 |
| 1803656 | 25502368 | 1801109 |
| 1885476 | 26598891 | 1885427 |

All use host PID/mount/network namespaces `4026531836`, `4026531832`,
`4026531833` and inherited `XDG_RUNTIME_DIR=/run/user/1000`,
`WAYLAND_DISPLAY=qindaqt-0`. That host Wayland listener remains live, and sampled
portal sockets remain connected to it. Thus the private **buses** are dead;
these were never isolated runtime/namespace processes. Manager cleanup must
revalidate boot ID, PID start ticks, executable and absent private bus immediately
before acting, rather than matching names or relying on PPID alone.

Selected raw evidence is intentionally ignored local output at
`build/orphan-audit.json` in the performance worker worktree. It contains all
18 PID/start ticks, exact absent bus paths, session identities, namespace IDs,
selected environment, open log paths and per-process memory. It is available to
root in this shared filesystem and is not committed machine state.

Memory at sampling: summed RSS 723,196 KiB (706 MiB), PSS 25,970 KiB (25 MiB),
Private_Clean + Private_Dirty 872 KiB, Swap/SwapPss 103,588 KiB (101 MiB).
RSS repeats shared library pages and must not be described as 706 MiB of unique
resident leakage. The processes total only 73 CPU ticks since creation at the
recheck. This establishes leftover resources, **not** a cause of the observed
aged-session dock slowdown.

## Existing guards and remaining harness gap

ADR-0276 and private-session-isolation worker repair `8bcfa9fa`/`c0e976cb`
prevent nested session helpers publishing activation environment to the host or
restarting its services. They do not own/reap arbitrary private broker-activated
children. The bubblewrap desktop-session harness separately suppresses Qt/GTK
portal activation (`QT_NO_XDG_DESKTOP_PORTAL=1`, `GTK_USE_PORTAL=0`) and owns its
PID namespace. The observed site-capture runs did not use that harness.

Read-only inspection of laptop website source `d93bc7b0` plus its existing local
edits shows `capture-profile-live.sh` and `record-live.sh` launch dbus-run-session
before replacing capture environment. The profile harness keeps host runtime;
record-live only creates a private runtime for audio scenes. Neither script
suppresses portal activation or explicitly owns/reaps its activated descendants.
Their cleanup stops known compositor/app/audio children only. The website tree
is absent from origin/main in this worker, so its existing separate lane remains
owner; no changes were made there. The source gap permits recurrence; no new
capture was launched to reproduce it during this audit.

## qinda Weston: leave untouched

PID 1661631, start ticks 18302266, started 2026-09-24 21:45:54, has PPID 1 and
gone session leader 1661624. It is `weston --backend=headless --renderer=gl
--socket=qtk-0 --width=1280 --height=800 --idle-time=0`, cwd QindaTK, with an open
`weston.log` in an old agent scratchpad. Runtime `/run/user/1000/qtkw` has a live
`qtk-0` listener. Its only observed connected Wayland peers are its stock
weston-keyboard (1661658) and weston-desktop-shell (1661659), both start ticks
18302279. It uses the **live host bus** `/run/user/1000/bus`, unlike the portals.
The compositor alone has RSS 9,912 KiB, PSS 1,678 KiB and SwapPss 23,688 KiB.
This looks like a leftover headless fixture but does not prove no owner intends
to reconnect; it is not included in the safe-clean portal set.

Read-only checks completed successfully; all 18 original process identities
and bus/session absence rechecked. No user config contents, credentials,
process termination, live-session mutation, build mutation or product edits.
