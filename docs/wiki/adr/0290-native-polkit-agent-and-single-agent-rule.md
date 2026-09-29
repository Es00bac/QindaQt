# ADR-0290: Native QindaQt polkit agent, and a single-agent autostart rule

- **Status:** Accepted
- **Date:** 2026-09-28
- **Owners:** Session supervision and platform
- **Supersedes:** None
- **Superseded by:** None

## Context

Two polkit authentication agents have registered at every QindaQt login:
the session supervisor starts a KDE agent candidate
(`src/session_supervisor/src/polkit_agent_selection.cpp`), and XDG autostart
independently runs `polkit-gnome-authentication-agent-1`, whose own
`.desktop` entry ships `NotShowIn=MATE;KDE` -- a list that never named
QindaQt, so it never excluded itself here. polkitd accepts whichever agent
registers first and shows a different winner each session. [ADR-0100](0100-own-desktop-essentials-in-a-session-process.md)
already recorded one live observation of the KDE agent's dialog appearing in
an installed session.

The plan is `docs/plans/2026-09-28-plasma-free-qindaqt.md` on the
`plan/plasma-free` branch (not yet merged to main, so not linked here),
section 3.4 (plan slice PF15), under the same guiding principle as every
other Plasma replacement in that plan: *"Each replacement is a native
QindaQt implementation designed to integrate cleanly with QindaQt... A
like-for-like port of the KDE component does not meet the bar."* The owner's
2026-09-28 decisions also fix that there is no Plasma fallback on either
host.

## Decision

`qindaqt-polkit-agent` (`src/apps/polkit_agent`, installed at
`${KDE_INSTALL_LIBEXECDIR}/qindaqt-polkit-agent`) is a native QindaQt app
built on polkit-qt6 (`sys-auth/polkit-qt`, CMake package `PolkitQt6-1`), not
a port of any existing agent:

- It subclasses `PolkitQt1::Agent::Listener` and registers for
  `PolkitQt1::UnixSessionSubject(getpid())` at `/org/qindaqt/PolkitAgent`.
  A registration conflict (another agent already holds the slot) is a clear
  stderr line and exit code 2, never a crash loop: the session supervisor's
  existing `OptionalSessionChild` one-restart budget already bounds this (it
  does not inspect the exit code, so one restart is attempted and then the
  child stays stopped for the rest of the session -- verified, not changed,
  and documented at the `m_polkitAgent->start()` call site).
- One request is ever open at a time; a second `initiateAuthentication`
  while one is open is queued, not dropped, and activates the moment the
  first completes. `cancelAuthentication()` (which carries no cookie in the
  installed polkit-qt6 API) closes whichever request is currently open.
- Session logic -- identity choice, attempt/retry/cancel/cancel-from-polkit
  transitions, request queueing, and requester (pid-to-program) resolution
  -- is pure C++ with no polkit type in its public interface, so it is
  unit-tested without polkitd, D-Bus, or a live session. Only
  `polkit_listener.cpp` includes a `PolkitQt1` header.
- The dialog is QindaTK QML (`QindaQt.Controls`, `QindaQt.Tokens`), so it
  follows the session's theme and tokens, including dark mode, exactly like
  every other first-party app. It is presented as a layer-shell overlay on
  the output under the pointer, with a token-coloured scrim and
  `KeyboardInteractivityExclusive`, so keystrokes cannot land in whatever
  window had focus underneath it. Enter authenticates, Escape cancels, and
  the password is never logged, never retained past the call that uses it,
  and cleared from the field after every attempt.
- `defaultPolkitAgentCandidates()` (`src/session_supervisor`) names only
  this agent's install path. There is no KDE or other distribution
  fallback, matching the owner's 2026-09-28 decision that the goal is to
  replace KDE and Plasma completely, not keep a fallback beside the native
  replacement. `--polkit-agent` and `--no-polkit-agent` remain as
  configuration escape hatches for an explicit override and for private/
  diagnostic sessions, which must never launch a host binary.
- The session autostart catalog (`src/session_autostart`, [ADR-0247](0247-run-xdg-autostart-in-the-session-supervisor.md))
  treats a fixed, documented table of distribution polkit-agent `Exec`/
  `TryExec` basenames (`polkit-gnome-authentication-agent-1`,
  `polkit-kde-authentication-agent-1`, `lxqt-policykit-agent`,
  `polkit-mate-authentication-agent-1`, `xfce-polkit`, `lxpolkit`) as
  ineligible, superseded by this agent, independent of that entry's own
  `NotShowIn`/`OnlyShowIn` content -- closing exactly the gap that let
  polkit-gnome's entry run uncontested. Every other autostart entry's
  eligibility is unchanged.

## Consequences

- Exactly one agent registers with polkitd per session; the nondeterministic
  winner ADR-0100 recorded is gone.
- Packaging must swap `kde-plasma/polkit-kde-agent` for `sys-auth/polkit-qt`
  as the session's polkit-agent dependency (the manager's change, not made
  here; see the packaging lines this change's handoff recommends).
- This change never registers the agent against a real polkitd or starts
  any service, per the workflow's live-session safety rule; the live
  behaviour (`pkexec true` in an installed session, then a wrong password,
  then the right one) is the owner's post-integration check, not evidence
  this change's own tests produced.
- The Details disclosure's vendor line reads polkit's `details` map key
  `polkit.action_vendor`, which has not been verified against a live
  polkitd (unlike `polkit.subject-pid`, which is well-established). An
  absent or wrong key degrades to an honest blank line, never a guess.
- `PolkitOverlaySurface` duplicates `shell_surface`'s small
  `LayerShellQt::Window` technique rather than depending on that module,
  because neither of its existing public surfaces (the panel-shaped
  `PanelSurfaceBackend`, the non-exclusive corner-anchored
  `LayerShellNotificationSurface`) fits a full-output, keyboard-exclusive
  scrim. See "Revisit when."
- [ADR-0100](0100-own-desktop-essentials-in-a-session-process.md) and
  [ADR-0247](0247-run-xdg-autostart-in-the-session-supervisor.md) each gain
  a dated section recording this change rather than being rewritten.

## Revisit when

A second consumer needs the same full-output, keyboard-exclusive layer-shell
overlay shape (the session-lock slice, PF5-8, is the likely candidate) --
promote `PolkitOverlaySurface`'s technique into `shell_surface` then, instead
of a third independent copy. Or: the owner's live check finds
`polkit.action_vendor` is not the key a real polkitd actually sends -- either
find the correct key or drop the vendor line rather than leave a comment
that claims more confidence than the evidence supports.
