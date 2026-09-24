// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaQt.Tokens 1.0

// Per-chip overlay for live customization. Always present so the
// Meta+right-click chord wins over the applet's own right-click handlers
// (exclusive grab on press, non-chord presses fall through untouched). In
// edit mode (ADR-0266) a shield covers the whole chip: the applet beneath is
// inert (no clicks, popups, wheel or drags of its own), a left drag anywhere
// on it moves the applet, and a right click opens its customize menu. Drop
// targets are resolved by the panel surfaces (PanelLiveCustomization).
Item {
    id: root
    objectName: "appletEditHandle"

    required property var panel
    required property var applet
    // LiveCustomizationController (may be null: everything is inert).
    property var controller: null
    // The PanelContent hosting this chip; publishes the drag pointer.
    property var editorHost: null
    readonly property bool active: controller !== null && controller.available === true
    readonly property bool editMode: active && controller.editMode === true
    readonly property string panelId: String(panel.id ?? "")
    readonly property string appletId: String(applet.id ?? "")

    z: 100

    // The menu opens at the chip's far corner, never under the pointer (a
    // popup that starts under it pre-hovers its first entry and shifts every
    // keyboard position); the compositor flips it back on screen at edges.
    function openMenu() {
        if (!active) {
            return false
        }
        appletMenu.popup(0, root.height)
        return true
    }

    // Chord: the configured modifiers plus the right button, exclusive on
    // press so the applet beneath never also opens its own menu.
    TapHandler {
        objectName: "appletChordTap"
        enabled: root.active
        acceptedButtons: Qt.RightButton
        acceptedModifiers: root.active ? root.controller.chordModifiers : Qt.NoModifier
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: root.openMenu()
    }

    Rectangle {
        objectName: "appletEditHandleFrame"
        anchors.fill: parent
        visible: root.editMode
        color: dragHandler.active
            ? (Tokens.ready ? Tokens.state.hover : "#3355aa")
            : "transparent"
        opacity: dragHandler.active ? 0.5 : 1
        border.width: 1
        border.color: Tokens.ready ? Tokens.fg.default : "#dddddd"
        radius: 4
        Text {
            anchors.centerIn: parent
            text: "☰"
            color: Tokens.ready ? Tokens.fg.default : "#dddddd"
            font.pixelSize: Math.max(8, Math.min(14, parent.height - 6))
            Accessible.ignored: true
        }
    }

    // AGENT-CONTRACT (ADR-0266, with ADR-0265): edit mode owns every press on
    // the chip. The shield accepts all buttons, so nothing beneath -- the
    // applet's controls and menus, and the dock strip's own tile drag,
    // drag-off-to-remove and grouping (W13) -- ever sees a press while the
    // whole applet is being moved; outside edit mode the shield is disabled
    // and this file adds nothing but the chord.
    MouseArea {
        id: shield
        objectName: "appletEditShield"
        anchors.fill: parent
        enabled: root.editMode
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        cursorShape: dragHandler.active ? Qt.ClosedHandCursor : Qt.OpenHandCursor
        onClicked: (mouse) => {
            if (mouse.button === Qt.RightButton) {
                root.openMenu()
            }
        }
        onWheel: (wheel) => { wheel.accepted = true }

        // AGENT-GUARD: the DragHandler must stay a handler of the shield.
        // Handlers see a press before their own item does, so it holds a
        // passive grab while the shield takes the exclusive one, then takes
        // the drag over past the threshold. On any item beneath the shield it
        // would never see the press at all.
        DragHandler {
            id: dragHandler
            objectName: "appletEditDrag"
            acceptedButtons: Qt.LeftButton
            target: null
            onActiveChanged: {
                if (active) {
                    if (!root.controller.beginAppletDrag(root.panelId, root.appletId)) {
                        return
                    }
                    root.trackAt(centroid.position)
                } else if (root.controller.dragActive) {
                    if (root.controller.dropAccepted) {
                        root.controller.dropApplet()
                    } else {
                        root.controller.cancelDrag()
                    }
                }
            }
            onCentroidChanged: {
                if (active && root.controller.dragActive) {
                    root.trackAt(centroid.position)
                }
            }
        }
    }

    // The pointer keeps its implicit grab on this surface while the button
    // is held, even over another panel or display, so every position is
    // published from here in this surface's coordinates.
    function trackAt(localPoint) {
        if (editorHost === null || editorHost.trackDrag === undefined) {
            return
        }
        const hostPoint = shield.mapToItem(editorHost, localPoint.x, localPoint.y)
        editorHost.trackDrag(hostPoint.x, hostPoint.y)
    }

    AppletCustomizeMenu {
        id: appletMenu
        objectName: "appletCustomizeMenu:" + root.appletId
        panel: root.panel
        applet: root.applet
        controller: root.controller
    }
}
