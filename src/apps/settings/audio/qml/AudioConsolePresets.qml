// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QindaTK as Tk

// The console's preset, macro, and VBAN bar. One line holds the preset
// picker and its save/delete actions; macros and VBAN toggles flow beneath,
// wrapping with the window instead of overflowing it.
//
// AGENT-GUARD: no text entry. The Audio route's intent surface is closed and
// its boundary gate refuses any text field; a new preset takes the first free
// "Preset N" name instead.
Tk.Flex {
    id: bar

    required property var model
    required property bool enabledControls

    readonly property var presets: model.consolePresets ?? []

    direction: Tk.Flex.Column
    gap: Tk.Theme.space.xs
    objectName: "consolePresets"

    function nextName() {
        for (let index = 1; index < 1000; ++index) {
            const candidate = qsTr("Preset %1").arg(index)
            if (bar.presets.indexOf(candidate) < 0)
                return candidate
        }
        return ""
    }

    Tk.Flex {
        gap: Tk.Theme.space.xs

        Tk.ComboBox {
            id: picker
            objectName: "consolePresetPicker"
            small: true
            Tk.Flex.grow: 1
            Tk.Flex.maxWidth: 280
            model: bar.presets
            enabled: bar.enabledControls && bar.presets.length > 0
            displayText: currentIndex < 0 ? qsTr("Presets") : currentText
            Accessible.name: qsTr("Console presets")
            tooltip: qsTr("Console presets")
            onActivated: index => bar.model.loadPreset(bar.presets[index])
        }
        Tk.Button {
            objectName: "consolePresetSave"
            small: true
            text: qsTr("Save as new")
            available: bar.enabledControls && bar.nextName() !== ""
            onClicked: bar.model.savePreset(bar.nextName())
            Accessible.name: qsTr("Save the console as a new preset")
            tooltip: qsTr("Save the console as a new preset")
        }
        Tk.Button {
            objectName: "consolePresetDelete"
            small: true
            text: qsTr("Delete")
            available: bar.enabledControls && picker.currentIndex >= 0
            onClicked: bar.model.deletePreset(bar.presets[picker.currentIndex])
            Accessible.name: qsTr("Delete the selected preset")
            tooltip: qsTr("Delete the selected preset")
        }
    }

    Tk.Caption {
        objectName: "consoleMacroCaption"
        visible: (bar.model.consoleMacros?.length ?? 0) > 0
                 || (bar.model.consoleVban?.length ?? 0) > 0
        text: qsTr("Macros and network streams")
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }

    // Macro buttons (ADR-0183) and VBAN toggles (ADR-0185) share one wrapping
    // line: both are "press and the desk reacts" controls, and either list
    // can grow without bound in the user's document.
    Tk.Flex {
        wrap: Tk.Flex.Wrap
        gap: Tk.Theme.space.xs

        Repeater {
            model: bar.model.consoleMacros ?? []
            delegate: AudioConsolePad {
                required property string modelData
                implicitHeight: 20
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
                id: vbanPad
                required property var modelData
                implicitHeight: 20
                objectName: "consoleVban_" + modelData.name
                text: (modelData.outgoing ? qsTr("Send %1") : qsTr("Receive %1")).arg(modelData.name)
                checkable: true
                available: bar.enabledControls
                Binding on checked { value: vbanPad.modelData.enabled === true; when: !vbanPad.down }
                onToggled: bar.model.setVbanEnabled(modelData.name, checked)
                Accessible.name: text
            }
        }
    }
}
