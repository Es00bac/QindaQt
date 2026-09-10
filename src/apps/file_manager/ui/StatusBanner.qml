// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Banner replacing the former QindaQt.Controls StateCard (ADR-0116): stock
// controls and palette roles only. One accessible alert surface with a title,
// a wrapped message, and an optional trailing action.
Control {
    id: root

    property string title: ""
    property string message: ""
    property string actionText: ""
    signal actionTriggered()

    padding: 6
    Accessible.role: Accessible.AlertMessage
    Accessible.name: root.title + (root.message.length > 0 ? " — " + root.message : "")

    background: Rectangle {
        radius: 4
        color: root.palette.alternateBase
        border.color: root.palette.mid
    }

    contentItem: RowLayout {
        spacing: 8

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Label {
                Layout.fillWidth: true
                visible: root.title.length > 0
                text: root.title
                font.bold: true
                wrapMode: Text.WordWrap
                Accessible.ignored: true
            }
            Label {
                Layout.fillWidth: true
                visible: root.message.length > 0
                text: root.message
                wrapMode: Text.WordWrap
                Accessible.ignored: true
            }
        }
        Button {
            visible: root.actionText.length > 0
            text: root.actionText
            onClicked: root.actionTriggered()
        }
    }
}
