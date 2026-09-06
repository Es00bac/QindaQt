# First-party applications

First-party applications are ordinary desktop clients. They use public services
and shared design contracts rather than reaching into the shell's private state.
The shared AppShell boundary provides lifecycle, actions, menus, injected
integration, focus, and accessibility contracts; it is not a universal owner of
application documents or platform policy.

## Welcome guide

Welcome is the first-run introduction and remains available from the launcher.
Its seven short chapters explain ordinary desktop use, intentional window
arrangement, groups and pages, safe detach and movement, customization, and
appearance before offering buttons that open the real Settings, Text Editor,
and File Manager applications. The guide follows the confirmed desktop
appearance and uses the shared QST-1/Controls language.

Its **Show at next launch** choice is application-local and defaults on. Turning
it off prevents later automatic opening without removing the manual launcher
entry. See [Welcome to QindaQt](../apps/welcome.md) for the exact process and
persistence boundary.

## Settings Center

The Settings Center owns bounded route registration, wide/compact navigation,
route lifetime, keyboard navigation, and accessible presentation. Individual
routes own their drafts and service-client interactions. A route's existence
must not be confused with complete platform capability.

| Route | What it controls or presents |
| --- | --- |
| Notifications | Confirmed interruption preferences through Settings1. |
| Appearance | Validated appearance drafts, QST preview, and per-key commits. |
| Display | Output/mode/scale drafts and reversible preview/confirmation through Display1. |
| Network | Secret-free inventory and bounded connection controls; interactive credentials belong to a separate agent. |
| Customize | Direct profile canvas, keyboard-equivalent edits, user-profile persistence, and confirmed selection. |
| Audio | Devices, streams, defaults, volume, and mute through Audio1. |
| Bluetooth | Adapter/device inventory, discovery, and controls with BlueZ-owned pairing authority. |
| Power | Supply, profile/hold, and brightness state, with session actions behind separate authenticated boundaries. |
| Clipboard | Default-off history preference, content-free counts/state, and confirmed clearing. |
| Color | ICC catalog/import and per-display assignment intent; compositor color application is a separate boundary. |

See [Settings Center](../apps/settings-center.md) and the route pages in the
[documentation catalog](catalog/reading.md). Preference keys and defaults
are enumerated in the [configuration catalog](catalog/settings.md).

## Text Editor

The editor handles bounded local UTF-8 text, with up to 32 independently owned
document tabs. It preserves the opened newline/BOM policy, validates content,
and saves atomically. Each tab tracks external changes; a conflicting save
retains local text and offers explicit recovery instead of silently overwriting.
Find/replace supports bounded literal and restricted regular-expression searches,
with Replace All undoable as one operation.

Optional restore persists paths only, not unsaved contents. CLI paths override
restore on launch. Missing files and unsafe restore storage are handled through
the documented admission policy. Local menus remain authoritative when global
menu export is unavailable. See [Text Editor](../apps/text-editor.md) for exact
size limits, supported regex syntax, shortcuts, consent, and deferrals.

## File Manager

The File Manager provides local directory navigation, breadcrumbs/history,
bounded file launching, and identity-checked local mutation and recovery.
Its S1 boundary includes a documented home Trash contract. Filesystem operations
must validate the actual local target rather than treating a stale visible row
as authority. It is not evidence of a general remote filesystem, privileged
file-management, or arbitrary protocol-handler implementation. Consult
[File Manager](../apps/file-manager.md) for supported operations and explicit
S1 deferrals before assuming parity with another file manager.

## Terminal

The Terminal owns child processes and PTYs and confines qtermwidget behind a
rendering adapter. It supports up to eight sessions/tabs, profiles and Settings1
persistence, bounded per-session scrollback search, and confirmed opening of
visible links. Launch policy separates an executable and argv; rendering does
not own process lifecycle. Closing and failure recovery must preserve accurate
exit state and bounded teardown. See [Terminal](../apps/terminal.md) for commands,
search/link restrictions, focus, and keyboard behavior.

## Shared visual language and future scope

QST-1 derives semantic appearance values; Controls supplies reusable compiled
QML primitives; Widgets consumers adapt public tokens without inventing a second
palette authority. Cross-app keyboard, accessibility, DPI, and visual evidence
is tracked independently from individual feature tests.

Viewer, archive, monitor, software, and other long-range application workflows
in architecture descriptions are product scope, not proof of shipped executables.
Use the [feature catalog](catalog/features.md) and [repository catalog](catalog/repository.md)
to distinguish implementations from future scope. Return to the
[handbook index](index.md).
