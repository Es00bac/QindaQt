// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// Live customization of one panel surface (O9): the Meta+right-click chord,
// the panel menu, the edit-mode bar, and the drop-target arithmetic
// edit-mode drags resolve against. Owned by PanelContent, which keeps the
// zone budget and the surface material; everything here is a facade over the
// LiveCustomizationController the shell runtime injects.
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
    // The stretch the edit bar takes at the trailing end of the material in
    // edit mode; PanelContent subtracts it from the zones' extent so the bar
    // never covers an applet (ADR-0266).
    readonly property real editBarReserve: editBar.visible
        ? (horizontal ? editBar.width : editBar.height) + panelContent.contentInset : 0

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

    // A drop target for a point in this surface's coordinates, or null
    // outside it: the nearest zone along the main axis, with the applet the
    // drop lands before. Zone boundaries sit halfway between neighbouring
    // zones so an empty (zero-extent) zone stays reachable; a dock keeps the
    // zone rectangles, so its whole shelf and margins stay the centre zone.
    function dropTargetAt(x, y) {
        if (x < 0 || y < 0 || x > width || y > height) {
            return null
        }
        const along = horizontal ? x : y
        const lead = zone => horizontal ? zone.x : zone.y
        const trail = zone => lead(zone) + (horizontal ? zone.width : zone.height)
        const dock = panelContent.dockMode
        const toCenter = dock ? trail(startZone) : (trail(startZone) + lead(centerZone)) / 2
        const toEnd = dock ? lead(endZone) : (trail(centerZone) + lead(endZone)) / 2
        const zoneItem = along < toCenter ? startZone : along >= toEnd ? endZone : centerZone
        const local = zoneItem.mapFromItem(panelContent, x, y)
        return {panelId: panelId, zone: zoneItem.zone,
                beforeAppletId: zoneItem.beforeAppletAt(horizontal ? local.x : local.y)}
    }

    // This surface's solved geometry in global logical coordinates, or null
    // where it is unknown (no output id: offscreen hosts and the preview).
    function surfaceGeometry() {
        if (!available || outputId === "") {
            return null
        }
        const own = controller.panelSurface(outputId, panelId)
        return own !== null && own !== undefined && own.panelId !== undefined ? own : null
    }

    // An edit-mode drag that started on this surface moved to (x, y) here
    // (ADR-0266). With a known geometry the point goes to the controller in
    // global coordinates and the surface under it resolves the target
    // (onDragPointChanged below), so a drag crosses panels and displays;
    // without one only this surface can resolve, and anywhere else is off
    // target.
    function trackDrag(x, y) {
        if (!available || controller.dragActive !== true) {
            return
        }
        const own = surfaceGeometry()
        if (own !== null) {
            controller.trackDragPoint(own.x + x, own.y + y)
            return
        }
        const target = dropTargetAt(x, y)
        if (target !== null) {
            controller.hoverDropTarget(target.panelId, target.zone, target.beforeAppletId)
        } else {
            controller.hoverDropTarget("", "", "")
        }
    }

    Connections {
        target: root.available ? root.controller : null
        function onDragPointChanged() {
            const own = root.surfaceGeometry()
            if (own === null || root.controller.dragActive !== true) {
                return
            }
            const point = root.controller.dragPoint
            const target = root.dropTargetAt(point.x - own.x, point.y - own.y)
            if (target !== null) {
                root.controller.hoverDropTarget(target.panelId, target.zone,
                                                target.beforeAppletId)
            }
        }
    }

    PanelCustomizeMenu {
        id: panelMenu
        panel: root.panelContent.panel
        controller: root.controller
        outputId: root.outputId
    }

    // Edit mode: Add applet… / Undo / Done at the trailing end of the
    // panel's material, in the stretch the zones leave free for it. A dock
    // widens its centred shelf by the same reserve, so the bar sits on the
    // painted shelf (inside the input mask), never out in its clear margin.
    PanelEditBar {
        id: editBar
        readonly property rect material: root.panelContent.materialBounds
        controller: root.controller
        panelId: root.panelId
        vertical: !root.horizontal
        buttonHeight: root.horizontal
            ? Math.max(18, Math.min(32, material.height - root.panelContent.crossAxisInset * 2))
            : 24
        buttonWidth: root.horizontal ? -1 : Math.max(18, root.width - root.panelContent.crossAxisInset * 2)
        x: root.horizontal
            ? material.x + material.width - width - root.panelContent.contentInset
            : root.panelContent.crossAxisInset
        y: root.horizontal
            ? material.y + (material.height - height) / 2
            : material.y + material.height - height - root.panelContent.contentInset
    }
}
