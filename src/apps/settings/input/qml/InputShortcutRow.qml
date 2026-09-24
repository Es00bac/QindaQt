// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0
import QindaTK as Tk

// One global shortcut row: the action and component names, the active keys
// drawn as key caps, reset, clear, remove (command shortcuts only), capture,
// and the explicit decision a captured key needs when another action already
// holds it (ADR-0134).
//
// AGENT-GUARD: Tk.KeyCap arrived in dev-libs/qindatk-0.1.0-r5. Against r4 or
// older this file fails to load ("KeyCap is not a type") and the whole
// Shortcuts destination is lost, so the package must depend on r5.
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

    // AGENT-CONTRACT: `keys` is ShortcutsModel's display string: each binding
    // is one single-chord QKeySequence in NativeText ("Ctrl+Shift+K"), and
    // bindings are joined with ", " (shortcuts_model.cpp keysDisplay). The
    // port builds every sequence from one combined key, so ", " never
    // separates chords here. A comma key ("Ctrl+,") is still safe: its
    // own comma is not followed by a space.
    readonly property var bindings: keys.length > 0 ? keys.split(", ") : []
    readonly property string spokenKeys:
        bindings.map(binding => capSequence(binding)).join(qsTr(" or "))

    // KeyCap splits its sequence on "+", so the Plus key itself ("Ctrl++",
    // or a bare "+") is named instead of producing empty caps.
    function capSequence(binding) {
        if (binding === "+")
            return qsTr("Plus")
        if (binding.endsWith("++"))
            return binding.slice(0, -1) + qsTr("Plus")
        return binding
    }

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
            // The keys cell: the capture prompt, "Disabled", or one KeyCap
            // group per binding separated by a muted "or". `text` and the
            // accessible name spell the same state in words.
            Flow {
                id: keysCell
                objectName: "inputShortcutKeys_" + row.index
                readonly property bool showCaps: !row.capturing
                                                 && row.bindings.length > 0
                readonly property string text: row.capturing
                      ? qsTr("Press keys — Esc cancels, Backspace clears")
                      : showCaps ? row.spokenKeys : qsTr("Disabled")
                Layout.fillWidth: true
                spacing: Tokens.space["2"]
                Accessible.role: Accessible.StaticText
                Accessible.name: showCaps
                                 ? qsTr("Shortcut: %1").arg(row.spokenKeys)
                                 : text

                Label {
                    objectName: "inputShortcutKeysText_" + row.index
                    visible: !keysCell.showCaps
                    text: keysCell.text
                    muted: !row.capturing
                }
                Repeater {
                    model: keysCell.showCaps ? row.bindings : []
                    delegate: Row {
                        id: binding
                        required property string modelData
                        required property int index
                        spacing: Tokens.space["2"]
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: binding.index > 0
                            text: qsTr("or")
                            muted: true
                        }
                        Tk.KeyCap {
                            objectName: "inputShortcutKeyCap_" + row.index
                                        + "_" + binding.index
                            anchors.verticalCenter: parent.verticalCenter
                            sequence: row.capSequence(binding.modelData)
                        }
                    }
                }
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
