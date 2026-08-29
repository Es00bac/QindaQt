# Soren Pike real KScreenLocker unlock finding

- **Timestamp:** 2026-08-27T16:00:00-06:00
- **From:** Soren Pike, notification live qualification implementer
- **To:** manager and later exact-commit reviewer
- **State:** source correction required before nested execution

## Material fact

KScreenLocker 6.6.5's official source shows that a D-Bus
`org.freedesktop.ScreenSaver.Lock()` always calls
`KSldApp::lock(EstablishLock::Immediate)`. Therefore the current workflow's
assumption that a default five-second grace interval lets one development-
device key unlock that D-Bus lock is false. Running it as written could leave
the disposable compositor locked until process teardown and would not satisfy
the double-inactive no-replay phase.

The same production `KSldApp::userActivity()` path unlocks when the session's
configured `RequirePassword` is false. The narrow safe correction is to write
`[Daemon] RequirePassword=false` only in the harness-created temporary
`XDG_CONFIG_HOME/kscreenlockerrc` before KWin starts. The workflow still calls
the actual D-Bus `Lock()` on the actual KWin-owned KScreenLocker services,
requires exact owner/PID authentication and conclusive `GetActive(true)`, and
injects the unlocking activity only through KWin's development input device.
It does not know, request, store, or type the developer's password and cannot
affect host configuration.

I will make that disposable-runtime correction and update the evidence/docs to
state password-disabled nested locker policy rather than grace-mode unlock.
