// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Tokens 1.0

// Shared popup chrome for the desktop controls. It is a separate popup
// window: the layer-shell panel rejects keyboard focus and cannot paint
// outside its own surface, so every outward-facing control surface here is
// hosted in its own window and seeds focus itself (audit findings A01/A02).
// Callers add rows as children; heading and feedback are owned here.
// Placement is owned here too: the popup opens flush with its anchor's left
// edge, directly below it, or directly above it when the hosting panel sits
// on the bottom edge (beside it for side panels), sliding along the panel
// axis to stay on the output (docs/wiki/shell/desktop-controls.md).
T.Popup {
    id: popup

    property string heading: ""
    property string feedback: ""
    property Item initialFocusItem: null
    default property alias rows: body.data
    // The control that owns the popup; defaults to the item it is declared in.
    property Item anchorItem: null
    // Explicit override ("top", "bottom", "left", "right"); empty detects the
    // hosting panel's edge.
    property string panelEdge: ""
    // Bumped on every open: anchor geometry is read through mapToItem, which
    // notifies nothing when the panel lays controls out again.
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

    // AGENT-GUARD: QtWayland ignores a Popup.Window's x/y once the popup has a
    // parent item. QQuickPopupWindow hands that item's scene rectangle to the
    // xdg_positioner, anchored at its top-right corner with bottom-right
    // gravity (Qt 6.11 qquickpopupwindow.cpp, qwaylandxdgshell.cpp), so a
    // popup parented to its control opened at the control's right edge. The
    // popup is parented to this 1x1 cell instead, whose top-right corner is
    // the placement origin; x/y keep other platforms on that same origin, and
    // the mask keeps CloseOnPressOutsideParent measured against the control.
    readonly property Item positionerAnchor: Item {
        objectName: "controlPopupPositionerAnchor"
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
            return "top"
        // RuntimePanel exposes its resolved panel map; the edge is exact there.
        const hostWindow = anchorItem.Window.window
        const hostPanel = hostWindow !== null ? hostWindow.panel : undefined
        const edge = hostPanel ? String(hostPanel.edge ?? "") : ""
        if (edge === "top" || edge === "bottom" || edge === "left" || edge === "right")
            return edge
        // Hosts without a panel model (desktop surface, previews, tests): open
        // away from the nearer horizontal output edge.
        const center = anchorItem.mapToItem(null, anchorItem.width / 2, anchorItem.height / 2)
        return outputHeight() > 0 && center.y > outputHeight() / 2 ? "bottom" : "top"
    }

    function outputWidth() {
        return anchorItem !== null ? anchorItem.Screen.width : 0
    }

    function outputHeight() {
        return anchorItem !== null ? anchorItem.Screen.height : 0
    }

    // AGENT-GUARD: keep Popup.Window. An item popup would be clipped to the
    // 26-36 px panel band and could never receive keyboard focus.
    popupType: T.Popup.Window
    modal: false
    focus: true
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside
                 | T.Popup.CloseOnPressOutsideParent
    padding: Tokens.space["3"]
    width: Math.min(480, Math.max(240, contentItem.implicitWidth + leftPadding + rightPadding))
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

    onOpened: Qt.callLater(function() {
        if (popup.initialFocusItem !== null)
            popup.initialFocusItem.forceActiveFocus(Qt.PopupFocusReason)
        else
            body.forceActiveFocus(Qt.PopupFocusReason)
    })

    background: Rectangle {
        radius: Tokens.radius.l
        color: Tokens.bg.raised
        border.width: Tokens.space["1"] / 2
        border.color: Tokens.outline.divider
    }

    contentItem: ColumnLayout {
        spacing: Tokens.space["2"]

        C.Label {
            objectName: "controlPopupHeading"
            Layout.fillWidth: true
            visible: popup.heading.length > 0
            text: popup.heading
            font.pointSize: Tokens.type.title
            font.weight: Font.DemiBold
            Accessible.role: Accessible.Heading
        }

        ColumnLayout {
            id: body
            Layout.fillWidth: true
            spacing: Tokens.space["1"]
        }

        C.Label {
            objectName: "controlPopupFeedback"
            Layout.fillWidth: true
            visible: popup.feedback.length > 0
            text: popup.feedback
            color: Tokens.fg.default
            maximumLineCount: 3
            elide: Text.ElideRight
            Accessible.role: Accessible.AlertMessage
        }
    }
}
