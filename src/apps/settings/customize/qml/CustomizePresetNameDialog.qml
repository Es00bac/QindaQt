// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Names a preset: "save" keeps the current layout as a new preset, "saveAs"
// keeps an edited built-in as a new preset, "rename" renames an own preset.
// The model validates every keystroke (presetNameError) so Save is only
// offered for a name it will accept; a store failure stays in the dialog.
T.Dialog {
    id: dialog

    required property var customizeSettings
    property string mode: "save"
    property string presetId: ""
    property string failure: ""
    readonly property string nameError: dialog.customizeSettings.presetNameError(
        nameField.text, dialog.mode === "rename" ? dialog.presetId : "")

    objectName: "customizePresetNameDialog"
    title: dialog.mode === "rename" ? qsTr("Rename preset") : qsTr("Save as preset")
    modal: true
    anchors.centerIn: T.Overlay.overlay
    width: Math.min(420, (T.Overlay.overlay !== null ? T.Overlay.overlay.width : 420)
                         - 2 * Tokens.space["4"])

    function openFor(nextMode, id, currentName) {
        dialog.mode = nextMode
        dialog.presetId = id
        dialog.failure = ""
        nameField.text = nextMode === "rename" ? currentName
                       : nextMode === "saveAs" ? qsTr("%1 (edited)").arg(currentName)
                       : ""
        dialog.open()
    }

    function submit() {
        if (dialog.nameError.length > 0) {
            return
        }
        const saved = dialog.mode === "rename"
            ? dialog.customizeSettings.renamePreset(dialog.presetId, nameField.text)
            : dialog.customizeSettings.savePresetAs(dialog.presetId, nameField.text)
        if (saved) {
            dialog.close()
        } else {
            dialog.failure = dialog.customizeSettings.errorText
        }
    }

    onOpened: {
        nameField.forceActiveFocus(Qt.TabFocusReason)
        nameField.selectAll()
    }

    contentItem: ColumnLayout {
        spacing: Tokens.space["2"]

        Label {
            Layout.fillWidth: true
            text: dialog.mode === "rename"
                  ? qsTr("Choose a new name for this preset.")
                  : dialog.mode === "saveAs"
                    ? qsTr("Your edits are kept as a new preset in My presets.")
                    : qsTr("The current layout, with every change made on the panels, is kept as a new preset in My presets.")
            wrapMode: Text.Wrap
        }

        TextField {
            id: nameField

            objectName: "customizePresetNameField"
            Layout.fillWidth: true
            placeholderText: qsTr("Preset name")
            maximumLength: dialog.customizeSettings.maximumNameLength
            accessibleName: qsTr("Preset name")
            accessibleDescription: dialog.nameError
            error: text.length > 0 && dialog.nameError.length > 0
            onTextEdited: dialog.failure = ""
            onAccepted: dialog.submit()
        }

        Label {
            objectName: "customizePresetNameError"
            Layout.fillWidth: true
            visible: text.length > 0
            text: dialog.failure.length > 0 ? dialog.failure
                  : nameField.text.length > 0 ? dialog.nameError : ""
            muted: true
            wrapMode: Text.Wrap
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Tokens.space["2"]

            Item { Layout.fillWidth: true }

            Button {
                objectName: "customizePresetNameCancel"
                text: qsTr("Cancel")
                emphasized: false
                onClicked: dialog.close()
            }
            Button {
                objectName: "customizePresetNameSave"
                text: dialog.mode === "rename" ? qsTr("Rename") : qsTr("Save")
                available: dialog.nameError.length === 0
                onClicked: dialog.submit()
            }
        }
    }
}
