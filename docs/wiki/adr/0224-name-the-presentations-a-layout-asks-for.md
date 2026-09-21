# ADR-0224: Name the presentations a layout asks for

- **Status:** Accepted
- **Date:** 2026-09-20
- **Owners:** Shell presentation
- **Amends:** [ADR-0223](0223-one-stock-profile-per-distinct-feel.md)

## Context

Three stock profiles named plugins that had no manifest:
`application-launcher` and `grouped-task-list` in `unity-inspired`, and
`centered-task-list` in the centred Windows preset. Those instances resolved
to nothing, which is why the Unity rail showed a duplicated task list and the
centred taskbar showed none. ADR-0223 removed the dangling names and retired
the centred Windows preset with them.

That was the wrong half of the fix. The names were not mistakes — they were
requests for presentations the implementations do not currently offer. A Unity
rail wants a large square launcher tile and one square tile per application, a
centred Windows taskbar wants glyph-only tiles with an underline indicator, and
its start menu is a different Windows start menu from the worn XP panel that
Bliss reproduces. The operator's direction was to create what was missing and
to give the start-menu applet a choice of Windows start menu.

## Decision

Every presentation a stock layout asks for has a name, and asking for one
never means a second implementation.

**Three manifests over existing implementations.** `application-launcher`,
`grouped-task-list` and `centered-task-list` are real applets with their own
names, descriptions, zones, sizing and default settings, whose `entryPoint`
is the launcher's or the task list's. This is the aliasing precedent
`status-tray.json` already set for `system-tray` over
`qindaqt.applets.status-notifier`. The capability policy needs no new rule:
`auditedBuiltin` grants by default and neither implementation carries a
builtin deny.

**`task-list` gains `presentation`.** `standard` is a panel row with a title;
`luna` is the Bliss dressing; `centered` is the Windows-11 glyph-only tile with
an underline that widens while the task is focused; `rail` is the Unity square
tile that marks its leading edge. `dockMode` always wins, because a dock tile
is already glyph-only and owns its own bottom-anchored magnification envelope
that a flat taskbar tile must not inherit — so the glyph-only presentations get
their own centred icon rather than reusing the dock icon.

**`start-menu` gains `variant`.** `luna` is the default and the reference: the
worn XP two-column panel behind the green start pill, unchanged, so Bliss and
every existing user profile render exactly as before. `modern` is the Windows
11 centred card — search field, five-column pinned grid, the complete program
list behind an **All apps** toggle, session footer — behind a glyph-only square
button. An unrecognised value renders Luna, because a typo in a profile must
not produce an empty panel.

**The centred Windows preset returns** as `windows-modern`: a 48-pixel bottom
bar with the modern start panel and `centered-task-list`, widgets at the left
and the status area at the right. `windows-classic` and `mate-inspired` stay
retired; a `classic` variant is the obvious third if the 95/98 taskbar is
wanted back, and the enum is written so adding it is additive.

## Consequences

The stock set is nine profiles. The launcher and task-list invariants now count
the alias names as the one menu slot and the one hosted task list, so "exactly
one" still holds everywhere. The applet catalogue grows from 28 manifests to
31, and both count guards moved together.

Exactly one start panel is built per instance, through a `Loader` on a binding
that resolves once: a `Popup` creates its content item whether or not it ever
opens, so declaring both would make every Luna taskbar pay for the modern
panel's search field, pinned grid and program list. The loader is
unconditionally active, because the fail-closed rule for a missing facade is
"the button renders and refuses to open", not "the panel is absent" — an absent
panel is indistinguishable from a broken one.

The modern panel is tokenized while the Luna panel keeps its hex dressing
constants. Windows 11's start menu has no fixed period palette to reproduce —
it follows the system accent and light/dark mode — so QST-1 is the faithful
choice here as well as the maintainable one. The modern panel reuses
`StartMenuLeftColumn` for its All-apps list, so there is one searchable, lazily
rendered program list and one keyboard contract behind both variants.

`presentation` had to be added to the `TaskListApplet` QML test stub as well as
the real module; the global-menu panel rows copy that stub directory at
configure time, so a stale build tree reports the new property as
non-existent until CMake re-runs.
