// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Every global shortcut the desktop authority reports, searchable, with
// capture-to-assign, conflict naming, reset, clear, and custom command
// shortcuts. Conflicts are computed by the model and always name the
// conflicting action (ADR-0134).
ColumnLayout {
    id: root

    required property var inputSettings
    readonly property var shortcuts: inputSettings.shortcuts

    readonly property Item firstFocusTarget: degraded.visible ? degraded
                                             : searchField

    Component.onCompleted: shortcuts.refresh()

    spacing: Tokens.space["2"]

    function conflictText(conflicts) {
        return qsTr("Already used by %1").arg(conflicts.join(", "))
    }

    DegradedNotice {
        id: degraded
        objectName: "inputShortcutsDegraded"
        Layout.fillWidth: true
        visible: !shortcuts.available
        reason: qsTr("Global shortcuts are unavailable right now.")
    }

    FormRow {
        objectName: "inputShortcutsSearchRow"
        Layout.fillWidth: true
        visible: shortcuts.available
        label: qsTr("Search")
        description: qsTr("Filter by component or action name")
        editor: TextField {
            id: searchField
            objectName: "inputShortcutsSearchField"
            placeholderText: qsTr("Type to filter")
            Layout.preferredWidth: 320
            onTextChanged: shortcuts.filter = text
            Accessible.description: parent.description
        }
    }

    Label {
        objectName: "inputShortcutsStatus"
        Layout.fillWidth: true
        visible: text.length > 0
        text: shortcuts.statusText
        wrapMode: Text.Wrap
        Accessible.role: Accessible.AlertMessage
        Accessible.name: text
    }

    ListView {
        id: shortcutList
        objectName: "inputShortcutsList"
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: shortcuts.available
        clip: true
        model: shortcuts
        spacing: Tokens.space["1"]
        Accessible.role: Accessible.List
        Accessible.name: qsTr("Global shortcuts")

        delegate: Rectangle {
            id: row
            required property int index
            required property string componentName
            required property string actionName
            required property string keys
            required property string defaultKeys
            required property bool isCommand
            required property string command
            width: ListView.view.width
            height: rowLayout.implicitHeight + Tokens.space["3"] * 2
            color: Tokens.bg.raised
            radius: Tokens.radius.s

            // Capture state for this row; null while not capturing.
            property var pendingKeys: null
            readonly property bool capturing: pendingKeys !== null

            ColumnLayout {
                id: rowLayout
                anchors.fill: parent
                anchors.margins: Tokens.space["3"]
                spacing: Tokens.space["1"]

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        Layout.fillWidth: true
                        text: row.actionName
                        font.weight: Font.DemiBold
                    }
                    Label {
                        visible: row.isCommand && row.command.length > 0
                        text: row.command
                        muted: true
                        elide: Text.ElideMiddle
                        Layout.maximumWidth: 220
                    }
                }
                Label {
                    text: row.componentName
                    muted: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Tokens.space["2"]
                    Label {
                        objectName: "inputShortcutKeys_" + row.index
                        Layout.fillWidth: true
                        text: row.capturing
                              ? qsTr("Press keys — Esc cancels, Backspace clears")
                              : row.keys.length > 0 ? row.keys
                                                    : qsTr("Disabled")
                        muted: !row.capturing && row.keys.length === 0
                    }
                    Button {
                        objectName: "inputShortcutReset_" + row.index
                        text: qsTr("Reset")
                        enabled: !row.capturing && row.defaultKeys.length > 0
                                 && row.keys !== row.defaultKeys
                        onClicked: shortcuts.resetToDefault(row.index)
                    }
                    Button {
                        objectName: "inputShortcutClear_" + row.index
                        text: qsTr("Clear")
                        enabled: !row.capturing && row.keys.length > 0
                        onClicked: shortcuts.clear(row.index)
                    }
                    Button {
                        objectName: "inputShortcutRemove_" + row.index
                        text: qsTr("Remove")
                        visible: row.isCommand
                        onClicked: shortcuts.removeCommand(row.index)
                    }
                    ShortcutCaptureButton {
                        objectName: "inputShortcutCapture_" + row.index
                        sequence: 0
                        captureHint: qsTr("Capturing…")
                        placeholderText: qsTr("Change")
                        formatter: key => shortcuts.displayKey(key)
                        enabled: !shortcuts.busy
                        onCaptured: sequence => {
                            row.pendingKeys = [sequence]
                            const conflicts =
                                shortcuts.conflictsFor(row.pendingKeys)
                            if (conflicts.length === 0) {
                                shortcuts.assign(row.index, row.pendingKeys)
                                row.pendingKeys = null
                            }
                        }
                    }
                }

                RowLayout {
                    visible: row.capturing
                        && shortcuts.conflictsFor(row.pendingKeys).length > 0
                    Layout.fillWidth: true
                    spacing: Tokens.space["2"]
                    Label {
                        objectName: "inputShortcutConflict_" + row.index
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        text: row.capturing
                              ? root.conflictText(shortcuts.conflictsFor(
                                                      row.pendingKeys))
                              : ""
                        Accessible.role: Accessible.AlertMessage
                        Accessible.name: text
                    }
                    Button {
                        objectName: "inputShortcutConflictAssign_" + row.index
                        text: qsTr("Assign anyway")
                        onClicked: {
                            shortcuts.assign(row.index, row.pendingKeys)
                            row.pendingKeys = null
                        }
                    }
                    Button {
                        objectName: "inputShortcutConflictCancel_" + row.index
                        text: qsTr("Cancel")
                        onClicked: row.pendingKeys = null
                    }
                }
            }
        }
    }

    SectionHeader {
        objectName: "inputCustomCommandsHeader"
        Layout.fillWidth: true
        visible: shortcuts.available
        title: qsTr("Custom command shortcut")
    }

    RowLayout {
        objectName: "inputCustomCommandRow"
        Layout.fillWidth: true
        visible: shortcuts.available
        spacing: Tokens.space["2"]

        TextField {
            id: commandName
            objectName: "inputCommandNameField"
            placeholderText: qsTr("Name")
            Layout.preferredWidth: 160
            Accessible.name: qsTr("Command shortcut name")
        }
        TextField {
            id: commandLine
            objectName: "inputCommandLineField"
            placeholderText: qsTr("Command to run")
            Layout.preferredWidth: 260
            Accessible.name: qsTr("Command to run")
        }
        ShortcutCaptureButton {
            id: commandCapture
            objectName: "inputCommandCapture"
            placeholderText: qsTr("Set shortcut")
            formatter: key => shortcuts.displayKey(key)
        }
        Button {
            objectName: "inputCommandAddButton"
            emphasized: true
            text: qsTr("Add")
            enabled: commandName.text.trimmed().length > 0
                     && commandLine.text.trimmed().length > 0
            onClicked: {
                if (shortcuts.addCommand(commandName.text,
                                         commandLine.text,
                                         commandCapture.sequence !== 0
                                             ? [commandCapture.sequence]
                                             : [])) {
                    commandName.text = ""
                    commandLine.text = ""
                    commandCapture.sequence = 0
                }
            }
        }
    }

    Item { Layout.fillHeight: true }
}
