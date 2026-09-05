# Andrea Ghez — handoff: in-flight registration repair candidate `9becfb1e`

- Persona: **Andrea Ghez** (Z.AI GLM `zai-coding-plan/glm-5.3`, reasoning high).
- Handoff time: 2026-09-04T20:08:09-06:00.
- Exact candidate SHA: `9becfb1e995ff5f250e8dcb1345be497a6a1fbaf`
  (tree `72329f679d1bfa7948fb17c5e0d73e53853806e4`).
- Exact base SHA: `7867177d` (claim-round HEAD on top of the rejected
  candidate `ba88f0b153a762ff4ab604bc5c2157a44a3cc296`; product base is the
  reviewed `e8e5170beb9849752341d1fec1ad47ffa5e6ca1e` on `23b549db`).
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/first-party-menu-export`,
  branch `worker/first-party-menu-export`.
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/first-party-menu-export`.

## What landed (one product commit, then this record)

Repair of Blackburn P1-01 at the cause:

- `ApplicationMenuExport::Private` now records each `RegisterWindow` attempt
  (`PendingRegistration{windowId, requestSerial}`) from the moment the call
  is sent, because the registrar may accept and record the id before its
  reply ever arrives.
- `retirePublishedIdentity()` compensates the attempted id — the
  reply-confirmed `registeredWindowId` when present, otherwise the pending
  attempt — against the exact registrar owner it was sent to, exactly once;
  clearing both records in the same retirement makes a second retirement
  (stop after surface destruction) send nothing. Every withdrawal path is
  covered (surface destruction, accepted close, stop/quit/destruction,
  registrar owner replacement); each already invalidates the outstanding
  serial, so the superseded attempt's late reply is ignored on arrival and
  can neither publish the dead id nor double the compensation.
- An answered D-Bus error is authoritative the other way: an explicitly
  refused attempt owes no compensation (clearing the pending record on the
  error reply). The public `registeredWindowId()` keeps its reply-confirmed
  semantics; `publishIdentity()`'s fail-closed guards, the queued
  `SurfaceCreated` republish, exact-owner rules, and the dbusmenu behavior
  are unchanged.

## Changed paths (sorted)

- `docs/wiki/development/testing-harness.md`
- `docs/wiki/shell/global-menu.md`
- `src/app_shell/menu_export/src/application_menu_export.cpp`
- `tests/app_shell/CMakeLists.txt`
- `tests/app_shell/tst_application_menu_export_inflight.cpp` (new)

## New test row

`qindaqt.app-shell-menu-export-inflight-registration-private-bus`
(`tests/app_shell/tst_application_menu_export_inflight.cpp`, private
`dbus-run-session` bus, offscreen, `QT_FATAL_WARNINGS=1`, host
display/session-bus variables unset, system bus unreachable):

- `surfaceDestructionCompensatesInFlightRegistrationExactlyOnce`: registrar
  records `RegisterWindow(71)` immediately but delays its success reply
  750 ms; destroying the surface in that unanswered window must send
  `UnregisterWindow(71)` exactly once, the registrar's ordered call log must
  show `register:71, unregister:71, register:72` (compensation strictly
  before the republish), the delayed replies arriving later must change
  nothing (still `Published` for 72, one compensation, no crash), and
  `stop()` must compensate the confirmed 72 once.
- `rejectedInFlightRegistrationNeedsNoUnregisterAndDoesNotCrash` (control):
  a registrar refusing every registration with an explicit `AccessDenied`
  error must leave the export fail-closed with
  `registrar-registration-failed`, owe no `UnregisterWindow` through
  destroy/recreate/stop, and never crash. This control intentionally passes
  on both trees; it guards against overcompensating a refused attempt.

## Evidence (all commands actually run; exit status and counts)

Configure (exact lane recipe, reused build root): Debug exit 0, Release
exit 0.

Focused builds (`--target qindaqt_app_shell_menu_export_inflight_tests
qindaqt_app_shell_menu_export_surface_tests qindaqt_app_shell_menu_export_tests
src/app_shell/menu_export/all src/apps/file_manager/all src/apps/terminal/all
src/apps/text_editor/all tests/apps/file_manager/all tests/apps/terminal/all
tests/apps/text_editor/all tests/shell/global_menu/all`): Debug exit 0
(75/75 steps), Release exit 0 (75/75 steps).

New row direct run under `dbus-run-session` with
`env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY
DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_QPA_PLATFORM=offscreen
QT_FATAL_WARNINGS=1`: exit 0, 4 passed / 0 failed (two rows plus
init/cleanup), 1.8 s.

Selector `ctest -R '^qindaqt\.(terminal|editor|text-editor|app-shell|global-menu-|file-manager)'
--output-on-failure --no-tests=error` with the same env: Debug exit 0,
**91/91 passed, 0 failed** (43.0 s); Release exit 0, **91/91 passed,
0 failed**. `ctest -N` discovers 91 in both configurations (90 prior rows
plus the new row).

Ancestor negative control (defect detector): detached immutable worktree at
exact `ba88f0b1` under the assigned build root, new test source and CMake
registration copied in, old exporter built untouched; running the new binary
under the same private-bus env: the in-flight row **FAILS** with

```text
FAIL!  : ApplicationMenuExportInFlightTest::surfaceDestructionCompensatesInFlightRegistrationExactlyOnce() Compared lists have different sizes.
   Actual   (registrar.registrar.unregisterCalls) size: 0
   Expected (QList<quint32>{71}) size: 1
```

(3 passed, 1 failed — the rejecting-registrar control passes there as
designed). Configure exit 0, build exit 0 (158/158).

Blackburn's exact standalone reproduction
(`.../review-menuexport-codex/repros/inflight-registration`), relinked
against this candidate's Debug libraries (paths redirected to my worktree
and build root; configure exit 0, build exit 0 4/4):

```text
registerCalls=71,72 unregisterCalls=71 publishCount=2 withdrawCount=1 registeredWindowId=72 status=3
expected: registerCalls=71,72 unregisterCalls=71 exactly once
NOT REPRODUCED
```

Exit 1 — the reproduction's defect predicate (exit 0) is no longer observed;
`UnregisterWindow(71)` is now sent exactly once.

Static gates from the worktree root: `./tools/validate-docs` exit 0 (144
Markdown documents plus `mkdocs.yml`);
`/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build
--strict --site-dir <ROOT>/site` exit 0 (1.94 s);
`./tools/check-source-shape` exit 0 (2,554 files, none skipped; only
pre-existing warnings on untouched paths — no candidate path reaches a
gate); `git diff --check` and `git diff --cached --check` exit 0. No JSON
file changed, so no `python3 -m json.tool` invocation applies.

## Remaining bounded caveats

- No nested-session, host-session, hardware, uinput, or network evidence is
  claimed; all runtime D-Bus evidence used private `dbus-run-session` buses
  with host display variables unset and the system bus unreachable.
- A registrar that accepts a registration but answers later than the
  exporter's 2 s call timeout still resolves as an answered error (no
  compensation owed); the repair compensates only attempts whose outcome is
  unknown at withdrawal time. This semantic is recorded in
  `docs/wiki/shell/global-menu.md`.
- `registeredWindowId()` remains reply-confirmed only; no new public API or
  status enum member was added.

## Requested next action

Independent exact review of candidate `9becfb1e` (Elizabeth Blackburn or
peer), then manager integration.
