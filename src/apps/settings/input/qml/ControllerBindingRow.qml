// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

FormRow {
    id: root
    required property var binding
    required property var controllerModel
    property string chosenAction: binding.action
    signal captureActivityChanged(bool active)
    label: binding.label
    description: chosenAction === "dictate" ? qsTr("Hold while speaking; release to insert the text") : ""
    editor: RowLayout {
        spacing: Tokens.space["2"]
        ComboBox {
            id: actionChoice
            objectName: "controllerBinding_" + root.binding.id
            Layout.preferredWidth: 240
            model: root.controllerModel.actions
            textRole: "label"
            valueRole: "value"
            currentIndex: Math.max(0, root.controllerModel.actions.findIndex(
                                      entry => entry.value === root.chosenAction))
            enabled: root.controllerModel.available && !root.controllerModel.busy
            Accessible.name: root.binding.label + qsTr(" action")
            onActivated: index => {
                root.chosenAction = root.controllerModel.actions[index].value
                if (root.chosenAction !== "shortcut")
                    root.controllerModel.setBinding(root.binding.id, root.chosenAction)
                else
                    shortcutCapture.beginCapture()
            }
        }
        ShortcutCaptureButton {
            id: shortcutCapture
            objectName: "controllerShortcut_" + root.binding.id
            visible: root.chosenAction === "shortcut"
            sequence: root.controllerModel.sequenceKey(root.binding.shortcut ?? "")
            formatter: key => root.controllerModel.sequenceText(key)
            enabled: root.controllerModel.available && !root.controllerModel.busy
            Accessible.name: root.binding.label + qsTr(" keyboard shortcut")
            onCaptured: key => root.controllerModel.setShortcut(root.binding.id, key)
            onCleared: root.controllerModel.setBinding(root.binding.id, "none")
            onCaptureActivityChanged: active => root.captureActivityChanged(active)
        }
    }
}
