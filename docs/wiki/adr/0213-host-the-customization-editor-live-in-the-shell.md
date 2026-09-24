# ADR-0213: host the customization editor live in the shell

- **Status:** Accepted
- **Date:** 2026-09-18
- **Owners:** Shell runtime, customization editor domain
- **Supersedes:** None
- **Superseded by:** In part, [ADR-0266](0266-edit-mode-drags-applets-across-panels-and-displays.md):
  how edit-mode drags resolve their targets (now on the panel surface under a
  global drag point), and decision 5's "Esc is not offered" (panels take
  on-demand keyboard focus while editing). In part,
  [ADR-0267](0267-settings-switches-layout-presets-and-editing-happens-on-the-panels.md):
  the Settings route is no longer a second editing surface (it switches and
  saves presets); its editor host stays only as the parity reference. The
  rest stands.

## Context

Until now the only way to change a layout was the Settings Customize route
(ADR-0043 isolates the editor domain it composes; ADR-0122 makes the shell
adopt the profile it writes). Users expect to customize the desktop where it
is: a modifier chord on a panel, an applet or the desktop, and a menu that
adds, moves, removes or configures immediately, plus an edit mode with drag
handles. Two designs were possible: a second, shell-local editing path that
talks to the transaction engine directly, or hosting the existing editor
domain (repository, engine adapter, session, user profile store) inside the
shell. A second path would have duplicated gesture, rollback, lease and
persistence policy in QML and would have let the two surfaces drift in what
they persist for the same intent.

Delivering the chord to a layer-shell panel needed two compositor facts,
verified in the KWin 6.6.6 sources: the pointer-focused surface receives
keyboard modifiers (`KeyboardInterface::setModifierFocusSurface`), and the
window-action filter runs `[MouseBindings] CommandAll3` (default `Resize`) on
every client window including layer-shell panels and consumes the press,
while `Nothing` replays it to the surface.

## Decision

1. The shell hosts the editor domain, never a second path. `LiveEditorHost`
   composes `LayoutEditingRepository` → `CoordinatorEditingEngine` →
   `EditorSession(UserProfileStore)` with the same arguments, in the same
   order, as the Settings route's host. This composition is the parity
   contract: the same intent sequence persists byte-identical profiles from
   either surface, and tests hold both hosts to it.
2. The editor intent vocabulary grows by exactly the two whole-panel intents
   the live menus need (`AddPanel`, `RemovePanel`), mapped one-to-one onto
   the engine's existing commands; no engine command or persisted field is
   added.
3. Every menu entry and every edit-mode drop is one editor gesture followed
   by one `applyToUserProfile`; the shell adopts the written profile through
   its existing layout adoption. An adoption of the profile the live session
   just committed keeps the session (so Undo spans the edit run); any other
   adoption or an output-generation change rebuilds it. The writable user
   store joins the remembered catalog directories as soon as it exists, so a
   first-ever Apply on a fresh account is adopted live too.
4. The chord is one Settings1 key with a default (`shell.customization.chord`
   = `meta-right`, alternative `meta-alt-right`), read through a
   purpose-scoped client. `qindaqt-wm` seeds `kwinrc [MouseBindings]
   CommandAll3=Nothing` (seed-missing only) so the press reaches the panel;
   no compositor-plugin change is made for the chord.
5. Menus are keyboard contracts: fixed entry order, no hidden or disabled
   rows, opened away from the pointer, and the menu owns its content view's
   key handling. Edit mode leaves through Done, Meta+Shift+E or the menu;
   layer-shell panels take no keyboard focus, so Esc is not offered for it.

## Consequences

- One editing policy, two surfaces. Bugs in gesture or persistence policy are
  fixed once, in the domain.
- The shell links `QindaQt::ShellCustomizationEditor` (static) and gains a
  controller, four panel QML components and one desktop component; no new
  process, bus interface or persisted field.
- Keyboard positions of menu entries are load-bearing for the nested rows
  and for users; changing the order changes the contract and the rows.
- The nested rows (`shell.live-customization.*`) run the production shell in
  a private virtual KWin with the fixture profile shadowing the built-in
  default through a private `XDG_DATA_DIRS` entry and the user store absent,
  replaying every observed intent through the Settings route's own host
  (`qindaqt-customize-parity-tool`) and comparing bytes.
- A fresh Settings1 key means the resident service must know the schema at
  install time (the shell falls back to the default chord when the scope is
  unavailable).
- The chord seed has a cost: KWin's default `CommandAll3=Resize` is what
  gives a fresh profile Meta+right-drag window resizing, and the seed turns
  it off (`Nothing`) so the press reaches the panel instead. A user who wants
  the resize back sets `[MouseBindings] CommandAll3` themselves; the seed is
  seed-missing and never overwrites that choice. Whether a `meta-alt-right`
  chord could avoid the seed depends on KWin's modifier matching and was not
  verified here.

## Revisit when

- The profile schema grows a zone or panel field the editor cannot express
  through the existing nine intents.
- Layer-shell panels gain on-demand keyboard focus, which would allow Esc and
  a focusable edit bar.
- KWin changes how modifiers reach pointer-focused surfaces or how window
  actions treat layer-shell windows.
