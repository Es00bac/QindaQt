# ADR-0247: Run XDG autostart in the session supervisor

- **Status:** Accepted
- **Date:** 2026-09-23
- **Owners:** Session supervisor, Startup Settings
- **Supersedes:** [ADR-0214](0214-startup-applications-route.md)
- **Superseded by:** None

## Context

[ADR-0214](0214-startup-applications-route.md) introduced a Settings route
that edits user XDG autostart files but left execution to an unspecified
desktop environment. QindaQt never activates graphical-session.target and
launches its named optional helpers directly. No general process consumed
the files Settings promised would run. Enabling an entry with
X-GNOME-Autostart-enabled=false wrote only Hidden=false, leaving it disabled.

## Decision

A read-only public autostart catalog owns basename shadowing, condition
evaluation, and bounded desktop-entry launch planning. Settings projects
its enabled and eligible values while retaining a separate user-file write
boundary. The session supervisor owns a one-shot batch over eligible entries
after its shell and optional desktop children start. It retains direct
QProcess children, never restarts them, and terminates survivors at session
teardown. Shell recovery does not rescan.

The catalog receives explicit roots and environment facts. Production
composition resolves XDG values, while private tests inject disposable roots.
The --no-autostart option disables the production scan in diagnostic sessions.
The public launcher parser expands Exec without a shell. Unsupported activation
forms remain visible with a reason and never silently count as runnable.
Re-enable patches every recognized disable flag in the user's copy.

## Consequences

- Settings-created commands and user/system XDG entries share one next-login
  execution owner. A03's OBS preference writes an ordinary desktop entry
  for this mechanism.
- Eligible entries start once per supervised login across shell crashes.
  Only direct children are owned; forked descendants and D-Bus-activated
  applications require separate lifecycle design.
- Unsupported D-Bus activation and early startup phases are diagnosed in
  Settings. Any future support must extend the catalog and supervisor together.

## 2026-09-28 update: a documented polkit-agent basename exception

[ADR-0290](0290-native-polkit-agent-and-single-agent-rule.md) adds one
documented exception to this ADR's eligibility rules: an `Exec`/`TryExec`
basename matching a fixed table of distribution polkit authentication
agents (`polkit-gnome-authentication-agent-1`,
`polkit-kde-authentication-agent-1`, `lxqt-policykit-agent`,
`polkit-mate-authentication-agent-1`, `xfce-polkit`, `lxpolkit`) is
ineligible -- superseded by the session's own `qindaqt-polkit-agent` --
independent of that entry's `NotShowIn`/`OnlyShowIn`, `Hidden`, or any
other condition this ADR's catalog otherwise evaluates. Every entry
outside that table keeps exactly the eligibility this ADR describes; the
exception exists because a real polkit-gnome entry's own
`NotShowIn=MATE;KDE` never named QindaQt, so this catalog's ordinary
conditions could never have excluded it on their own.
