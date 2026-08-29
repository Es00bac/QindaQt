---
author: Babbage the 2nd
role: File Manager S0 runtime/package exact reviewer
timestamp: 2026-08-28T15:40:25Z
thread: first-party-native-apps
status: reviewing
candidate: 3fd38425127d2ecf76485a6e84e675460071f5d8
---

# Babbage the 2nd — File Manager exact-review midpoint

Completed gates on exact `3fd38425127d2ecf76485a6e84e675460071f5d8`
so far:

- detached HEAD/tree/parent and clean-state verification: PASS;
- exact seven-path manifest, ancestry, and `git diff --check`: PASS;
- every changed line plus File Manager wiki/ADR/module boundary, focused CMake
  package runner, and all four model/controller suites read: complete;
- bounded-ready static attack: no blocker found. The component's status signal
  and single-shot deadline are context-owned by the local event loop, no event
  processing occurs between the initial `Loading` check and connections, loop
  exit destroys both connections/timer, `create()` remains after `Ready`, and
  the engine/component stay caller-owned on the GUI thread;
- CLI ordering/test strength static attack: PASS. More than one argument still
  exits 2 first; a supplied non-folder now exits 4 before intentionally missing
  theme lookup, making the regression non-vacuous;
- preservation audit: no model, launcher, controller, desktop, or QML source
  changed, so the already exact-reviewed read-only navigation/launch/keyboard/
  accessibility boundary is not bypassed by this runtime-only descendant;
- fresh strict Debug/shared/testing configure with KWin/production shell/host
  uinput off: PASS. Serial five-target build is active and was at 59/138 at
  this timestamp.

Still required before verdict: complete that build; independently execute the
exact eight File Manager rows; inspect the staged executable's dynamic section,
resolved libraries, payload/import confinement, and real sanitized QML-root
probe; establish parent-failure/non-vacuity evidence; run source/doc/diff/static
gates; then post exact severity counts and next action. No product/Git/host
session/input/config/user-data mutation has occurred.
