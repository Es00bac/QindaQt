// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T

// Chrome-free popup that places itself against the control that owns it, on
// whichever output edge its host panel occupies. This is the one owner of
// panel-popup placement: a panel applet opens its surface flush with the
// leading edge of its control and directly below it, directly above it when
// the panel sits on the bottom edge, and beside it on a side panel, then
// slides along the panel axis to stay on the output.
//
// Carries no visuals on purpose. Consumers supply `background` and
// `contentItem`; QindaQt.Controls.ControlPopupFrame is the tokenized dressing
// of this type, and the Bliss start panel and the per-service applets supply
// their own.
//
// AGENT-GUARD: keep popupType Window. A layer-shell panel surface is 26-46 px
// tall, rejects keyboard focus, and cannot paint outside itself, so every
// outward-facing panel surface must be its own window and seed focus itself.
// An item popup would be clipped to the panel band and never focusable.
//
// AGENT-GUARD: QtWayland ignores a Popup.Window's x/y once the popup has a
// parent item. QQuickPopupWindow hands that item's scene rectangle to the
// xdg_positioner anchored at its top-right corner with bottom-right gravity
// (Qt 6.11 qquickpopupwindow.cpp, qwaylandxdgshell.cpp), so a popup parented
// to its control opens at the control's *right* edge -- the "start menu in
// the upper-right corner of the start button" defect. The popup is therefore
// parented to a hidden 1x1 positioner cell whose top-right corner is the
// placement origin; x/y keep every other platform on that same origin, and
// the cell's containment mask keeps CloseOnPressOutsideParent measured
// against the control rather than against the cell.
//
// AGENT-CONTRACT: docs/wiki/shell/panel-popup-placement.md is the source of
// truth for the vectors below. QindaQt.Shell.GlobalMenu's GlobalMenuNativeMenu
// reimplements the same math because a QtQuick.Controls Menu owns its own
// top-level window and submenu chain and cannot inherit this type; the two
// must stay in step, and both are pinned by tests.
T.Popup {
    id: popup

    // The control that owns the popup. Defaults to the item the popup is
    // declared in; placement, the positioner cell, and the outside-press area
    // are all measured against it.
    property Item anchorItem: null
    // Explicit override ("top", "bottom", "left", "right"); empty detects the
    // hosting panel's edge.
    property string panelEdge: ""
    // Set by applets on a vertical panel so edge detection without a panel
    // model (desktop surface, previews, tests) falls back sideways rather
    // than across the bar.
    property bool vertical: false
    // Bumped on every open: anchor geometry is read through mapToItem, which
    // notifies nothing when the panel lays its controls out again.
    property int placementRevision: 0

    readonly property string resolvedPanelEdge: placementRevision >= 0 ? detectPanelEdge() : ""
    readonly property point placement: placementRevision >= 0 && anchorItem !== null
        ? placementFor(anchorItem.mapToItem(null, 0, 0), anchorItem.width, anchorItem.height,
                       width, height, resolvedPanelEdge, outputWidth(), outputHeight())
        : Qt.point(0, 0)
    // The positioner cell's top-left in anchorItem coordinates. The clamp
    // reads non-notifying scene geometry, so it also follows each open.
    readonly property point positionerCell: placementRevision >= 0 && anchorItem !== null
        ? anchorItem.mapFromItem(null, positionerCellFor(
              anchorItem.mapToItem(null, placement.x, placement.y),
              anchorItem.Window.width, anchorItem.Window.height))
        : Qt.point(placement.x - 1, placement.y)

    readonly property Item positionerAnchor: Item {
        objectName: "panelPopupPositionerAnchor"
        parent: popup.anchorItem
        visible: false
        width: 1
        height: 1
        x: popup.positionerCell.x
        y: popup.positionerCell.y
        containmentMask: QtObject {
            function contains(point: point): bool {
                const owner = popup.anchorItem
                return owner !== null
                    && owner.contains(popup.positionerAnchor.mapToItem(owner, point.x, point.y))
            }
        }
    }

    // Pure placement: the popup origin relative to the anchor's top-left.
    // AGENT-GUARD: `anchorPosition` is window-local on purpose. Wayland never
    // tells a layer-shell client where its window sits, so a global frame
    // would invent positions; clamping therefore happens only along the panel
    // axis, where an unknown window offset can delay a clamp but never cause a
    // wrong one, and the compositor's popup positioner stays the backstop.
    function placementFor(anchorPosition, anchorWidth, anchorHeight, popupWidth, popupHeight,
                          edge, boundsWidth, boundsHeight) {
        const sideways = edge === "left" || edge === "right"
        let px = edge === "left" ? anchorWidth : (edge === "right" ? -popupWidth : 0)
        let py = sideways ? 0 : (edge === "bottom" ? -popupHeight : anchorHeight)
        if (sideways)
            py += slideOffset(anchorPosition.y + py, popupHeight, boundsHeight)
        else
            px += slideOffset(anchorPosition.x + px, popupWidth, boundsWidth)
        return Qt.point(px, py)
    }

    // Shift keeping [start, start + extent] inside [0, limit], preferring the
    // start edge when the popup is larger than the output.
    function slideOffset(start, extent, limit) {
        if (!(limit > 0))
            return 0
        return Math.max(0, Math.min(start, limit - extent)) - start
    }

    // Window-local top-left of the 1x1 positioner cell for a window-local
    // popup origin. An xdg_positioner anchor rectangle may not leave its
    // parent surface, so an origin outside the host window (above a bottom
    // panel, left of a right panel) is clamped in and the compositor's
    // placement-area slide completes the placement.
    function positionerCellFor(origin, surfaceWidth, surfaceHeight) {
        const inside = (value, limit) => limit > 0 ? Math.max(0, Math.min(value, limit - 1)) : value
        return Qt.point(inside(Math.floor(origin.x) - 1, surfaceWidth),
                        inside(Math.floor(origin.y), surfaceHeight))
    }

    function detectPanelEdge() {
        if (panelEdge.length > 0)
            return panelEdge
        if (anchorItem === null)
            return vertical ? "left" : "top"
        // RuntimePanel exposes its resolved panel map; the edge is exact there.
        const hostWindow = anchorItem.Window.window
        const hostPanel = hostWindow !== null ? hostWindow.panel : undefined
        const edge = hostPanel ? String(hostPanel.edge ?? "") : ""
        if (edge === "top" || edge === "bottom" || edge === "left" || edge === "right")
            return edge
        // Hosts without a panel model (desktop surface, previews, tests): open
        // away from the nearer output edge along the popup's own axis.
        const center = anchorItem.mapToItem(null, anchorItem.width / 2, anchorItem.height / 2)
        if (vertical)
            return outputWidth() > 0 && center.x > outputWidth() / 2 ? "right" : "left"
        return outputHeight() > 0 && center.y > outputHeight() / 2 ? "bottom" : "top"
    }

    function outputWidth() {
        return anchorItem !== null ? anchorItem.Screen.width : 0
    }

    function outputHeight() {
        return anchorItem !== null ? anchorItem.Screen.height : 0
    }

    popupType: T.Popup.Window
    modal: false
    focus: true
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside
    x: placement.x - positionerCell.x
    y: placement.y - positionerCell.y

    // The declared parent becomes the owner; the popup itself then hangs off
    // the positioner cell (see positionerAnchor).
    Component.onCompleted: {
        if (anchorItem === null)
            anchorItem = parent
        if (anchorItem !== null)
            parent = positionerAnchor
    }

    onAboutToShow: ++placementRevision
}
