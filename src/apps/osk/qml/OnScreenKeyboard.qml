// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaQt.Tokens 1.0

// The keyboard panel. Its width is the output's width (the compositor parks
// an input panel along the bottom edge); its height follows the rows.
Item {
    id: root

    objectName: "onScreenKeyboard"

    readonly property real keyHeight: Math.max(48, Tokens.space["4"] * 3)
    readonly property real gap: Tokens.space["2"]
    readonly property real margin: Tokens.space["3"]

    width: Screen.width
    implicitHeight: keyboard.panelHeight
    height: implicitHeight

    function relayout() {
        keyboard.relayout(width, keyHeight, gap, margin)
    }

    onWidthChanged: relayout()
    onKeyHeightChanged: relayout()
    Component.onCompleted: relayout()

    Rectangle {
        anchors.fill: parent
        color: Tokens.bg.raised
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 1
            color: Tokens.outline.strong
        }
    }

    Repeater {
        model: keyboard.rows
        delegate: Item {
            id: row
            required property int index
            required property var modelData
            anchors.fill: parent
            Repeater {
                model: row.modelData
                delegate: KeyCap {
                    required property int index
                    required property var modelData
                    rowIndex: row.index
                    keyIndex: index
                    kind: modelData.kind
                    label: modelData.label
                    active: modelData.active
                    x: modelData.x
                    y: modelData.y
                    width: modelData.width
                    height: modelData.height
                    onPressedKey: (r, k) => keyboard.press(r, k)
                }
            }
        }
    }
}
