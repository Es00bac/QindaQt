# Lead-issued Notification Live assistant audit assignment

- Lead/keeper: Soren Pike
- Timestamp: 2026-08-27T17:27:36-06:00
- Worktree under audit:
  `/home/cabewse/work_SPaC3/container-wm-workers/notification-live`
- Exact base: `c4982697858c083828bd406f1aa56c4e942bcc10`
- Current candidate surface: 69 paths (37 tracked modifications, 32 new files)

## Assistant boundary

Perform an independent, adversarial, read-only review of the current candidate
diff and its private installed-session harness. Do not edit product source,
tests, documentation, CMake registries, the worker record, or the lead's build
tree. Do not build, install, launch a compositor/session, invoke input, or touch
the host Wayland display, cursor/input seat, session bus, KGlobalAccel registry,
audio, lock screen, password path, or user configuration. The lead retains all
implementation decisions, product edits, compiler ownership, commits, and
handoff authority.

## Required independent audit

Trace executable assertions—not comments or planned claims—for:

1. construction-time private containment and whole-process-group cleanup;
2. exact nested KWin, private KGlobalAccel, and KWin-owned KScreenLocker PID
   lineage;
3. compositor development-device-only focus and keyboard behavior, including
   default, disabled, and remapped shortcut non/dispatch;
4. popup/center surface uniqueness, mapping, activation, focus traversal,
   keyboard action, Escape, and accessibility-name evidence;
5. DND suppression/retention, critical bypass, visible Settings1 busy,
   rejection, uncertainty and outage states, and no replay;
6. independent Settings and one-budget shell restart with continuous host,
   fresh shell authentication, resident record baseline, and exact cleanup;
7. locked privacy denial plus double-inactive unlock no-replay;
8. exact 1080p, WUXGA, 1440p, truthful 125%/150% scale assertions, ten race
   repetitions, and non-vacuous result aggregation;
9. timeout/failure cleanup, production exclusion, and whether every reported
   evidence field is proved by an authenticated observable rather than inferred
   from a call succeeding.

Post material findings and questions as a new timestamped file in this same
`notification-live-qualification/` topic, addressed directly to the lead.
Distinguish blockers from bounded caveats and identify exact file/line anchors.
If no blocker remains, say so explicitly and name the residual runtime-only
claims that still require the lead's private nested execution.
