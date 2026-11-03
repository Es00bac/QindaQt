// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk
import QindaTK.QindaQt
import QindaQt.Link

// The compact reply form a notification's Reply action opens (`qindaqt-link --reply -- <device> <key>
// <action>`). Closes itself when the phone confirms; shows the error otherwise.
Tk.AppWindow {
    id: window

    width: 400
    height: 190
    visible: true
    title: Reply.appName.length > 0 ? qsTr("Reply to %1").arg(Reply.appName) : qsTr("Reply")

    QindaQtTheme {}

    Connections {
        target: Reply
        function onFinished() { Qt.quit() }
    }

    Tk.Flex {
        anchors.fill: parent
        direction: Tk.Flex.Column
        padding: Tk.Theme.space.md
        gap: Tk.Theme.space.sm

        Tk.TextArea {
            id: field
            objectName: "replyField"
            rows: 3
            placeholderText: qsTr("Your reply")
            enabled: Reply.state !== "sending"
            Tk.Flex.grow: 1
            Tk.Flex.basis: 0
            Component.onCompleted: field.forceActiveFocus()
        }
        Tk.Notice {
            objectName: "replyError"
            visible: Reply.state === "error"
            variant: "danger"
            text: Reply.error
        }
        Tk.Flex {
            gap: Tk.Theme.space.xs
            justify: Tk.Flex.End
            Tk.Button {
                text: qsTr("Cancel")
                onClicked: Qt.quit()
            }
            Tk.Button {
                objectName: "replySend"
                text: qsTr("Send")
                iconName: "send"
                variant: "accent"
                busy: Reply.state === "sending"
                enabled: field.text.trim().length > 0 && Reply.state !== "sending"
                onClicked: Reply.send(field.text)
            }
        }
    }
}
