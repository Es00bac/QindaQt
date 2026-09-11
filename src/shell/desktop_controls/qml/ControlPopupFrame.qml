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
    // The control that owns the popup; defaults to its visual parent.
    property Item anchorItem: parent
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
    x: placement.x
    y: placement.y

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
