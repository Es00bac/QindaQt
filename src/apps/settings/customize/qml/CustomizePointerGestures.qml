// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick

// AGENT-GUARD: The page owns pointer capture. Preview projection replaces
// canvas delegates, so capturing on the dragged chip loses the gesture midway.
MouseArea {
    id: root
    required property Item page
    required property var customizeSettings
    property var payload: null
    property point pressPoint
    property bool started: false
    property bool acceptedTarget: false
    property string lastTarget: ""
    preventStealing: true
    acceptedButtons: Qt.LeftButton

    function hit(item, point, propertyName) {
        if (item === root || !item.visible || !item.enabled)
            return null
        const local = item.mapFromItem(root, point.x, point.y)
        const inside = item.contains(local)
        if (!inside && item.clip)
            return null
        const children = item.children
        for (let i = children.length - 1; i >= 0; --i) {
            const found = hit(children[i], point, propertyName)
            if (found)
                return found
        }
        return inside && item[propertyName] !== undefined ? item : null
    }

    function updateTarget(point) {
        const target = hit(page, point, "targetPanelId")
        acceptedTarget = false
        if (!target) {
            lastTarget = ""
            return
        }
        // Copy identity before hover; the model may destroy target immediately.
        const panelId = target.targetPanelId
        const zone = target.targetZone
        const key = panelId + "/" + zone
        if (key !== lastTarget) {
            lastTarget = key
            customizeSettings.hoverDropTarget(panelId, zone, "")
        }
        acceptedTarget = customizeSettings.dropAccepted
    }

    onWheel: wheel => { wheel.accepted = false }

    onPressed: mouse => {
        const source = hit(page, Qt.point(mouse.x, mouse.y), "dragPluginId")
        if (!source || !customizeSettings.canEdit) {
            mouse.accepted = false
            return
        }
        payload = { pluginId: source.dragPluginId, panelId: source.dragPanelId,
                    appletId: source.dragAppletId }
        pressPoint = Qt.point(mouse.x, mouse.y)
        started = false
        acceptedTarget = false
        lastTarget = ""
        page.forceActiveFocus()
    }
    onPositionChanged: mouse => {
        if (!pressed || !payload)
            return
        if (!started) {
            const dx = mouse.x - pressPoint.x
            const dy = mouse.y - pressPoint.y
            if (Math.sqrt(dx * dx + dy * dy) < Qt.styleHints.startDragDistance)
                return
            started = true
            if (payload.appletId)
                customizeSettings.startAppletDrag(payload.panelId, payload.appletId)
            else
                customizeSettings.startPaletteDrag(payload.pluginId)
        }
        if (customizeSettings.visualDragActive)
            updateTarget(Qt.point(mouse.x, mouse.y))
    }
    onReleased: mouse => {
        if (started) {
            if (customizeSettings.visualDragActive) {
                updateTarget(Qt.point(mouse.x, mouse.y))
                if (acceptedTarget)
                    customizeSettings.commitDrag()
                else
                    customizeSettings.cancelDrag()
            }
        } else if (payload) {
            if (payload.appletId) {
                customizeSettings.selectApplet(payload.panelId, payload.appletId)
            } else {
                const panels = customizeSettings.panels
                if (panels.length)
                    customizeSettings.keyboardInsert(payload.pluginId, panels[0].id, "start", "")
            }
        }
        payload = null
    }
    onCanceled: {
        if (started && customizeSettings && customizeSettings.visualDragActive)
            customizeSettings.cancelDrag()
        payload = null
    }
    Component.onDestruction: {
        if (started && customizeSettings && customizeSettings.visualDragActive)
            customizeSettings.cancelDrag()
    }
}
