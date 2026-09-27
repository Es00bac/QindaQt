# Core recovery handoff

- Timestamp: 2026-09-27T16:44:19.975456+00:00
- Candidate: gabbee `f7ec6b2` (branch `fix/recovery-coverage-20260927`, pushed to qinda hub)
- Base: gabbee `794ed46`; desktop audit base `64ae95c3`.
- Paths: Gabbee watchdog/desktop recovery/reporting modules, systemd drop-ins/source installer, README/recovery operations, focused tests.
- Evidence: `PYTHONPATH=src QT_QPA_PLATFORM=offscreen python3 -m pytest -q tests/test_voice_watchdog.py tests/test_recovery_reports.py tests/test_desktop_recovery.py tests/test_recovery_backup.py tests/test_backup_sources.py tests/test_qindaqt_voice.py` — exit 0, 83 passed. `git diff --check` — exit 0.

## Findings

Both hosts installed QindaQt r1 and Gabbee 20260926 with identical supervisor and recovery-module SHA256 hashes. Both timers active and statically linked from vendor timers.target.wants, despite is-enabled reporting disabled. Laptop delivered 22 incidents to qinda. Workstation has two queued dictation recovery incidents, zero sent; loopback SSH public-key failure prevented delivery. Laptop Clipboard has no physical-bus owner and unit start-limit-hit; repeated private-session restarts exhausted bounded recovery. Healthy circuits intentionally do not clear this limit.

Coverage now includes network, Bluetooth, power, appearance portal and missing D-Bus-activated Settings1 as well as existing audio/clipboard/display/Voice1. Settings GUI processes and hung-but-name-owning services are outside the crash/missing-owner coverage. No physical crash injection, restart or user configuration mutation was performed by this worker.

Runtime EnvironmentFile pins can override the supervisor's newly published environment at the next login. Templates now inherit manager environment; KDE backend retains static identity. Remove obsolete known generated pin files during deployment and inspect legacy user overrides before replacing them.

## Requested next action

Independent review exact Gabbee candidate, then root integration and overlay revision. Extend ebuild recovery.conf unit loop to qindaqt-network-service.service, qindaqt-bluetooth-service.service, qindaqt-power-service.service, xdg-desktop-portal-qindaqt.service, retaining existing audio/clipboard/display. Install both hosts, reload managers, verify installed hashes and drop-ins, then repair laptop Clipboard once under manager control. Report writer local fixture is safe with temporary HOME; never run watchdog main as a private verification because it discovers physical sessions.

Remaining caveat: fresh physical logout/login is not exercised while user preserves degraded session; source regression covers environment transition and no stale drop-in override.
