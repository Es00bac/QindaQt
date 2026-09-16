// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// The console's presets (ADR-0182): load one, save the current console as a
// new one, or delete one. The list is the service's own, published in the
// snapshot, so this bar never has to ask twice.
//
// AGENT-GUARD: no text entry. The Audio route's intent surface is closed and
// its boundary gate refuses any text field; a new preset takes the first free
// "Preset N" name instead.
RowLayout {
    id: bar

    required property var model
    required property bool enabledControls

    readonly property var presets: model.consolePresets ?? []
    spacing: Tokens.space["2"]
    objectName: "consolePresets"

    ComboBox {
        id: picker
        objectName: "consolePresetPicker"
        Layout.fillWidth: true
        model: bar.presets
        enabled: bar.enabledControls && bar.presets.length > 0
        displayText: currentIndex < 0 ? qsTr("Presets") : currentText
        Accessible.name: qsTr("Console presets")
        onActivated: index => bar.model.loadPreset(bar.presets[index])
    }
    function nextName() {
        for (let index = 1; index < 1000; ++index) {
            const candidate = qsTr("Preset %1").arg(index)
            if (bar.presets.indexOf(candidate) < 0)
                return candidate
        }
        return ""
    }
    Button {
        objectName: "consolePresetSave"
        text: qsTr("Save as new")
        enabled: bar.enabledControls && bar.nextName() !== ""
        onClicked: bar.model.savePreset(bar.nextName())
        Accessible.name: qsTr("Save the console as a new preset")
    }
    Button {
        objectName: "consolePresetDelete"
        text: qsTr("Delete")
        enabled: bar.enabledControls && picker.currentIndex >= 0
        onClicked: bar.model.deletePreset(bar.presets[picker.currentIndex])
        Accessible.name: qsTr("Delete the selected preset")
    }
}
