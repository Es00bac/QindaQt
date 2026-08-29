# Notification Live fresh Debug qualification claim

- Worker: Soren Pike
- Timestamp: 2026-08-27T17:09:28-06:00
- Exact source base: `c4982697858c083828bd406f1aa56c4e942bcc10`
- Branch/worktree: `worker/notification-live` at
  `/home/cabewse/work_SPaC3/container-wm-workers/notification-live`
- Preserved candidate state: 69 paths (37 tracked modifications and 32 new
  files); the independent final static audit reported no remaining blocker.
- Public-main compatibility: accepted main
  `94e84077e33a279dcebee24511e7dbdf1b87e3e1` overlaps only
  `docs/wiki/adr/index.md`, `docs/wiki/development/testing-harness.md`, and
  `mkdocs.yml`. Those changes are additive QST/Audio documentation and ADR
  registrations; no production source, test source, or CMake registry overlaps
  this candidate.
- Resource claim: Notification Live owns the manager-released single compiler
  lane. Build concurrency is capped at one.

## Immediate bounded execution

1. Configure a new current-source Debug tree using the repository developer
   switches, without modifying the older focused build tree.
2. Build that tree with `cmake --build ... --parallel 1`.
3. Run only focused notification, session-supervisor, runtime-options,
   development-input, and shell offscreen tests that do not launch installed
   nested KWin.
4. Stop compiler activity and report exact build/test results before any
   Release, sanitizer, package, or installed nested-session work.

The host Wayland display, cursor/input seat, session bus, shortcut registry,
audio, lock screen, password path, and user configuration remain prohibited.
All runtime fixtures in this pass are private/offscreen and test-owned.
