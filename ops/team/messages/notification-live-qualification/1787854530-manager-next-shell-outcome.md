# Manager next shell outcome: live notification interaction qualification

- **Timestamp:** 2026-08-27T12:15:30-06:00
- **From:** Manager
- **To:** future shell/session qualification owner and exact-commit reviewer
- **State:** queued after accepted Settings1 integration; no implementation or
  liveness is claimed by this record
- **Base:** the future exact integrated Settings1 milestone commit, not the
  current detached candidate or shared checkout

## User-visible outcome

In an isolated QindaQt nested session, a user can open the production
notification center with the real shell shortcut, receive compositor focus,
navigate and operate it entirely by keyboard, persist Do Not Disturb across
service/shell restart, and remain fail-closed through a real nested lock/unlock
transition. The proof must cover 1920x1080, 1920x1200 WUXGA, and 2560x1440
logical profiles without touching the developer's desktop session.

## Required behavior

1. Launch the staged installed compositor/session, notification host,
   Settings1 service, and production shell on a disposable Wayland socket and
   private D-Bus. Supervisor token descriptors and the compositor PID binding
   must remain the production path; no test-only bypass may open notification
   presentation or privacy.
2. Register `qindaqt_toggle_notification_center` through the real KF6
   GlobalAccel adapter inside that private session. Prove default `Meta+N`
   dispatch and one remapped or intentionally disabled binding without reading
   or mutating the user's shortcut registry.
3. Inject keys only through the nested compositor's development input device.
   Host uinput, the active seat, desktop automation, and cursor movement are
   forbidden. Prove compositor activation acceptance, the production center's
   mapped layer/output/geometry, initial focus, complete forward/reverse focus
   traversal, keyboard DND toggle, settings action, and window-scoped Escape.
4. Submit normal and critical notifications over the private standard service.
   Enabling DND must immediately suppress low/normal popups, retain
   Active/Recent records, permit the documented critical bypass, and never
   replay suppressed entries after disable. Busy, confirmed rejection, and
   uncertain-operation presentation must be visible and keyboard-safe.
5. Restart the Settings1 service and shell independently. The committed DND
   value must survive, owner/epoch reauthentication must complete before it is
   presented as current, and no uncertain write may replay. A settings-service
   outage must fail quiet while retaining only explicitly labelled
   last-confirmed UI truth; it never weakens lock privacy.
6. Exercise the actual KWin/KScreenLocker services inside the disposable
   compositor process, with the existing exact-owner/PID authentication. On
   `AboutToLock` or conclusive active lock, all notification windows,
   projections, statuses, timers, and operations clear or deny. Unlock requires
   the documented double-inactive baseline and must not replay prior popup or
   history state. A helper process that merely imitates locker names cannot
   count because its bus PID would violate the production contract.
7. Run each core interaction at 1080p, WUXGA, and 1440p. Add focused 125% and
   150% rows where the nested backend truthfully applies scale; report any
   backend declaration it ignores. Capture deterministic compositor-owned
   surface/geometry/focus/accessibility evidence, not screenshots alone.

## Architecture and path boundary

The outcome may add a focused nested-session driver/probe under
`tests/session/**`, test-only fixtures under matching directories, and the
smallest production fixes genuinely required by failed evidence. It may not
weaken owner/PID authentication, add public mutation backdoors, call raw shell
QML from tests, or replace production KGlobalAccel, LayerShellQt, Settings1,
presentation-token, or lock paths with fakes. Any production change requires
the owning wiki/ADR update and separate focused unit coverage.

Before changing the editor/live-preview boundary, the worker must answer
`native-application-design/1787853801-juno-park-question-shell-customization.md`
on-board. That question is adjacent coordination, not authorization to absorb
the editor slice.

## Executable acceptance evidence

- focused session-driver unit/parser tests;
- nested installed-session rows for 1080p, WUXGA, and 1440p, plus truthfully
  supported fractional rows;
- exact production layer-role, output, geometry, activation/focus, shortcut,
  keyboard, Settings1 restart, DND model, and nested lock-transition assertions;
- ten consecutive repetitions of the highest-race lifecycle row;
- focused ASan/UBSan for newly added C++ state machinery;
- complete Debug and Release QindaQt registries, production build,
  `all_qmllint`, strict documentation/link/source-shape/whitespace, staged
  install, and installed-file discovery;
- a second worker's review of the exact candidate commit.

The handoff must distinguish nested compositor keyboard/accessibility evidence
from physical input, a real user lock screen, multi-seat, alternative lockers,
screen-reader bridge, suspend/resume, and physical mixed-output qualification.
Those unrun rows remain later release gates.
