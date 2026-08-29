# P2: activated Settings1 service survives permanent session-bus loss

- **Timestamp:** 2026-08-27T12:39:05-06:00
- **Exact candidate:** `2a1e2626e5d4e8e4526bfadbb8100931208f3179`
- **Severity:** P2 — blocks release/integration
- **Evidence boundary:** prefix-aware staged install, isolated `dbus-run-session`,
  reviewer-owned XDG directories; no user session or desktop

The installed `org.qindaqt.Settings1` activation path starts successfully, but
the resident process does not terminate or recover when that private session bus
goes away. I reproduced this three times. In the final controlled reproduction,
`dbus-run-session` completed with status 0 after a successful staged
`GetSnapshot`, yet one second later exact staged PID `3274080` remained `Sl`
with PPID 1561 and retained the now-dead private address
`unix:path=/tmp/dbus-otIDz0sbQz,...`. I terminated that exact fixture PID and
verified cleanup. Logs are under
`build/settings-final-review-runtime/orphan-repro-{activation,process}.log` in
the detached reviewer worktree.

`src/services/settings_service/src/main.cpp:48-57` checks startup once and then
runs the event loop unconditionally. `ResidentSettingsService::start()` checks
`isConnected()` only before registration
(`src/services/settings_service/src/resident_settings_service.cpp:87-101`),
while `isRunning()` later reports only in-memory repository/name state and does
not inspect the connection (`:219-222`). There is no disconnect handler that
stops the service, exits the application, or reconnects and obtains a fresh
owner/epoch. Consequently this process can never serve on a replacement bus;
repeated daemon loss accumulates unreachable resident processes and stale
in-memory authority.

The existing lifecycle test proves explicit `stop()`/release/restart while its
daemon remains alive, then destroys service objects before terminating the bus
(`tests/services/settings_service/tst_settings_service_lifecycle.cpp:238-250`).
It does not exercise the installed resident process across daemon loss. This is
not an intentional lifetime documented by the service contract: the service is
the independently activatable session-bus authority
(`docs/wiki/architecture/settings-service.md:3-7`), and the accepted test
contract claims local daemon-loss coverage
(`docs/wiki/development/testing-harness.md:338-353`).

Repair should define and test the resident policy for permanent bus loss. The
minimal deterministic activation policy is to terminate promptly after the
constructing session-bus connection disconnects, so a replacement daemon can
activate a fresh process/owner/epoch. A repair must be committed by the
implementer and this reviewer must recheck that exact commit; the current exact
candidate is not acceptable for integration.
