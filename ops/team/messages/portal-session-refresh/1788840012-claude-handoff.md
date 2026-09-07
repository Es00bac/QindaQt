# Extend ADR-0094 resident-service refresh to the portal frontend/backend — handoff

- From: Claude
- At: 2026-09-06T22:00:12-06:00
- State: handoff, not live
- Base: `7d63ebcaebb0b621072c482431e5c155cf681de1` (main)
- Worktree: `/home/cabewse/work_SPaC3/container-wm/.cache/portal-session-refresh`
- Owned paths: `src/session_supervisor/src/resident_service_refresh.{h,cpp}`,
  `src/session_supervisor/app/main.cpp` (callsite only),
  `tests/session_supervisor/{tst_resident_service_refresh.cpp,session_resident_service_refresh_publisher.cpp}`,
  `docs/wiki/adr/0094-refresh-resident-wayland-session-services.md`,
  `docs/wiki/architecture/compositor-session.md`.

## Outcome

A live physical-session recovery on 2026-09-06 found a second cause in the
same class as ADR-0094: `xdg-desktop-portal` (PID 1966668, started 19:33 under
a prior KDE desktop) stayed resident across a later QindaQt login
(`qindaqt-0`, 20:17) and kept `WAYLAND_DISPLAY`/`XDG_CURRENT_DESKTOP=KDE`
from that prior desktop, because the portal frontend selects and caches its
backend once at its own startup rather than per call. Its routed-to backend,
`plasma-xdg-desktop-portal-kde.service`, is exposed to the same stale
environment through the KDE-only drop-in from ADR-0088. Restarting the
session's two portal user units (no host logout) recovered correct routing.

This candidate extends the fixed, reviewed resident-refresh list in
`residentServiceRefreshUnits()` (renamed from `residentWaylandServiceUnits()`,
with `refreshResidentWaylandServices()` renamed to `refreshResidentServices()`
and the callsite in `main.cpp` updated) from two units to four:
`qindaqt-clipboard-host.service`, `qindaqt-display-service.service`,
`xdg-desktop-portal.service`, `plasma-xdg-desktop-portal-kde.service`. The
rename corrects the contract: the header/ADR/architecture-doc language no
longer claims the list is limited to direct Wayland consumers, and does not
claim every D-Bus service needs a restart here — only ones that independently
cache desktop-scoped environment or routing state at their own startup. The
two pre-existing Display/Clipboard units and the best-effort
missing-unit/one-failure-does-not-stop-the-rest behavior are unchanged.

ADR-0094's Context/Decision/Consequences sections and
`docs/wiki/architecture/compositor-session.md` are updated to describe both
causes and the four-unit list; no other ADR or unrelated service/preference is
touched.

## Evidence

- Own focused configure: `cmake --preset dev -B build/own-focused` — exit 0
  (separate from any canonical build root; did not touch it).
- Focused build (`-j2`, targets `qindaqt_resident_service_refresh_tests`,
  `qindaqt_session_resident_service_refresh_publisher`, `qindaqt-session`):
  exit 0, 33/33 build steps, no warnings from the changed files.
- `ctest -R resident-service-refresh --output-on-failure`:
  `qindaqt.session-resident-service-refresh` — exit 0, 1/1. This private-bus
  test (fake `org.freedesktop.systemd1.Manager`) covers explicit required
  units/order and continues past one failing unit, plus
  `residentUnitListNamesOnlyReviewedResidentServices()` asserting the
  production four-unit list in order.
- `./tools/validate-docs`: exit 0, 189 Markdown documents and navigation.
- `mkdocs build --strict --site-dir /tmp/qq-docs-check` (docs venv): exit 0.
- `git diff --check`: exit 0.

## Bounded caveats and next action

No full/broad build, no nested/live/private desktop session was started; no
host D-Bus or systemd unit was touched. This does not add or change any other
resident-service entry, and does not claim the mechanism handles every
possible activation-environment scaling failure — only the two demonstrated
lifecycle causes (stale Wayland connection, stale cached portal routing).

Please assign a different reviewer for the exact commit against
`7d63ebcaebb0b621072c482431e5c155cf681de1`, then integrate into main with the
usual reconfigure + focused/broad verification.
