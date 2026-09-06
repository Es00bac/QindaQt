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
    // Position in the inventory; the arrangement diagram shows the same number.
    property int ordinal: 0
    // Cards state the native mode separately from the logical diagram size.
    // At 200%, a 3840 × 2160 panel is 1920 × 1080 logical; presenting only
    // the latter here would misidentify the monitor's selected resolution.
    readonly property var selectedMode: {
        const modes = root.outputData.modes ?? []
        for (let index = 0; index < modes.length; ++index) {
            if (modes[index].id === root.outputData.modeId) {
                return modes[index]
            }
        }
        return null
    }
    readonly property string summaryText: selectedMode !== null
                                          ? qsTr("%1 × %2 pixels · %3% scale")
                                            .arg(selectedMode.pixelWidth)
                                            .arg(selectedMode.pixelHeight)
                                            .arg(Math.round((root.outputData.scale ?? 1) * 100))
                                          : qsTr("%1 × %2 logical · %3% scale")
                                            .arg(root.outputData.logicalWidth)
                                            .arg(root.outputData.logicalHeight)
                                            .arg(Math.round((root.outputData.scale ?? 1) * 100))
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
    Accessible.name: (root.ordinal > 0 ? qsTr("Display %1: ").arg(root.ordinal) : "")
                     + qsTr("%1 (%2)%3").arg(root.outputData.label)
                                        .arg(root.outputData.connectorName)
                                        .arg(root.outputData.primary ? qsTr(", Primary") : "")
    Accessible.description: root.outputData.enabled
                            ? qsTr("Enabled, %1×%2").arg(root.outputData.logicalWidth)
                                                   .arg(root.outputData.logicalHeight)
                            : qsTr("Disabled. Select it, then turn on Enable display.")
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

            Rectangle {
                objectName: "displayOutputCardOrdinal"
                visible: root.ordinal > 0
                implicitWidth: Math.max(22, ordinalText.implicitWidth + Tokens.space["2"])
                implicitHeight: 22
                radius: height / 2
                color: root.outputData.enabled ? Tokens.accent.default : Tokens.bg.highest
                border.width: root.outputData.enabled ? 0 : Tokens.space["1"] / 2
                border.color: Tokens.outline.strong
                Accessible.ignored: true

                Text {
                    id: ordinalText
                    anchors.centerIn: parent
                    text: root.ordinal
                    font.family: Tokens.type.fontFamily
                    font.pointSize: Tokens.type.caption
                    font.weight: Font.DemiBold
                    color: root.outputData.enabled ? Tokens.accent.fg : Tokens.fg.muted
                }
            }

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
            text: root.outputData.enabled ? root.summaryText
                                            : qsTr("Disabled — select to enable")
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
