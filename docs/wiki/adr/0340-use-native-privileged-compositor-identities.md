# ADR-0340: Use native privileged compositor identities

- **Status:** Proposed — coordinated source candidate; package and native gates pending
- **Date:** 2026-10-02
- **Owners:** Compositor, Platform, Program Manager
- **Amends:** [ADR-0291](0291-run-on-qindaqt-kwin.md)

## Context

PF21 ends the temporary KDE interface and desktop-file-key admission used while
QindaQt replaced the KDE portal. Keeping the aliases after the replacement would
allow stock clients into privileged QindaQt interfaces and leave Gabbee dependent
on the legacy scripting namespace.

## Decision

The fork exports Screenshot, EIS, TabletModeManager, VirtualKeyboard and Scripting
only under `org.qindaqt.KWin*` and `/org/qindaqt/KWin`. It owns no `org.kde.KWin`
alias. Only `X-QindaQt-KWin-Wayland-Interfaces` and
`X-QindaQt-KWin-DBus-Restricted-Interfaces` grant privileged interfaces. EIS admits
only the same-user owner of the native portal backend.

These native desktop-entry fields retain the XDG string-list format. Public
lookups and private lock/capture launch checks decode the selected KService
entry with KConfig's `readXdgListEntry`; custom-key `QStringList` conversion
cannot supply that contract. Missing, empty and legacy KDE-only fields grant
no native interface, and list decoding never replaces launcher trust checks.

`CompositorNames` records the screenshot and EIS identities for all desktop
callers. The screenshot application and protected capture helper declare the
native restricted interface in their desktop entries. Gabbee selects the native
scripting service when available and retains its independent stock-desktop
fallback; it pins one endpoint for load, run and unload.

Wayland protocol identifiers and standard `org.freedesktop.*` portal interfaces
retain their wire names. Source XML filenames may retain upstream names because
the installed copies already use the native namespace.

## Consequences and qualification

Deploy the fork, desktop callers, native portal routing and Gabbee together.
This source candidate does not authorize switching installed routing before the
real frontend, input/clipboard, capture, native shortcut and power gates pass.
Retain the current open user session until the user saves and starts a fresh login.

Qualification must include the idempotent fork rename check, focused caller and
Gabbee tests, the staged installation collision check, and a coherent native
session proving the native endpoints and rejection of legacy privilege keys.
The source and installed states remain distinct in the task list and handoff.
