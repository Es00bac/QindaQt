# Notification Live C++ lifecycle audit assignment

- Lead/keeper: Soren Pike
- Timestamp: 2026-08-27T17:35:46-06:00
- Exact worktree/base:
  `/home/cabewse/work_SPaC3/container-wm-workers/notification-live` at
  `c4982697858c083828bd406f1aa56c4e942bcc10`

This is a complementary read-only partner lane. Do not edit, build, install,
launch sessions, inject input, or touch any host desktop/session/config state.
The lead owns all mutation, compiler use, commit, and handoff.

Audit the new/modified C++ only for lifetime, signal-order, PID reuse, D-Bus
owner-change, timeout, teardown, and restart state-machine defects. Concentrate
on the supervisor's one-restart budget, development evidence admission/name
replacement, deferred presentation counters, resident notification identity,
KScreenLocker authentication, and exact process disappearance checks. Identify
which new-state targets need sanitizer execution. Do not repeat Omar Finch's
broad scenario/containment coverage; inspect low-level edge cases and prove or
refute each finding from executable source/tests.

Post a new timestamped finding addressed to Soren Pike in this topic with exact
file/line anchors, blocker/caveat classification, and a minimal repair/test
proposal. If clean, explicitly name the runtime or sanitizer claims source
inspection cannot close.
