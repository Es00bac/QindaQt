# ADR-0221: One owner for panel popup placement

- **Status:** Accepted
- **Date:** 2026-09-20
- **Owners:** Shell presentation

## Context

Every panel applet opens its surface as a `Popup.Window`, because a layer-shell
panel band rejects keyboard focus and cannot paint outside itself. On Wayland,
QtWayland ignores such a popup's `x`/`y` once it has a parent item and anchors
its `xdg_positioner` at that item's **top-right** corner with bottom-right
gravity. A popup parented directly to its control therefore opens at the
control's right edge, regardless of which output edge the panel occupies.

`ControlPopupFrame` solved this for the desktop controls in 2026: edge-aware
placement plus a hidden 1x1 positioner cell whose top-right corner is the
placement origin. The solution was never shared. Nine further popups — the
Bliss start panel, the launcher browser, and the audio, power, Bluetooth,
clipboard, OBS, smart-lights and tray popups — kept opening with no placement
at all. The operator reported the visible symptom: the start menu appearing in
the upper-right corner of the start button instead of above it on a bottom
taskbar. `GlobalMenuNativeMenu` had already copied the math, so the repository
carried one correct implementation, one copy, and ten omissions.

## Decision

`QindaQt.Controls.PanelPopup` is the one owner of panel-popup placement.

- It is chrome-free: it owns `popupType`, the placement vectors, the panel-edge
  resolution, the positioner cell and its containment mask, and nothing else.
  Consumers supply `background` and `contentItem`.
- `ControlPopupFrame` becomes its tokenized dressing and keeps its heading,
  rows and feedback API unchanged, so its nine consumers are untouched.
- The start panel and the per-service applet popups derive from it directly and
  pass their anchor control and panel axis.
- It lives in `QindaQt.Controls` rather than a new module because every applet
  QML module already imports and installs that module, and because the type
  depends on nothing beyond QtQuick: the panel model is read by duck typing and
  is optional.
- `GlobalMenuNativeMenu` keeps its own copy of the pure math. A
  `QtQuick.Controls` `Menu` owns its top-level window and Qt-driven submenu
  chain and cannot inherit a `Popup` subclass. The duplication is deliberate,
  declared on both sides, and pinned by tests on both sides.

## Consequences

A popup that opens in the wrong place is now a defect in one file. The
desktop-controls boundary gate accepts either the literal `popupType` or the
delegating spelling and separately asserts that `PanelPopup` still pins the
window guarantee, the placement functions, and the positioner reparent, so
delegation cannot become a loophole. `qindaqt.start-menu-qml` gains the
regression row for the reported defect.

The public Controls surface grows from 20 to 21 types, so both count guards
(`check_control_source_policy.cmake`,
`run_installed_controls_consumer.cmake`) were bumped together. `PanelPopup`
stays inside the Controls import allowlist and carries no palette literal.

Placement stays window-local: a Wayland layer-shell client never learns where
its window sits, so clamping happens only along the panel axis and the
compositor's positioner remains the backstop. The contract is recorded in
[Panel popup placement](../shell/panel-popup-placement.md).
