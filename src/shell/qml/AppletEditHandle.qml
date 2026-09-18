// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaQt.Tokens 1.0

// Per-chip overlay for live customization. Always present so the
// Meta+right-click chord wins over the applet's own right-click handlers
// (exclusive grab on press, non-chord presses fall through untouched); in
// edit mode it also paints a handle and turns the chip into a drag source
// whose drop targets are resolved by the hosting PanelContent.
Item {
    id: root
    objectName: "appletEditHandle"

    required property var panel
    required property var applet
    // LiveCustomizationController (may be null: everything is inert).
    property var controller: null
    // The PanelContent hosting this chip; resolves drop targets.
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

    DragHandler {
        id: dragHandler
        objectName: "appletEditDrag"
        enabled: root.editMode
        acceptedButtons: Qt.LeftButton
        target: null
        onActiveChanged: {
            if (active) {
                if (!root.controller.beginAppletDrag(root.panelId, root.appletId)) {
                    return
                }
                root.hoverAt(centroid.position)
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
                root.hoverAt(centroid.position)
            }
        }
    }

    function hoverAt(localPoint) {
        if (editorHost === null || editorHost.dropTargetAt === undefined) {
            return
        }
        const hostPoint = mapToItem(editorHost, localPoint.x, localPoint.y)
        const target = editorHost.dropTargetAt(hostPoint.x, hostPoint.y)
        if (target !== null && target !== undefined) {
            controller.hoverDropTarget(String(target.panelId), String(target.zone),
                                       String(target.beforeAppletId ?? ""))
        }
    }

    AppletCustomizeMenu {
        id: appletMenu
        objectName: "appletCustomizeMenu:" + root.appletId
        panel: root.panel
        applet: root.applet
        controller: root.controller
    }
}
