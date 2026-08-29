# Keira Dunn claim: Status-notifier tray S0 source/static foundation

- **Timestamp:** 2026-08-28T13:32:37Z
- **Status:** claimed; implementation active
- **Exact base:** `9db68c4023257b49421101fa1b13c73bbc2cfa85` (public `main`)
- **Branch:** `worker/system-tray-s0`
- **Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/system-tray-s0`

I own one bounded outcome: a StatusNotifier source/static foundation with
validated item/icon/menu/status values, an exact-owner keyed registry with
replacement/removal, bounded icon/menu payloads, activation/context-menu/
secondary-activation request intents, loading/empty/degraded presentation, and
keyboard/accessibility identities, proven by hostile tests for spoofed owner,
stale reply, malformed menu/icon, duplicate identity, and restart.

## Path ownership

New tree only: `src/shell/status_notifier/**` and `tests/shell/status_notifier/**`,
plus the primary wiki page, one ADR, and their navigation/links. I will not
touch production shell code, applet registries, shared CMake wiring, the
roadmap, or any other lane's paths. No session D-Bus, host tray app, GUI,
session launch, or action execution will occur; transport is an injected
interface with fakes only.

## Completion evidence

Because this slice is source/static by assignment, evidence is the exact
candidate commit plus static gates: `git diff --check`, repository
source-shape, dependency-free documentation validation, and Qt-header-only
syntax checks of every new C++ file. CTest compilation/execution requires the
manager's shared build wiring and is explicitly requested as integrator/reviewer
action.

## Collision and dependency risks

None known. `src/shell/status_notifier` and `tests/shell/status_notifier` are
new directories; nobody else lists them. The integration branch will need two
one-line `add_subdirectory` additions (parent `src/CMakeLists.txt` and
`tests/CMakeLists.txt`), which I leave to the manager under the shared-build
coordination rule.
