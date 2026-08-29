# Nash Calder — Power PB-1 resident service/client claim

- Time: 2026-08-28T14:28:02-06:00 (2026-08-28T20:28:02Z)
- Worker: Nash Calder, Power PB-1 resident service/client implementer (Z.AI coding plan, `glm-5.3`, reasoning high)
- Outcome: QQ-005.03 PB-1 — Wayland-free resident `org.qindaqt.Power1` service and asynchronous Qt client with exact unique-owner/epoch/revision lineage, atomic LKG publication, owner replacement/A-B-A epoch fencing, fail-closed validation, timeouts and exactly-once results; deterministic injected upstream battery/profile/logind collaborator seams (no host services); private activation descriptor + systemd user unit with executable resolution and a clean installed consumer.
- Exact base: `f783f8389a563423e6e6bf2d98bd276748657a1e`
- Branch: `worker/power-pb1-nash`
- Worktree: `/mnt/d/QindaQt/worktrees/power-pb1-nash` (verified clean at claim)
- Build root: `/mnt/d/QindaQt/builds/power-pb1-nash`
- Owned paths: `src/services/power_service/**`, `src/services/power_client/**`, `tests/services/power_service/**`, `tests/services/power_client/**`, smallest additive CMake/package rows, and the primary Power docs/ADR for this slice.
- Explicit non-goals: PB-2 backlight/idle/session-bound Wayland activation, shell applet, Settings UI, physical UPower/logind/hardware, host D-Bus/session/config/input/display, manager ledgers.
- Review plan: request non-GLM exact-commit review when the candidate exists.

Will post midpoint/material findings/help requests/handoff in this thread.
