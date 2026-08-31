// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Card representing a single monitor / display output in the inventory.
T.AbstractButton {
    id: root

    required property var outputData
    required property bool selected
    required property bool canEdit
    signal selectedRequested()

    implicitWidth: 200
    implicitHeight: 90
    checkable: true
    autoExclusive: true
    checked: root.selected
    enabled: root.canEdit
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    activeFocusOnTab: true

    Accessible.role: Accessible.RadioButton
    Accessible.name: qsTr("%1 (%2)%3").arg(root.outputData.label)
                                      .arg(root.outputData.connectorName)
                                      .arg(root.outputData.primary ? qsTr(", Primary") : "")
    Accessible.description: root.outputData.enabled
                            ? qsTr("Enabled, %1×%2").arg(root.outputData.logicalWidth)
                                                   .arg(root.outputData.logicalHeight)
                            : qsTr("Disabled")
    Accessible.checkable: true
    Accessible.checked: root.selected
    Accessible.onPressAction: root.click()

    onClicked: root.selectedRequested()
    Keys.onReturnPressed: event => {
        root.click()
        event.accepted = true
    }
    Keys.onEnterPressed: event => {
        root.click()
        event.accepted = true
    }
    Keys.onSpacePressed: event => {
        root.click()
        event.accepted = true
    }

    contentItem: ColumnLayout {
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

    background: Rectangle {
        radius: Tokens.radius.m
        color: root.selected ? Tokens.bg.highest : Tokens.bg.raised
        border.width: root.activeFocus || root.selected
                      ? Tokens.space["1"] : Tokens.space["1"] / 2
        border.color: root.activeFocus ? Tokens.focus.ring
                      : root.selected ? Tokens.accent.default : Tokens.outline.strong

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: root.down ? Tokens.state.pressed
                 : root.hovered ? Tokens.state.hover : "transparent"
            Accessible.ignored: true
        }
    }
}
