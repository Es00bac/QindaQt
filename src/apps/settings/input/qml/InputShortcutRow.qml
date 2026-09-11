// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// One global shortcut row: the action and component names, the active keys,
// reset, clear, remove (command shortcuts only), capture, and the explicit
// decision a captured key needs when another action already holds it
// (ADR-0134).
Rectangle {
    id: row

    required property var shortcuts
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

    function conflictText(conflicts) {
        return qsTr("Already used by %1").arg(conflicts.join(", "))
    }

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
                onClicked: row.shortcuts.resetToDefault(row.index)
            }
            Button {
                objectName: "inputShortcutClear_" + row.index
                text: qsTr("Clear")
                enabled: !row.capturing && row.keys.length > 0
                onClicked: row.shortcuts.clear(row.index)
            }
            Button {
                objectName: "inputShortcutRemove_" + row.index
                text: qsTr("Remove")
                visible: row.isCommand
                onClicked: row.shortcuts.removeCommand(row.index)
            }
            ShortcutCaptureButton {
                objectName: "inputShortcutCapture_" + row.index
                sequence: 0
                captureHint: qsTr("Capturing…")
                placeholderText: qsTr("Change")
                formatter: key => row.shortcuts.displayKey(key)
                enabled: !row.shortcuts.busy
                onCaptured: sequence => {
                    row.pendingKeys = [sequence]
                    const conflicts = row.shortcuts.conflictsFor(
                        row.index, row.pendingKeys)
                    if (conflicts.length === 0) {
                        row.shortcuts.assign(row.index, row.pendingKeys)
                        row.pendingKeys = null
                    }
                }
            }
        }

        RowLayout {
            visible: row.capturing
                && row.shortcuts.conflictsFor(row.index,
                                              row.pendingKeys).length > 0
            Layout.fillWidth: true
            spacing: Tokens.space["2"]
            Label {
                objectName: "inputShortcutConflict_" + row.index
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: row.capturing
                      ? row.conflictText(row.shortcuts.conflictsFor(
                                              row.index, row.pendingKeys))
                      : ""
                Accessible.role: Accessible.AlertMessage
                Accessible.name: text
            }
            Button {
                objectName: "inputShortcutConflictAssign_" + row.index
                text: qsTr("Assign anyway")
                onClicked: {
                    row.shortcuts.assign(row.index, row.pendingKeys)
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
