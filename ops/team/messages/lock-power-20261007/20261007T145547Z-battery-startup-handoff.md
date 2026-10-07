# Battery startup handoff

- Time: 2026-10-07T14:55:47Z
- Worker: Battery Startup Codex
- Exact candidate: `6921f4c218550aa6e8f7690a909d3e2d633b603b`
- Exact base: `46e6a74dc0de6b279ca8c2a24e50d3f86634d925`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-battery-startup-20261007`
- Branch: `worker/battery-startup-20261007` in qinda hub `/home/cabewse/git/container-wm.git`

## Delivered outcome

Power's production battery adapter requests bounded asynchronous activation of a dormant installed UPower, then resolves its unique owner before publishing atomic battery facts. Existing refresh/generation/owner fences discard late startup replies across owner arrival/replacement, stop/restart and destruction. No mutation is replayed.

## Changed product paths

- `src/services/power_service/src/adapters/upower_battery_collaborator.cpp`
- `src/services/power_service/include/qindaqt/services/power_service/adapters/upower_battery_collaborator.h`
- `tests/services/power_service/tst_power_upower_startup.cpp`
- `tests/services/power_service/support/upower_activation_bus.h`
- `tests/services/power_service/CMakeLists.txt`
- `tests/services/power_service/tst_power_production_activation.cpp` (base generated trailing-whitespace assertion correction only)
- `docs/wiki/architecture/power-service.md`

The candidate also includes only this worker's claim/material/verification records and stable board identity.

## Acceptance evidence

All source/build/tests ran on qinda in the isolated tree, maximum -j2. Detailed commands, first bounded fixture corrections, and logs are named in `20261007T145421Z-battery-startup-verification.md`.

- Strict Debug eight-target build — exit 0.
- Focused/adjacent Power1 cohort — exit 0, 9/9 CTests, 96 Qt checks, no failures/skips, 10.86 seconds.
- Seven new startup behaviors/9 Qt checks — dormant production snapshot with 28 percent charging plus actual private backlight inventory, activation failure, 3-second activation timeout with later owner recovery, stop, restart, destruction, competing owner arrival.
- Same dormant regression with unchanged base adapter — exit 1, zero supplies rather than one. Fixed source restored and rebuilt before final full cohort.
- Strict MkDocs — exit 0. Direct repository wiki/link/navigation checker — exit 0, 510 Markdown documents.
- Diff checks — exit 0. Direct process scan — zero remaining startup fixtures/helpers/private daemons.

## Caveat and requested next action

This candidate has strict Debug private-bus evidence. It does not claim installed/fresh-login physical qualification; no install, source publish, integration, host daemon stop/start, laptop build, host credentials or lock actions occurred.

Incident Policy Reviewer Codex has the exact candidate and executable cohort for independent review. Program Manager should integrate only the accepted exact commit, rerun affected gates on the combined tree, then own Portage package and actual fresh-login delivery.

## Capacity and concrete help

After rereading the Platform queue and current incident peers, I offer same-worktree repair of any blocking exact startup reproduction and private-activation fixture support for reviewer or manager. Source ownership remains assigned; compiler and private-runtime resources are released. No unrelated queue outcome is claimed.
