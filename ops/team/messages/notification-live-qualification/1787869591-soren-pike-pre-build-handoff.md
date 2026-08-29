# Notification-live consolidated pre-build handoff

- **Timestamp:** 2026-08-27T16:26:31-06:00
- **From:** Soren Pike, notification live qualification
- **To:** Manager and future exact-candidate reviewer
- **State:** source-complete pre-build boundary; compile/nested execution held
- **Exact base/HEAD:** `c4982697858c083828bd406f1aa56c4e942bcc10`
- **Current diff inventory:** 69 paths: 37 tracked modifications and 32
  untracked additions; no commit yet

## Current vertical slice

The diff contains the bounded one-shell-restart supervisor contract and
ADR-0019; the development-only authenticated shell snapshot plus filtered KWin
notification-surface view and ADR-0020; compositor keyboard allowlist additions;
natural QML focus repair/coverage; and a modular installed notification-live
driver with private runtime/stage/process, keyboard, surface, shortcut, lock,
Settings1, resident-record, and workflow collaborators.

The executable matrix defines 1080p, WUXGA, 1440p, separate truthful 125% and
150% rows, and a ten-repetition lifecycle. It requires exact private service
PIDs (including KGlobalAccel and both KScreenLocker names at KWin), real KF6
default/disabled/remapped shortcuts, production keyboard focus/DND/settings/
Escape behavior, normal/critical/no-replay presentation, Settings1 rejection/
uncertainty/outage/recovery, real locked submission privacy and double-inactive
unlock, resident host state across one shell PID replacement, and exact-group
cleanup without host input/session/config interaction.

## Independent static review

The independent read-only auditor's final recheck is clean with no remaining
static blocker. Its findings drove five material repairs plus two follow-ups:
non-vacuous resident-record restart proof; POSIX `shlex.quote`; guarded complete
process-group cleanup including the SIGKILL disappearance race; exact
KGlobalAccel-to-KWin PID evidence; bounded predecessor-owner D-Bus registration
with no queue/replacement; truthful external private-bus containment wording;
and explicit failure of a development-marked shell without supervised
notification authority.

## Evidence already run

- `python tests/session/test_notification_live_unit.py`: pass, 7/7.
- `python -m py_compile` for all six driver/unit modules: pass.
- Compositor D-Bus descriptor parser against checked-in XML/metadata: pass.
- `git diff --check`: pass.
- `python tools/docs_validation.py --root .`: pass, 44 documents.
- `python -m tools.source_shape.cli`: pass, 799 files, no warning/error.

No compiler, qmltestrunner, nested compositor, sanitizer, package matrix, or
MkDocs build has been run since the source expansion; those remain mandatory
after the manager releases the compiler lane. No host Wayland/display, host
session bus, cursor/input, shortcut registry, KScreenLocker, uinput, user
configuration, or password path was accessed.
