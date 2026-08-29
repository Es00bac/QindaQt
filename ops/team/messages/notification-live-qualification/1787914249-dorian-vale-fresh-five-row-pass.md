# Dorian Vale — fresh five-row private matrix passes

- **Timestamp:** 2026-08-28T10:50:49Z
- **Exact candidate:** `557260a50faaf083733afe5972ad6541ef398108`
- **Tree:** `8f9f131461157b33bb88e0b4a46811e2308c9329`
- **Status:** working; race-10x and teardown proof remain live

After completing the exact Debug build/install graph in the detached review
worktree, I replayed the five installed private Notification Live rows from the
candidate's own CTest registrations. All five passed, 5/5 in 49.19 seconds:

- `shell.notification-live.1080p` — 10.78 s, logical 1920×1080 at scale 1.0
- `shell.notification-live.wuxga` — 9.62 s, logical 1920×1200 at scale 1.0
- `shell.notification-live.1440p` — 9.63 s, logical 2560×1440 at scale 1.0
- `shell.notification-live.scale-125` — 9.53 s, logical 1536×864 at scale 1.25
- `shell.notification-live.scale-150` — 9.62 s, logical 1280×720 at scale 1.5

Each fresh JSON result reports `passed: true`, fresh shell PID after restart,
continuous notification-host PID, authenticated replacement shell, 50
development-input requests in the primary phase, forward/reverse focus,
default/disabled/remapped shortcut behavior, DND suppression/critical bypass,
private lock privacy and no replay, and explicit rejected/uncertain/outage/
settings-restart/shell-restart phases. No blocking finding is present.

The review now moves to the exact `shell.notification-live.race-10x` replay,
then residual process/runtime-root teardown and staged plugin ABI confirmation.
The harness used disposable private HOME/XDG/session-bus/process-group state;
no host display, input, bus, configuration, hardware, or desktop state was
accessed.
