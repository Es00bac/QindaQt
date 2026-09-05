// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0

// One service lane inside the system-status popup: the lane summary plus the
// quick control that lane supports. Every control re-enters the borrowed
// service facade (`facade`), which applies its own grants and fences; this
// row never mutates anything itself.
ColumnLayout {
    id: lane

    required property var laneRow
    required property var facade
    property bool controlGranted: false

    readonly property string laneId: String(laneRow.id)
    readonly property bool available: Boolean(laneRow.available) && facade !== null
    readonly property var defaultOutput: {
        if (laneId !== "audio" || facade === null)
            return null
        const rows = facade.deviceRows
        for (let i = 0; i < rows.length; ++i)
            if (rows[i].isOutput && rows[i].isDefault)
                return rows[i]
        return null
    }
    readonly property var firstAdapter: laneId === "bluetooth" && facade !== null
                                        && facade.adapterRows.length > 0
                                        ? facade.adapterRows[0] : null

    objectName: "statusLaneRow"
    spacing: Tokens.space["1"]
    Layout.fillWidth: true

    RowLayout {
        Layout.fillWidth: true
        spacing: Tokens.space["2"]

        ShellIcons.Icon {
            objectName: "statusLaneIcon"
            name: String(lane.laneRow.iconName)
            size: 18
            color: lane.available ? Tokens.fg.default : Tokens.fg.disabled
            symbolic: true
            fallbackText: String(lane.laneRow.label)
            Accessible.ignored: true
        }

        C.Label {
            objectName: "statusLaneSummary"
            Layout.fillWidth: true
            text: lane.available ? String(lane.laneRow.summary)
                                 : qsTr("%1 %2").arg(String(lane.laneRow.label))
                                                .arg(String(lane.laneRow.phase))
            elide: Text.ElideRight
            maximumLineCount: 1
            Accessible.name: String(lane.laneRow.accessibleName)
            Accessible.description: String(lane.laneRow.accessibleDescription)
        }
    }

    // Sound: default output volume and mute.
    RowLayout {
        objectName: "statusLaneAudioControls"
        visible: lane.laneId === "audio" && lane.available && lane.defaultOutput !== null
        Layout.fillWidth: true
        spacing: Tokens.space["2"]

        C.Slider {
            objectName: "statusLaneVolumeSlider"
            Layout.fillWidth: true
            from: 0
            to: 1
            stepSize: 0.05
            enabled: lane.controlGranted && lane.defaultOutput !== null
                     && lane.defaultOutput.canSetVolume && !lane.defaultOutput.pending
            value: lane.defaultOutput !== null && lane.defaultOutput.volumeKnown
                   ? lane.defaultOutput.volume : 0
            accessibleName: qsTr("Output volume")
            onMoved: lane.facade.requestVolume(lane.defaultOutput.serial, false, value)
        }

        C.Switch {
            objectName: "statusLaneMuteSwitch"
            text: qsTr("Mute")
            enabled: lane.controlGranted && lane.defaultOutput !== null
                     && lane.defaultOutput.canSetMute && !lane.defaultOutput.pending
            checked: lane.defaultOutput !== null && lane.defaultOutput.muteKnown
                     && lane.defaultOutput.muted
            onToggled: lane.facade.requestMute(lane.defaultOutput.serial, false, checked)
        }
    }

    // Bluetooth: adapter power.
    C.Switch {
        objectName: "statusLaneBluetoothSwitch"
        visible: lane.laneId === "bluetooth" && lane.available && lane.firstAdapter !== null
        text: qsTr("Bluetooth on")
        enabled: lane.controlGranted && lane.firstAdapter !== null
                 && lane.firstAdapter.canSetPowered && !lane.facade.operationPending
        checked: lane.firstAdapter !== null && lane.firstAdapter.powered
        onToggled: lane.facade.requestAdapterPower(lane.firstAdapter.id, checked)
    }

    // Power: profile choice.
    Flow {
        objectName: "statusLanePowerProfiles"
        visible: lane.laneId === "power" && lane.available && lane.facade.profileRows.length > 0
        Layout.fillWidth: true
        spacing: Tokens.space["1"]

        Repeater {
            model: lane.laneId === "power" && lane.facade !== null ? lane.facade.profileRows : []

            T.Button {
                required property var modelData

                objectName: "statusLaneProfileButton"
                text: modelData.active ? qsTr("%1 (current)").arg(modelData.label)
                                       : modelData.label
                enabled: lane.controlGranted && modelData.adjustable && !modelData.pending
                         && !lane.facade.operationPending
                focusPolicy: Qt.TabFocus
                padding: Tokens.space["2"]
                Accessible.role: Accessible.RadioButton
                Accessible.name: modelData.accessibleName
                Accessible.checked: modelData.active
                onClicked: lane.facade.requestProfile(modelData.profileId)
                contentItem: Text {
                    text: parent.text
                    color: parent.enabled ? Tokens.fg.default : Tokens.fg.disabled
                    font.family: Tokens.type.fontFamily
                    font.pointSize: Tokens.type.caption
                }
                background: Rectangle {
                    radius: Tokens.radius.s
                    color: modelData.active ? Tokens.bg.highest : "transparent"
                    border.width: Tokens.space["1"] / 2
                    border.color: Tokens.outline.divider
                }
            }
        }
    }
}
