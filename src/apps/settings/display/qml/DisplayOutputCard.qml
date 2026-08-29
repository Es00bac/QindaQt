// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Card representing a single monitor / display output in the inventory.
Rectangle {
    id: root

    required property var outputData
    required property bool selected
    required property bool canEdit
    signal selectedRequested()

    implicitWidth: 200
    implicitHeight: 90
    radius: Tokens.radius.m
    color: root.selected ? Tokens.bg.highest : Tokens.bg.raised
    border.width: root.selected ? Tokens.space["1"] : Tokens.space["1"] / 2
    border.color: root.selected ? Tokens.focus.ring : Tokens.outline.strong

    Accessible.role: Accessible.RadioButton
    Accessible.name: qsTr("%1 (%2)%3").arg(root.outputData.label)
                                      .arg(root.outputData.connectorName)
                                      .arg(root.outputData.primary ? qsTr(", Primary") : "")
    Accessible.description: root.outputData.enabled
                            ? qsTr("Enabled, %1×%2").arg(root.outputData.logicalWidth)
                                                   .arg(root.outputData.logicalHeight)
                            : qsTr("Disabled")
    Accessible.checked: root.selected

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.selectedRequested()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tokens.space["3"]
        spacing: Tokens.space["1"]

        RowLayout {
            Layout.fillWidth: true
            spacing: Tokens.space["2"]

            Label {
                Layout.fillWidth: true
                text: root.outputData.label
                font.family: Tokens.type.fontFamily
                font.pointSize: Tokens.type.body
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                color: root.outputData.enabled ? Tokens.fg.default : Tokens.fg.disabled
            }

            Rectangle {
                visible: root.outputData.primary
                implicitWidth: primaryText.implicitWidth + Tokens.space["2"] * 2
                implicitHeight: primaryText.implicitHeight + Tokens.space["1"]
                radius: Tokens.radius.s
                color: Tokens.accent.default

                Text {
                    id: primaryText
                    anchors.centerIn: parent
                    text: qsTr("Primary")
                    font.family: Tokens.type.fontFamily
                    font.pointSize: Tokens.type.caption
                    font.weight: Font.DemiBold
                    color: Tokens.accent.fg
                }
            }
        }

        Label {
            Layout.fillWidth: true
            text: root.outputData.connectorName
            font.family: Tokens.type.fontFamily
            font.pointSize: Tokens.type.caption
            color: Tokens.fg.muted
        }

        Label {
            Layout.fillWidth: true
            text: root.outputData.enabled
                  ? qsTr("%1×%2 @ %3x").arg(root.outputData.logicalWidth)
                                       .arg(root.outputData.logicalHeight)
                                       .arg(root.outputData.scale)
                  : qsTr("Disabled")
            font.family: Tokens.type.fontFamily
            font.pointSize: Tokens.type.caption
            color: root.outputData.enabled ? Tokens.fg.default : Tokens.fg.disabled
        }
    }
}
