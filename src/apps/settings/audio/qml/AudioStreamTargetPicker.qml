// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaTK as Tk
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// One stream's route chooser; selected truth comes only from Audio1 readback.
ColumnLayout {
    id: root
    required property var targetRow
    required property var audioSettings
    property alias entryControl: targetPicker
    spacing: Tokens.space["1"]

    Label {
        Layout.fillWidth: true
        text: root.targetRow?.targetKindText ?? qsTr("Device")
        muted: true
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }

    Tk.ComboBox {
        id: targetPicker
        objectName: "audioStreamTarget_" + (root.targetRow?.serial ?? 0)
        Layout.fillWidth: true
        small: true
        // A wheel over the chooser scrolls the compact Devices page.
        wheelEnabled: false
        readonly property var choices: root.targetRow?.targetChoices ?? []
        model: choices
        textRole: "label"
        // AGENT-GUARD: click/keyboard activation is intent, never route truth.
        readonly property int authoritativeIndex: {
            const selected = Number(root.targetRow?.targetSerial ?? 0)
            for (let i = 0; i < choices.length; ++i) {
                if (Number(choices[i].serial) === selected)
                    return i
            }
            return -1
        }
        currentIndex: authoritativeIndex
        function restoreAuthoritativeSelection() {
            // Qt's ComboBox writes currentIndex before activated, breaking
            // an ordinary binding. Reinstall it after dispatch/readback.
            targetPicker.currentIndex = Qt.binding(
                () => targetPicker.authoritativeIndex)
        }
        displayText: root.targetRow?.targetName ?? qsTr("Unknown device")
        enabled: root.targetRow?.moveAvailable ?? false
        tooltip: qsTr("%1 for %2")
            .arg(root.targetRow?.targetKindText ?? qsTr("Device"))
            .arg(root.targetRow?.applicationName ?? "")
        Accessible.name: tooltip
        onActivated: index => {
            if (root.targetRow !== null
                    && index >= 0 && index < choices.length) {
                const chosen = Number(choices[index].serial)
                if (chosen !== Number(root.targetRow.targetSerial))
                    root.audioSettings.moveStream(root.targetRow.serial, chosen)
            }
            Qt.callLater(targetPicker.restoreAuthoritativeSelection)
        }
    }
}
