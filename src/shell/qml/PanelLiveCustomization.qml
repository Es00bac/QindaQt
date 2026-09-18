// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// Live customization of one panel surface (O9): the Meta+right-click chord,
// the panel menu, the edit-mode Done/Undo bar, and the drop-target
// arithmetic edit-mode drags resolve against. Owned by PanelContent, which
// keeps the zone budget and the surface material; everything here is a
// facade over the LiveCustomizationController the shell runtime injects.
Item {
    id: root

    required property var panelContent
    // LiveCustomizationController (may be null: everything is inert).
    property var controller: null
    property string outputId: ""
    required property var startZone
    required property var centerZone
    required property var endZone
    readonly property bool available: controller !== null && controller.available === true
    readonly property bool editMode: available && controller.editMode === true
    readonly property bool horizontal: panelContent.horizontal
    readonly property string panelId: String(panelContent.panel.id ?? "")

    anchors.fill: parent

    // The configured chord (one Settings1 key) is held when every one of its
    // modifier bits is down.
    function chordHeld(modifiers) {
        if (!available) {
            return false
        }
        const chord = Number(controller.chordModifiers)
        return chord !== 0 && (modifiers & chord) === chord
    }

    // AGENT-GUARD: the menu opens at an explicit point along the panel's
    // far edge, never at the pointer: a popup window that starts under the
    // pointer pre-hovers its first entry and shifts every keyboard position.
    function openPanelMenu(pointerX, pointerY) {
        if (!available) {
            return false
        }
        panelMenu.popup(horizontal ? pointerX : width, horizontal ? height : pointerY)
        return true
    }

    // A drop target for a point in the panel's coordinates: a zone of this
    // panel with the applet the drop lands before, or (outside the surface)
    // another panel by solved geometry with its zone chosen by thirds.
    function dropTargetAt(x, y) {
        if (x >= 0 && y >= 0 && x <= width && y <= height) {
            const along = horizontal ? x : y
            let zoneItem = centerZone
            if (along < (horizontal ? startZone.x + startZone.width : startZone.y + startZone.height)) {
                zoneItem = startZone
            } else if (along >= (horizontal ? endZone.x : endZone.y)) {
                zoneItem = endZone
            }
            const local = zoneItem.mapFromItem(panelContent, x, y)
            return {panelId: panelId, zone: zoneItem.zone,
                    beforeAppletId: zoneItem.beforeAppletAt(horizontal ? local.x : local.y)}
        }
        if (!available || outputId === "") {
            return null
        }
        const own = controller.panelSurface(outputId, panelId)
        if (own === null || own === undefined || own.panelId === undefined) {
            return null
        }
        const other = controller.panelSurfaceAt(outputId, own.x + x, own.y + y)
        if (other === null || other === undefined || other.panelId === undefined
                || String(other.panelId) === panelId) {
            return null
        }
        const fraction = other.horizontal
            ? (own.x + x - other.x) / Math.max(1, other.width)
            : (own.y + y - other.y) / Math.max(1, other.height)
        const zone = fraction < 1 / 3 ? "start" : fraction < 2 / 3 ? "center" : "end"
        return {panelId: String(other.panelId), zone: zone, beforeAppletId: ""}
    }

    PanelCustomizeMenu {
        id: panelMenu
        panel: root.panelContent.panel
        controller: root.controller
    }

    // Edit mode: Done / Undo bar at the panel's trailing end. It overlays the
    // end zone on purpose: edit mode is a short session and the bar must be
    // reachable on every panel without moving anything.
    PanelEditBar {
        id: editBar
        controller: root.controller
        height: Math.max(18, (root.horizontal ? root.height : 24) - root.panelContent.crossAxisInset * 2)
        x: root.horizontal ? root.width - width - root.panelContent.contentInset
                           : root.panelContent.crossAxisInset
        y: root.horizontal ? root.panelContent.crossAxisInset
                           : root.height - height - root.panelContent.contentInset
    }
}
