# Panel popup placement

Every surface a panel applet opens — the Bliss start panel, the launcher
browser, the audio, power, Bluetooth, clipboard, OBS, smart-lights and tray
popups, and the desktop controls — is placed by one owner:
`QindaQt.Controls.PanelPopup`. It carries no visuals. Consumers supply
`background` and `contentItem`;
[`ControlPopupFrame`](desktop-controls.md#popup-placement) is its tokenized
dressing and the start panel and per-service applets supply their own.

## Placement contract

A popup opens flush with the **leading edge** of the control that owns it and

- directly **below** it on a `top` panel,
- directly **above** it on a `bottom` panel, and
- **beside** it on a `left` or `right` panel (to the control's outer side),

then slides along the panel axis to stay on the output. A popup larger than
the output keeps its start edge on the output rather than centring overflow.

The edge comes from the hosting `RuntimePanel` model (`window.panel.edge`),
which is exact. A host without a panel model — the desktop surface, previews,
tests — opens away from the nearer output edge along the popup's own axis:
across the bar normally, sideways when the applet sets `vertical`. An explicit
`panelEdge` ("top", "bottom", "left", "right") overrides both.

`anchorItem` is the control that owns the popup; it defaults to the item the
popup is declared in, and it is also the area `CloseOnPressOutsideParent`
measures against. `placementRevision` is bumped on every open because anchor
geometry is read through `mapToItem`, which notifies nothing when the panel
lays its controls out again.

The anchor position is **window-local on purpose**. A Wayland layer-shell
client never learns where its window sits, so a global frame would invent
positions. Clamping therefore happens only along the panel axis, where an
unknown window offset can delay a clamp but never cause a wrong one, and the
compositor's popup positioner remains the backstop.

## Why a 1x1 positioner cell

`popupType` is `Popup.Window` and must stay that way: a layer-shell panel
surface is 26-46 px tall, rejects keyboard focus, and cannot paint outside its
own band, so an item popup would be clipped to the panel and never focusable.

On Wayland, QtWayland then **ignores the popup's `x` and `y`** once the popup
has a parent item. `QQuickPopupWindow` hands that item's scene rectangle to
the `xdg_positioner` anchored at its **top-right** corner with bottom-right
gravity (Qt 6.11 `qquickpopupwindow.cpp`, `qwaylandxdgshell.cpp`). A popup
parented directly to its control therefore opens at the control's *right*
edge — which is exactly how the start panel used to appear in the upper-right
corner of the start button on a bottom taskbar.

`PanelPopup` keeps the declared control as `anchorItem` but parents the popup
to a hidden 1x1 cell whose top-right corner is the placement origin. `x`/`y`
keep every other platform on that same origin, and the cell's containment mask
forwards hit-testing to the control so `CloseOnPressOutsideParent` still
measures the control rather than the cell.

An `xdg_positioner` anchor rectangle may not leave its parent surface, so an
origin outside the host window — above a bottom panel, left of a right panel —
is clamped into it and the compositor's placement-area slide completes the
placement. KWin's placement area excludes a panel's strut, so a top-bar popup
opens at the control's left edge and at the lower of the control's bottom and
the bar's bottom edge.

## Second implementation

`QindaQt.Shell.GlobalMenu`'s `GlobalMenuNativeMenu` reimplements the same
`placementFor`/`slideOffset` math because a `QtQuick.Controls` `Menu` owns its
own top-level window and Qt-driven submenu chain and cannot inherit
`PanelPopup`. The two must stay in step. Both are pinned by tests: see
[Tests](#tests).

## Tests

| Row | Pins |
| --- | --- |
| `qindaqt.desktop-controls-boundary` | `PanelPopup` keeps `popupType: T.Popup.Window`, the placement functions, and the positioner-cell reparent; every popup in the desktop-controls module is a window |
| `qindaqt.desktop-controls-qml-menus` | the pure placement vectors for all four edges, axis slide, oversized popup, and unknown bounds; the live positioner cell and origin for a control with no panel model |
| `qindaqt.start-menu-qml` | the start panel's anchor is the applet cell, its vectors on all four edges, and the 1x1 cell whose right edge is the placement origin |
| `qindaqt.global-menu-applet-qml-*` | the same vectors for the `Menu` implementation on every panel edge |
| `qindaqt.controls-source-policy` | `PanelPopup` stays inside the Controls import allowlist and carries no palette literal |

## Related

- [Desktop controls](desktop-controls.md)
- [Production panel surfaces](panel-surfaces.md)
- [QindaQt.Controls 1.0](controls.md)
- [Global application menu](global-menu.md)
- [ADR-0221](../adr/0221-one-owner-for-panel-popup-placement.md)
