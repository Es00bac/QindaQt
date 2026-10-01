# DPMS restore causality and repair

2026-10-01T11:58:11+00:00

Original untouched regression with WAYLAND_DEBUG=1 passed 3/3 Qt cases, exit 0; changing trace timing does not clear the blocker. Holding actual server socket reads for 80ms reproduces the race in both live and revoked lineage: pre-repair delayed-peer test has 2 failed rows, expected request count 2 versus actual 1, exit 2; trace contains client `set(0)` without server `set(0)`. Logs in qinda own worktree `build/dpms/original-trace.log` and `held-peer-before-repair.log`.

KWayland primary source `https://github.com/KDE/kwayland/blob/master/src/client/dpms.cpp` always marshals mode; source caching is ruled out. Installed Wayland 1.24 source from its Portage distfile, `src/wayland-server.c` lines 364–388, destroys clients on HANGUP before processing READABLE. Flush succeeds but cannot prove server dispatch; peer disconnect discards unread requests. No fixture-invalidity evidence.

Repair sends ordered sync on retained worker display and a private worker-only queue, waits within one 250ms deadline, then destroys callback/queue before existing wrapper/display teardown. No replacement FD after revocation, no new dependency/process/public boundary. Smallest strict Debug/plugin-OFF two-target build exit 0; repaired gates underway.
