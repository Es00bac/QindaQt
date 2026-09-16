// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// The console's preset, macro, and VBAN bar. One line holds the preset
// picker and its save/delete actions; macros and VBAN toggles flow beneath,
// wrapping with the window instead of overflowing it.
//
// AGENT-GUARD: no text entry. The Audio route's intent surface is closed and
// its boundary gate refuses any text field; a new preset takes the first free
// "Preset N" name instead.
ColumnLayout {
    id: bar

    required property var model
    required property bool enabledControls

    readonly property var presets: model.consolePresets ?? []
    spacing: Tokens.space["1"]
    objectName: "consolePresets"

    RowLayout {
        Layout.fillWidth: true
        spacing: Tokens.space["1"]

        ComboBox {
            id: picker
            objectName: "consolePresetPicker"
            Layout.fillWidth: true
            Layout.preferredHeight: 24
            model: bar.presets
            enabled: bar.enabledControls && bar.presets.length > 0
            displayText: currentIndex < 0 ? qsTr("Presets") : currentText
            Accessible.name: qsTr("Console presets")
            onActivated: index => bar.model.loadPreset(bar.presets[index])
        }
        AudioConsolePad {
            objectName: "consolePresetSave"
            Layout.preferredHeight: 24
            text: qsTr("Save as new")
            available: bar.enabledControls && bar.nextName() !== ""
            onClicked: bar.model.savePreset(bar.nextName())
            Accessible.name: qsTr("Save the console as a new preset")
        }
        AudioConsolePad {
            objectName: "consolePresetDelete"
            Layout.preferredHeight: 24
            text: qsTr("Delete")
            available: bar.enabledControls && picker.currentIndex >= 0
            onClicked: bar.model.deletePreset(bar.presets[picker.currentIndex])
            Accessible.name: qsTr("Delete the selected preset")
        }
    }

    // Macro buttons (ADR-0183) and VBAN toggles (ADR-0185) share one wrapping
    // line: both are "press and the desk reacts" controls, and either list
    // can grow without bound in the user's document.
    Flow {
        Layout.fillWidth: true
        spacing: Tokens.space["1"]

        Repeater {
            model: bar.model.consoleMacros ?? []
            delegate: AudioConsolePad {
                required property string modelData
                Layout.preferredHeight: 24
                objectName: "consoleMacro_" + modelData
                text: modelData
                available: bar.enabledControls
                onClicked: bar.model.runMacro(modelData)
                Accessible.name: qsTr("Run macro %1").arg(modelData)
            }
        }
        Repeater {
            model: bar.model.consoleVban ?? []
            delegate: AudioConsolePad {
                required property var modelData
                Layout.preferredHeight: 24
                objectName: "consoleVban_" + modelData.name
                text: (modelData.outgoing ? qsTr("Send %1") : qsTr("Receive %1")).arg(modelData.name)
                checkable: true
                checked: modelData.enabled === true
                available: bar.enabledControls
                onToggled: bar.model.setVbanEnabled(modelData.name, checked)
                Accessible.name: text
            }
        }
    }

    function nextName() {
        for (let index = 1; index < 1000; ++index) {
            const candidate = qsTr("Preset %1").arg(index)
            if (bar.presets.indexOf(candidate) < 0)
                return candidate
        }
        return ""
    }
}
