# Ada Ruiz — Settings1 daemon-loss repair checkpoint

- **Timestamp:** 2026-08-27T12:50:40-06:00
- **Base:** `2a1e2626e5d4e8e4526bfadbb8100931208f3179`
- **Status:** implementation and focused proof complete; full gates pending

The production composition now subscribes the exact `QDBusConnection` passed
to `ResidentSettingsService` to the standard local
`org.freedesktop.DBus.Local.Disconnected` signal before claiming the service.
That signal quits the resident event loop. The adjacent `AGENT-GUARD`, service
architecture page, ADR-0012, and test matrix record why reconnecting stale
in-memory repository authority is forbidden.

A separate cohesive process-lifecycle test writes a private activation
descriptor for the real production executable, starts a real private daemon,
activates Settings1 with `GetSnapshot`, records exact PID/owner/epoch, terminates
the daemon, and requires the PID to disappear within five seconds. It repeats
activation on a replacement daemon with a deliberately different owner slot,
requires a different live PID/owner/epoch, then terminates the replacement and
requires no survivor. An exact-executable RAII guard removes either fixture if
an assertion fails.

Evidence so far:

- Debug build and Settings1 selection: **16/16 passed**.
- Release build and Settings1 selection: **16/16 passed**.
- New real activation/daemon-loss/reactivation test: **10/10 consecutive** in
  Debug and **10/10 consecutive** in Release; each run completed in 0.05–0.14s.
- Documentation validation: **42 documents**, exit 0.
- Source-shape: **769 files**, zero skips/violations; new test is below the
  decomposition threshold.
- Working diff whitespace check: exit 0.

No live session bus, desktop, input, compositor, integration checkout,
reviewer worktree, or unrelated lane was touched. Next are full Debug/Release,
production/QML, strict docs, isolated install, and installed UnknownKey plus
daemon-loss/reactivation probes before one new commit.
