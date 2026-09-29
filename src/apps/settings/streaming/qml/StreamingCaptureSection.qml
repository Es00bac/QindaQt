// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Preferences of the QindaQt Screenshot tool (ADR-0289): where captures go,
// what they are called, what a new capture starts with, and what happens
// when an OBS recording finishes.
//
// AGENT-CONTRACT: `captureSettings` is a Settings1ScreenshotPreferences. It
// validates and writes one key at a time and reads back before it reports
// "Saved."; this section only binds controls and restores their bindings
// after each request, so a refused write never leaves a control showing a
// value that was not stored.
ColumnLayout {
    id: root

    required property var captureSettings
    readonly property bool editable: root.captureSettings.loaded && !root.captureSettings.writePending
    // AGENT-GUARD: indices come from these arrays, not ComboBox.indexOfValue():
    // that function is not a binding dependency, so a binding evaluated before
    // the model populates would stay at the first entry forever.
    readonly property var modeChoices: [
        { "id": "region", "text": qsTr("A rectangular region") },
        { "id": "all-screens", "text": qsTr("All screens") },
        { "id": "current-screen", "text": qsTr("The current screen") },
        { "id": "active-window", "text": qsTr("The active window") },
        { "id": "window-under-pointer", "text": qsTr("The window under the pointer") }
    ]
    readonly property var delayChoices: [
        { "seconds": 0, "text": qsTr("No delay") },
        { "seconds": 3, "text": qsTr("3 seconds") },
        { "seconds": 5, "text": qsTr("5 seconds") },
        { "seconds": 10, "text": qsTr("10 seconds") }
    ]
    readonly property var finishChoices: [
        { "id": "notify", "text": qsTr("Notify with the file") },
        { "id": "show-in-folder", "text": qsTr("Show the file in its folder") },
        { "id": "quiet", "text": qsTr("Do nothing") }
    ]
    function indexIn(choices, key, value) {
        return Math.max(0, choices.findIndex(choice => choice[key] === value))
    }

    spacing: Tokens.space["2"]

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Screenshots and recording")
        description: qsTr("Used by the Screenshot tool, Print and the record shortcut")
    }

    FormRow {
        objectName: "captureFolderRow"
        Layout.fillWidth: true
        label: qsTr("Save folder")
        description: qsTr("Leave empty for %1").arg(root.captureSettings.effectiveFolder)
        editor: TextField {
            objectName: "captureFolderField"
            width: 280
            enabled: root.editable
            placeholderText: qsTr("Pictures/Screenshots")
            accessibleName: qsTr("Save folder")
            accessibleDescription: qsTr("Full path of the folder screenshots are saved to")
            text: root.captureSettings.folder
            onEditingFinished: {
                if (text !== root.captureSettings.folder)
                    root.captureSettings.setFolder(text)
                text = Qt.binding(function() { return root.captureSettings.folder })
            }
        }
    }

    FormRow {
        objectName: "capturePatternRow"
        Layout.fillWidth: true
        label: qsTr("File name")
        description: qsTr("{date}, {time} and {mode} are filled in. Next: %1")
                     .arg(root.captureSettings.fileNamePreview(root.captureSettings.fileNamePattern))
        editor: TextField {
            objectName: "capturePatternField"
            width: 280
            enabled: root.editable
            accessibleName: qsTr("File name pattern")
            accessibleDescription: qsTr("Name for new screenshots, without the extension")
            text: root.captureSettings.fileNamePattern
            error: root.captureSettings.fileNamePreview(text).length === 0
            onEditingFinished: {
                if (text !== root.captureSettings.fileNamePattern)
                    root.captureSettings.setFileNamePattern(text)
                text = Qt.binding(function() { return root.captureSettings.fileNamePattern })
            }
        }
    }

    FormRow {
        objectName: "captureModeRow"
        Layout.fillWidth: true
        label: qsTr("New screenshots capture")
        editor: ComboBox {
            objectName: "captureModeBox"
            enabled: root.editable
            textRole: "text"
            valueRole: "id"
            model: root.modeChoices
            currentIndex: root.indexIn(root.modeChoices, "id", root.captureSettings.defaultMode)
            Accessible.name: qsTr("Default capture area")
            onActivated: index => {
                root.captureSettings.setDefaultMode(root.modeChoices[index].id)
                currentIndex = Qt.binding(function() {
                    return root.indexIn(root.modeChoices, "id", root.captureSettings.defaultMode)
                })
            }
        }
    }

    FormRow {
        objectName: "captureDelayRow"
        Layout.fillWidth: true
        label: qsTr("Delay")
        editor: ComboBox {
            objectName: "captureDelayBox"
            enabled: root.editable
            textRole: "text"
            valueRole: "seconds"
            model: root.delayChoices
            currentIndex: root.indexIn(root.delayChoices, "seconds", root.captureSettings.delaySeconds)
            Accessible.name: qsTr("Default delay before capturing")
            onActivated: index => {
                root.captureSettings.setDelaySeconds(root.delayChoices[index].seconds)
                currentIndex = Qt.binding(function() {
                    return root.indexIn(root.delayChoices, "seconds", root.captureSettings.delaySeconds)
                })
            }
        }
    }

    FormRow {
        objectName: "captureShowResultRow"
        Layout.fillWidth: true
        label: qsTr("Show the result window")
        description: qsTr("Off: shortcut captures are saved straight away and announced")
        editor: Switch {
            objectName: "captureShowResultSwitch"
            enabled: root.editable
            checked: root.captureSettings.showResultWindow
            accessibleDescription: qsTr("Whether a shortcut capture opens the Screenshot window")
            onToggled: {
                root.captureSettings.setShowResultWindow(checked)
                checked = Qt.binding(function() { return root.captureSettings.showResultWindow })
            }
        }
    }

    FormRow {
        objectName: "captureRecordFinishRow"
        Layout.fillWidth: true
        label: qsTr("When a recording stops")
        editor: ComboBox {
            objectName: "captureRecordFinishBox"
            enabled: root.editable
            textRole: "text"
            valueRole: "id"
            model: root.finishChoices
            currentIndex: root.indexIn(root.finishChoices, "id", root.captureSettings.recordFinish)
            Accessible.name: qsTr("What happens when an OBS recording stops")
            onActivated: index => {
                root.captureSettings.setRecordFinish(root.finishChoices[index].id)
                currentIndex = Qt.binding(function() {
                    return root.indexIn(root.finishChoices, "id", root.captureSettings.recordFinish)
                })
            }
        }
    }

    Label {
        objectName: "captureWriteStatus"
        Layout.fillWidth: true
        muted: true
        visible: text.length > 0
        text: !root.captureSettings.loaded
              ? qsTr("Screenshot preferences are unavailable while the settings service is not running.")
              : root.captureSettings.writeStatusText
        Accessible.name: text
    }
}
