// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0

// Aggregated system status: one button showing a lane icon per granted
// service (sound, Bluetooth, power), and a popup with each lane's quick
// controls. All truth and every request come from the borrowed facades.
Item {
    id: root

    required property var access
    property bool vertical: false

    readonly property bool ready: access !== null && Tokens.ready
    readonly property var lanes: ready ? access.laneRows : []
    readonly property int iconExtent: Math.max(0, Math.min(18, height - Tokens.space["2"]))

    objectName: "systemStatusApplet"
    implicitWidth: summary.implicitWidth
    implicitHeight: vertical ? summary.implicitHeight : 28

    function facadeFor(laneId) {
        if (!root.ready)
            return null
        if (laneId === "audio") return root.access.audio
        if (laneId === "bluetooth") return root.access.bluetooth
        if (laneId === "power") return root.access.power
        return null
    }

    function controlGrantedFor(laneId) {
        if (!root.ready)
            return false
        if (laneId === "audio") return Boolean(root.access.audioControlGranted)
        if (laneId === "bluetooth") return Boolean(root.access.bluetoothControlGranted)
        if (laneId === "power") return Boolean(root.access.powerControlGranted)
        return false
    }

    T.ToolButton {
        id: summary
        objectName: "systemStatusSummary"
        anchors.fill: parent
        enabled: root.ready && root.lanes.length > 0
        focusPolicy: Qt.TabFocus
        hoverEnabled: true
        padding: Tokens.space["1"]
        implicitWidth: Math.max(28, laneIcons.implicitWidth + leftPadding + rightPadding)
        implicitHeight: Math.max(28, laneIcons.implicitHeight + topPadding + bottomPadding)
        text: ""

        Accessible.role: Accessible.Button
        Accessible.name: root.ready ? String(root.access.accessibleName) : qsTr("System status")
        Accessible.description: root.ready ? String(root.access.accessibleDescription)
                                           : qsTr("System status is not connected")

        function openDetails() {
            if (enabled)
                details.open()
        }

        onClicked: openDetails()
        Keys.onReturnPressed: openDetails()
        Keys.onEnterPressed: openDetails()
        Accessible.onPressAction: openDetails()

        contentItem: GridLayout {
            id: laneIcons
            flow: root.vertical ? GridLayout.TopToBottom : GridLayout.LeftToRight
            rows: root.vertical ? -1 : 1
            columns: root.vertical ? 1 : -1
            rowSpacing: Tokens.space["1"]
            columnSpacing: Tokens.space["2"]

            ShellIcons.Icon {
                objectName: "systemStatusPlaceholderIcon"
                visible: root.lanes.length === 0
                name: "preferences-plugin"
                size: root.iconExtent
                color: Tokens.fg.disabled
                symbolic: true
                fallbackText: qsTr("Status")
                Accessible.ignored: true
            }

            Repeater {
                model: root.lanes

                ShellIcons.Icon {
                    required property var modelData

                    objectName: "systemStatusLaneIcon"
                    name: String(modelData.iconName)
                    size: root.iconExtent
                    color: Boolean(modelData.attention) ? Tokens.status.warning.foreground
                         : Boolean(modelData.available) ? Tokens.fg.default : Tokens.fg.disabled
                    symbolic: true
                    fallbackText: String(modelData.label)
                    Accessible.ignored: true
                }
            }
        }

        background: Rectangle {
            radius: Tokens.radius.m
            color: summary.down ? Tokens.state.pressed
                 : summary.hovered ? Tokens.state.hover : "transparent"
            C.FocusRing { anchors.fill: parent; control: summary }
        }
    }

    ControlPopupFrame {
        id: details
        objectName: "systemStatusPopup"
        width: 340
        heading: qsTr("System status")
        onClosed: summary.forceActiveFocus(Qt.PopupFocusReason)

        Repeater {
            model: root.lanes

            StatusLaneRow {
                required property var modelData

                laneRow: modelData
                facade: root.facadeFor(String(modelData.id))
                controlGranted: root.controlGrantedFor(String(modelData.id))
            }
        }
    }
}
