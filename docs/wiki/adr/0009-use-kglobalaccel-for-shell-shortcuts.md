# ADR-0009: Use KGlobalAccel for shell-wide shortcuts

- **Status:** Accepted
- **Date:** 2026-08-26
- **Owners:** Shell
- **Supersedes:** None
- **Superseded by:** None

## Context

QindaQt shell functions such as opening the notification center must remain
reachable when no panel has keyboard focus and when a user-created profile
omits the corresponding applet. Wayland clients cannot safely implement this
with ordinary application shortcuts or raw input grabs. QindaQt already ships
KWin and its Hybrid compositor integration already uses KDE Frameworks'
KGlobalAccel client API for configurable global shortcuts.

Creating another shortcut daemon or adding a private compositor command for
each shell action would duplicate session-wide conflict handling, persistence,
and configuration. Registering shell presentation callbacks inside the KWin
plug-in would instead introduce a compositor-to-shell command protocol and make
the compositor own shell UI policy.

## Decision

The production shell will use the KF6 KGlobalAccel client library for
shell-owned global shortcuts. Each shortcut is represented by a `QAction` with
a stable object name and descriptive text. QindaQt registers both its default
and active sequence with `KGlobalAccel::Autoloading`, preserving a user's
remapped or disabled binding instead of reclaiming the default at startup.

The first action uses stable identity `qindaqt_toggle_notification_center` and
default sequence `Meta+N`. Its lifetime is scoped to the authenticated
notification presentation runtime; a standalone shell without the supervisor's
presentation descriptor does not expose an inert global action.

The shell retains each action for that complete runtime and connects it only to
a narrow shell-owned controller. Applet QML does not register global shortcuts,
and the compositor does not receive notification-presentation authority.

Registration failure is recoverable. The shell distinguishes whether
KGlobalAccel accepted both setter requests from whether an active sequence is
observable afterward. Setter acceptance alone does not prove that the service
or binding is active. Conversely, an empty active sequence can mean intentional
user disablement, a conflict, or unavailable service state and must not be
reclaimed or reported as an unconditional error. A setter refusal is logged;
the shell continues rather than aborting the desktop. Stock layouts retain the
pointer-accessible applet entry, while a custom layout remains free to omit it.
The shell does not silently install a process-local shortcut that only works
when a panel happens to have focus.

Tests use an injected registrar to verify stable IDs, defaults, dispatch, and
active-binding state changes without modifying the developer's session
shortcut registry. Real registration, dispatch, disablement, remapping, and
recovery remain isolated-session qualification.

## Consequences

- The production shell gains a mandatory KF6 GlobalAccel dependency while the
  preview and pure applet/model libraries remain Qt-only.
- KGlobalAccel/KWin remains the one session-wide authority for shortcut
  conflict resolution and persisted remapping.
- Shortcut callbacks may toggle shell presentation but do not grant applets
  access to the full notification model.
- Request acceptance and observable active-binding state remain separate so
  later diagnostics or settings UI can report them without treating an
  intentionally disabled shortcut as a registration failure.
- Settings UI for remapping can consume the same stable action identity later;
  no QindaQt-specific shortcut persistence format is introduced.

## Alternatives considered

- **Register the shortcut in the compositor plug-in.** Rejected because a
  notification-center command would couple compositor policy to shell UI and
  require a new cross-process mutation boundary.
- **Use a focused-window Qt shortcut.** Rejected because layer-shell panels do
  not normally own keyboard focus and the binding would not be global.
- **Implement a QindaQt shortcut daemon.** Rejected because KGlobalAccel already
  supplies conflict handling, persistence, remapping, and Wayland integration
  in the required KWin session.
- **Make the notification applet mandatory and non-removable.** Rejected because
  QindaQt profiles remain user-composable; keyboard reachability must not depend
  on one panel layout.

## Revisit when

Reconsider the client boundary if QindaQt no longer runs on a KWin session, or
if a standard Wayland/freedesktop global-shortcut protocol provides equivalent
conflict handling, persistence, remapping, and application identity without a
QindaQt-specific daemon or compositor command.
