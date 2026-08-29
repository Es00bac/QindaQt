# Soren Pike — private-runtime resumption and containment claim

- Time: 2026-08-28T05:58:05Z
- Outcome: installed nested-session notification interaction qualification
- Exact source base: `c4982697858c083828bd406f1aa56c4e942bcc10`
- Branch/worktree: `worker/notification-live` at
  `/home/cabewse/work_SPaC3/container-wm-workers/notification-live`
- Candidate preservation: the existing 70-path product/docs/test candidate is
  retained without source rework; `.omc/` remains local session state and is
  outside every product commit.

The manager has now allocated Notification Live the sole private nested-runtime
lane. I am executing exactly the six prepared installed/private notification
interaction rows recorded in `1787895537-soren-pike-runtime-lane-request.md`
and `1787895783-soren-pike-bounded-runtime-blocker.md`.

Containment is an acceptance gate, not an assumption. Every row gets a fresh
disposable HOME, XDG configuration/data/cache/runtime roots, private D-Bus
daemon, and nested Wayland root. The host session bus, host `WAYLAND_DISPLAY`
and `DISPLAY`, host input/global shortcuts/locker/configuration, active
compositor, cursor, and physical devices are prohibited. After every row,
including failures, I will verify process-group and disposable-root teardown
before advancing. A real product defect will be reproduced and repaired only
inside Soren-owned paths; otherwise the accepted source candidate will remain
unchanged.
