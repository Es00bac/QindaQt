// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Section for selecting resolution and refresh rate from server-advertised modes.
ColumnLayout {
    id: root

    required property var displaySettings
    required property bool editorBusy

    readonly property var currentModes: root.displaySettings.selectedOutput.modes ?? []
    readonly property string currentModeId: root.displaySettings.selectedOutput.modeId ?? ""
    readonly property bool outputEnabled: root.displaySettings.selectedOutput.enabled ?? false

    spacing: Tokens.space["3"]

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Resolution & Refresh Rate")
        description: qsTr("Choose from supported display modes reported by the compositor")
    }

    FormRow {
        Layout.fillWidth: true
        label: qsTr("Display resolution")
        description: qsTr("Active mode and refresh rate")
        errorMessage: root.displaySettings.fieldErrors[
                          root.displaySettings.selectedOutputId] ?? ""
        editor: modeSelector

        T.ComboBox {
            id: modeSelector
            objectName: "displayModeComboBox"
            Layout.fillWidth: true
            implicitHeight: 36
            enabled: root.displaySettings.canEdit && !root.editorBusy && root.outputEnabled

            model: root.currentModes
            textRole: "label"
            valueRole: "id"

            Accessible.role: Accessible.ComboBox
            Accessible.name: qsTr("Display resolution")
            Accessible.description: currentText

            currentIndex: {
                if (!root.currentModes) return -1;
                for (let i = 0; i < root.currentModes.length; ++i) {
                    if (root.currentModes[i].id === root.currentModeId) {
                        return i;
                    }
                }
                return -1;
            }

            onActivated: index => {
                if (index >= 0 && index < root.currentModes.length) {
                    const chosenMode = root.currentModes[index];
                    root.displaySettings.setOutputMode(
                        root.displaySettings.selectedOutputId, chosenMode.id);
                }
            }

            contentItem: Text {
                leftPadding: Tokens.space["3"]
                rightPadding: modeSelector.indicator.width + Tokens.space["3"]
                text: modeSelector.displayText
                font.family: Tokens.type.fontFamily
                font.pointSize: Tokens.type.body
                color: modeSelector.enabled ? Tokens.fg.default : Tokens.fg.disabled
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            background: Rectangle {
                radius: Tokens.radius.m
                color: Tokens.bg.highest
                border.width: modeSelector.activeFocus ? Tokens.space["1"] : Tokens.space["1"] / 2
                border.color: modeSelector.activeFocus ? Tokens.focus.ring : Tokens.outline.strong
            }
        }
    }
}
