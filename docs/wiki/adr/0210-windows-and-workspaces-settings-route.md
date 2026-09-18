# ADR-0210: Add the Windows & workspaces Settings route over the live keys only

- **Status:** Proposed
- **Date:** 2026-09-18
- **Owners:** Settings, Session
- **Supersedes:** None
- **Superseded by:** None

## Context

The plan's Windows & workspaces route lists focus policy, raise-on-hover
delay, title-bar double-click and wheel actions, the docking modifier, the
customization chord, workspace count and names, and the switching animation.
Of those, only the five `windowManagement.*` keys exist in schema v2, and
until [ADR-0209](0209-bridge-window-management-settings-into-kwinrc.md) none of
them changed anything. The customization chord key lives in the O9 candidate
and is not on main; the remaining controls have neither a key nor a consumer.

A Settings control that writes a key nothing reads is a lie the route would
tell on every visit. The Accessibility route already set the rule for that
case (`accessibility.screenReader`, ADR-0128): a key without a consumer is
reserved, not scoped, and the boundary scan rejects any route source naming
it.

## Decision

1. **The route ships over the four live keys** (`focusPolicy`,
   `dockingModifier`, `snapDistance`, `closeContainerPolicy`), each carried
   into kwinrc and re-read by KWin or the compositor plugin on reconfigure.
   `windowManagement.sessionRestore` stays reserved until a consumer exists.
2. **The route pattern is unchanged.** A thirteenth `SettingsRouteComponent`
   appended last (index 12) so every existing index, shortcut, and traversal
   order stays stable; a route-local `WindowsRouteComposition` singleton like
   Clipboard; a multi-key draft/Apply model identical in rules to
   Accessibility (per-key commits from fresh snapshots, conflict stop,
   uncertain no-replay, replacement abort); the same boundary, poison,
   installed-route, and Settings Center proofs.
3. **Choice labels are presentation.** The model publishes `{token, label}`
   lists in schema order; the page never shows a token and only an explicit
   activation writes the draft.
4. **The rest of the plan's list is a gap, not a control.** Raise-on-hover
   delay, title-bar double-click and wheel actions, workspace count and names,
   and the switching animation each need a schema key plus a consumer (most
   are kwinrc entries the ADR-0199 bridge could carry) before they can be
   offered honestly; the customization chord control lands when O9's
   `shell.customization.chord` reaches main.

## Consequences

- Users get a Windows & workspaces page whose every control takes effect in
  the running session, with no "restart required" state to explain.
- Adding the deferred controls later is additive: a key in schema v2, a
  bridge mapping, a consumer, then a row on this page.
- Proof: `qindaqt.settings-windows-model`, `qindaqt.settings-windows-page`,
  `qindaqt.settings-windows-boundary`, `qindaqt.settings-windows-boundary-poison`,
  `qindaqt.settings-windows-installed-route`, and the Settings Center suites
  (`qindaqt.settings-route-registry`, navigation controller, route
  construction, installed routes) updated for thirteen routes.
