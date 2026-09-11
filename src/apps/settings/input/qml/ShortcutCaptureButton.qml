// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// One key-capture control. Idle, it reads as a button showing the current
// sequence; clicked, it captures the next chord.
//
// AGENT-CONTRACT: Escape cancels and Backspace clears (product rule for
// every capture surface). A capture commits only a chord with at least one
// non-modifier key, encoded exactly as Qt::Modifier | Qt::Key so kglobalaccel
// and conflict lookups see one encoding end to end (ADR-0134). Set
// `formatter` to a callable for native display text (the shortcuts model
// exposes one); the built-in fallback is for isolated fixtures only.
T.Button {
    id: root

    property int sequence: 0
    property var formatter: null
    property string captureHint: qsTr("Press keys")
    property string placeholderText: qsTr("Not set")

    signal captured(int sequence)
    signal cleared()

    // True while the next key press belongs to this control. Focus loss
    // cancels, so Tab away is always a safe exit.
    readonly property bool capturing: root.captureMode && root.activeFocus
    property bool captureMode: false

    function beginCapture() {
        root.captureMode = true
        root.forceActiveFocus(Qt.TabFocusReason)
    }

    function cancelCapture() {
        root.captureMode = false
    }

    text: root.capturing ? captureHint
                         : root.sequence !== 0 ? sequenceText(root.sequence)
                                               : placeholderText

    onClicked: root.beginCapture()
    onActiveFocusChanged: if (!root.activeFocus) root.captureMode = false

    function sequenceText(sequence) {
        if (root.formatter !== null) {
            return root.formatter(sequence)
        }
        return builtinDisplay(sequence)
    }

    function builtinDisplay(sequence) {
        const mods = sequence & ~0x00ffffff
        const key = sequence & 0x00ffffff
        let text = ""
        if (mods & Qt.ControlModifier) text += "Ctrl+"
        if (mods & Qt.AltModifier) text += "Alt+"
        if (mods & Qt.ShiftModifier) text += "Shift+"
        if (mods & Qt.MetaModifier) text += "Meta+"
        text += builtinKeyName(key)
        return text
    }

    function builtinKeyName(key) {
        const special = {
            0x01000000: "Escape", 0x01000003: "Backspace", 0x01000004: "Return",
            0x01000006: "Ins", 0x01000007: "Del", 0x01000010: "Home",
            0x01000011: "End", 0x01000012: "Left", 0x01000013: "Up",
            0x01000014: "Right", 0x01000015: "Down", 0x01000016: "Page Up",
            0x01000017: "Page Down",
        }
        if (special[key] !== undefined) return special[key]
        if (key >= 0x01000030 && key <= 0x01000039)
            return "F" + (key - 0x01000030 + 1)
        if (key >= 0x20 && key <= 0x7e)
            return String.fromCharCode(key).toUpperCase()
        return "0x" + key.toString(16)
    }

    background: Rectangle {
        radius: Tokens.radius.s
        color: root.capturing ? Tokens.bg.highest : Tokens.bg.raised
        border.width: 1
        border.color: root.activeFocus ? Tokens.focus.ring
                                       : Tokens.outline.strong
    }

    contentItem: Label {
        text: root.text
        horizontalAlignment: Text.AlignHCenter
        muted: !root.capturing && root.sequence === 0
    }

    Keys.priority: Keys.BeforeItem
    Keys.onPressed: event => {
        if (!root.capturing) {
            event.accepted = false
            return
        }
        event.accepted = true
        const key = int(event.key)
        const mods = int(event.modifiers)
        if (key === Qt.Key_Escape && mods === 0) {
            root.captureMode = false
            return
        }
        if (key === Qt.Key_Backspace && mods === 0) {
            root.captureMode = false
            if (root.sequence !== 0) {
                root.sequence = 0
                root.cleared()
            }
            return
        }
        const modifierOnly =
            key === Qt.Key_Control || key === Qt.Key_Shift
            || key === Qt.Key_Alt || key === Qt.Key_Meta
            || key === Qt.Key_AltGr
        if (modifierOnly || key === 0) {
            return
        }
        if (key === Qt.Key_Tab && mods === 0) {
            // Keep standard focus navigation out of a capture.
            root.captureMode = false
            event.accepted = false
            return
        }
        root.captureMode = false
        root.sequence = key | mods
        root.captured(root.sequence)
    }
}
