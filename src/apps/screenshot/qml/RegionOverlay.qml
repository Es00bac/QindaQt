// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Window
import QindaTK as Tk
import QindaTK.QindaQt

// One output's share of the region picker: the frozen workspace, dimmed
// outside the selection, full screen on its own output.
//
// AGENT-CONTRACT: the selection is ONE global logical rectangle held by
// Main.qml's `regionState` and shared by every output's overlay, so a drag
// may cross outputs. Clamping, nudging and the pixel-size label come from
// captureFlow (region_geometry.cpp); nothing here computes geometry policy.
// Esc cancels, Enter captures (the whole output when nothing is selected).
Window {
    id: overlay

    required property var modelData
    required property QtObject regionState

    readonly property int originX: modelData.x
    readonly property int originY: modelData.y
    readonly property rect selection: regionState.selection
    readonly property bool hasSelection: selection.width > 0 && selection.height > 0
    readonly property rect localSelection: Qt.rect(selection.x - originX, selection.y - originY,
                                                   selection.width, selection.height)
    readonly property size pixels: hasSelection ? captureFlow.pixelSize(selection) : Qt.size(0, 0)

    // AGENT-GUARD: never transient for the (hidden) main window; on Wayland
    // a hidden parent would drag these surfaces along or refuse to map them.
    transientParent: null
    screen: Qt.application.screens[modelData.index]
    x: originX
    y: originY
    width: modelData.width
    height: modelData.height
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    visibility: Window.FullScreen
    color: "black"
    title: qsTr("Select a region to capture")

    QindaQtTheme {}

    function globalPoint(x, y) {
        return Qt.point(overlay.originX + x, overlay.originY + y)
    }

    function wholeOutput() {
        return Qt.rect(overlay.originX, overlay.originY, overlay.width, overlay.height)
    }

    Image {
        id: frozen
        anchors.fill: parent
        source: overlay.modelData.source
        cache: false
        smooth: true
        fillMode: Image.Stretch
    }

    // AGENT-NOTE: a content scrim over arbitrary screen pixels, deliberately
    // theme-independent; the selection frame and labels carry the theme.
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.45)
    }

    Item {
        visible: overlay.hasSelection
        x: overlay.localSelection.x
        y: overlay.localSelection.y
        width: overlay.localSelection.width
        height: overlay.localSelection.height
        clip: true
        Image {
            x: -parent.x
            y: -parent.y
            width: overlay.width
            height: overlay.height
            source: frozen.source
            cache: false
            smooth: true
            fillMode: Image.Stretch
        }
    }

    Rectangle {
        objectName: "selectionFrame"
        visible: overlay.hasSelection
        x: overlay.localSelection.x - border.width
        y: overlay.localSelection.y - border.width
        width: overlay.localSelection.width + 2 * border.width
        height: overlay.localSelection.height + 2 * border.width
        color: "transparent"
        border.color: Tk.Theme.color.accent
        border.width: 2
    }

    Rectangle {
        objectName: "selectionSize"
        visible: overlay.hasSelection
        color: Tk.Theme.color.panel
        border.color: Tk.Theme.color.border
        radius: Tk.Theme.radius.sm
        width: sizeLabel.implicitWidth + 2 * Tk.Theme.space.sm
        height: sizeLabel.implicitHeight + Tk.Theme.space.sm
        x: Math.min(Math.max(0, overlay.localSelection.x), overlay.width - width)
        y: overlay.localSelection.y + overlay.localSelection.height + Tk.Theme.space.sm + height < overlay.height
           ? overlay.localSelection.y + overlay.localSelection.height + Tk.Theme.space.sm
           : Math.max(0, overlay.localSelection.y - height - Tk.Theme.space.sm)
        Tk.Mono {
            id: sizeLabel
            anchors.centerIn: parent
            text: qsTr("%1 × %2").arg(overlay.pixels.width).arg(overlay.pixels.height)
        }
    }

    Rectangle {
        objectName: "regionHints"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: Tk.Theme.space.lg
        color: Tk.Theme.color.panel
        border.color: Tk.Theme.color.border
        radius: Tk.Theme.radius.sm
        width: hintColumn.implicitWidth + 2 * Tk.Theme.space.md
        height: hintColumn.implicitHeight + 2 * Tk.Theme.space.sm
        visible: !overlay.regionState.dragging

        Column {
            id: hintColumn
            anchors.centerIn: parent
            spacing: Tk.Theme.space.xs
            Row {
                spacing: Tk.Theme.space.sm
                Tk.Label { text: qsTr("Drag to select"); anchors.verticalCenter: parent.verticalCenter }
                Tk.KeyCap { sequence: "Return"; anchors.verticalCenter: parent.verticalCenter }
                Tk.Label { text: qsTr("captures"); anchors.verticalCenter: parent.verticalCenter }
                Tk.KeyCap { sequence: "Escape"; anchors.verticalCenter: parent.verticalCenter }
                Tk.Label { text: qsTr("cancels"); anchors.verticalCenter: parent.verticalCenter }
            }
            Tk.Caption {
                text: overlay.regionState.hint.length > 0 ? overlay.regionState.hint
                      : qsTr("Arrows move the selection, Shift moves faster, Alt resizes")
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.CrossCursor
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onPressed: mouse => {
            if (mouse.button === Qt.RightButton) {
                captureFlow.cancel()
                return
            }
            overlay.regionState.anchor = overlay.globalPoint(mouse.x, mouse.y)
            overlay.regionState.dragging = true
            overlay.regionState.selection = Qt.rect(0, 0, 0, 0)
        }
        onPositionChanged: mouse => {
            if (overlay.regionState.dragging)
                overlay.regionState.selection = captureFlow.selectionFromPoints(
                    overlay.regionState.anchor, overlay.globalPoint(mouse.x, mouse.y))
        }
        onReleased: overlay.regionState.dragging = false
        onDoubleClicked: captureFlow.confirmRegion(overlay.hasSelection ? overlay.selection
                                                                         : overlay.wholeOutput())
    }

    Item {
        id: keys
        anchors.fill: parent
        focus: true
        Accessible.role: Accessible.Pane
        Accessible.name: qsTr("Region to capture")
        Accessible.description: qsTr("Arrow keys move the selection, Alt with arrows resizes it, Enter captures and Escape cancels.")

        Keys.onPressed: event => {
            if (event.key === Qt.Key_Escape) {
                captureFlow.cancel()
                event.accepted = true
                return
            }
            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                captureFlow.confirmRegion(overlay.hasSelection ? overlay.selection : overlay.wholeOutput())
                event.accepted = true
                return
            }
            const step = (event.modifiers & Qt.ShiftModifier) ? 10 : 1
            let dx = 0
            let dy = 0
            if (event.key === Qt.Key_Left) dx = -step
            else if (event.key === Qt.Key_Right) dx = step
            else if (event.key === Qt.Key_Up) dy = -step
            else if (event.key === Qt.Key_Down) dy = step
            else return
            const start = overlay.hasSelection ? overlay.selection
                                               : captureFlow.keyboardSelection(overlay.modelData.index)
            overlay.regionState.selection = captureFlow.nudgeSelection(
                start, dx, dy, (event.modifiers & (Qt.AltModifier | Qt.ControlModifier)) !== 0)
            event.accepted = true
        }
    }

    Component.onCompleted: {
        overlay.requestActivate()
        keys.forceActiveFocus()
    }
}
